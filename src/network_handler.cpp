#include "network_handler.h"
#include "config.h"      // For global variables, server, webSocket objects
#include "nvs_handler.h" // For saveFanCurveToNVS, saveMqttConfig, saveMqttDiscoveryConfig
#include <SPIFFS.h>
#include <ArduinoJson.h> // Ensure it's included for both send and receive

void broadcastWebSocketData() {
    if (!isWiFiEnabled || WiFi.status() != WL_CONNECTED) return;
    
    ArduinoJson::JsonDocument jsonDoc; 

    // Corrected: Use if/else for assigning temperature or null
    if (tempSensorFound) {
        jsonDoc["temperature"] = currentTemperature;
    } else {
        jsonDoc["temperature"] = nullptr; 
    }
    jsonDoc["tempSensorFound"] = tempSensorFound; 
    jsonDoc["fanSpeed"] = fanSpeedPercentage;
    jsonDoc["isAutoMode"] = isAutoMode;
    jsonDoc["manualFanSpeed"] = manualFanSpeedPercentage;
    jsonDoc["fanRpm"] = fanRpm;
    jsonDoc["isWiFiEnabled"] = isWiFiEnabled; 
    jsonDoc["serialDebugEnabled"] = serialDebugEnabled; 

    // Add Fan Curve data
    ArduinoJson::JsonArray curveArray = jsonDoc["fanCurve"].to<ArduinoJson::JsonArray>();
    for (int i = 0; i < numCurvePoints; i++) {
        ArduinoJson::JsonObject point = curveArray.add<ArduinoJson::JsonObject>();
        point["temp"] = tempPoints[i];
        point["pwmPercent"] = pwmPercentagePoints[i];
    }

    // Add MQTT Configuration Data (excluding password)
    jsonDoc["isMqttEnabled"] = isMqttEnabled;
    jsonDoc["mqttServer"] = mqttServer;
    jsonDoc["mqttPort"] = mqttPort;
    jsonDoc["mqttUser"] = mqttUser;
    // DO NOT send mqttPassword back to the client
    jsonDoc["mqttBaseTopic"] = mqttBaseTopic;

    // ADDED: MQTT Discovery Configuration Data
    jsonDoc["isMqttDiscoveryEnabled"] = isMqttDiscoveryEnabled;
    jsonDoc["mqttDiscoveryPrefix"] = mqttDiscoveryPrefix;


    String jsonString;
    serializeJson(jsonDoc, jsonString);
    webSocket.broadcastTXT(jsonString);
    if (serialDebugEnabled && millis() % 60000 < 100) { 
        Serial.print("[WS_BCAST] "); Serial.println(jsonString);
    }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    if (!isWiFiEnabled) return; 

    switch(type) {
        case WStype_DISCONNECTED:
            if(serialDebugEnabled) Serial.printf("[WS][%u] Client Disconnected!\n", num);
            break;
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            if(serialDebugEnabled) Serial.printf("[WS][%u] Client Connected from %s, URL: %s\n", num, ip.toString().c_str(), (char *)payload);
            needsImmediateBroadcast = true; 
            break;
        }
        case WStype_TEXT: {
            if(serialDebugEnabled) Serial.printf("[WS][%u] Received Text: %s\n", num, (char *)payload);
            ArduinoJson::JsonDocument doc; 
            
            DeserializationError error = deserializeJson(doc, payload, length);

            if (error) {
                if(serialDebugEnabled) Serial.print(F("[WS_ERR] deserializeJson() failed: "));
                if(serialDebugEnabled) Serial.println(error.f_str());
                return;
            }
            const char* action = doc["action"];
            if (action) {
                if(serialDebugEnabled) Serial.printf("[WS] Action received: %s\n", action);
                if (strcmp(action, "setModeAuto") == 0) {
                    isAutoMode = true;
                    needsImmediateBroadcast = true; 
                    if(serialDebugEnabled) Serial.println("[SYSTEM] Mode changed to AUTO via WebSocket.");
                } else if (strcmp(action, "setModeManual") == 0) {
                    isAutoMode = false;
                    needsImmediateBroadcast = true; 
                    if(serialDebugEnabled) Serial.println("[SYSTEM] Mode changed to MANUAL via WebSocket.");
                } else if (strcmp(action, "setManualSpeed") == 0) {
                    if (!isAutoMode) { 
                        manualFanSpeedPercentage = doc["value"]; 
                        needsImmediateBroadcast = true; 
                        if(serialDebugEnabled) Serial.printf("[SYSTEM] Manual speed target set to %d%% via WebSocket.\n", manualFanSpeedPercentage);
                    } else {
                        if(serialDebugEnabled) Serial.println("[WS_WARN] Ignored setManualSpeed, not in manual mode.");
                    }
                } else if (strcmp(action, "setCurve") == 0) {
                    if (!tempSensorFound) {
                        if(serialDebugEnabled) Serial.println("[WS_WARN] Ignored setCurve, temperature sensor not found.");
                        break; 
                    }
                    ArduinoJson::JsonArray newCurve = doc["curve"];
                    if (newCurve && newCurve.size() >= 2 && newCurve.size() <= MAX_CURVE_POINTS) { 
                        if(serialDebugEnabled) Serial.println("[WS] Received new fan curve data.");
                        bool curveValid = true;
                        int lastTemp = -100; 
                        int tempTempPointsValidation[MAX_CURVE_POINTS];
                        int tempPwmPercentagePointsValidation[MAX_CURVE_POINTS]; 
                        int newNumPoints = newCurve.size();

                        for(int i=0; i < newNumPoints; ++i) {
                            ArduinoJson::JsonObject point = newCurve[i];
                            if (!point["temp"].is<int>() || !point["pwmPercent"].is<int>()) {
                                curveValid = false;
                                if(serialDebugEnabled) Serial.printf("[WS_ERR] Curve point %d missing temp or pwmPercent, or wrong type.\n", i);
                                break;
                            }
                            int t = point["temp"];
                            int p = point["pwmPercent"];

                            if (t < 0 || t > 120 || p < 0 || p > 100 || (i > 0 && t <= lastTemp) ) {
                                curveValid = false;
                                if(serialDebugEnabled) Serial.printf("[WS_ERR] Invalid curve point %d received: Temp=%d, PWM=%d. LastTemp=%d\n", i, t, p, lastTemp);
                                break;
                            }
                            tempTempPointsValidation[i] = t;
                            tempPwmPercentagePointsValidation[i] = p; 
                            lastTemp = t;
                        }

                        if (curveValid) {
                            numCurvePoints = newNumPoints;
                            for(int i=0; i < numCurvePoints; ++i) {
                                tempPoints[i] = tempTempPointsValidation[i];
                                pwmPercentagePoints[i] = tempPwmPercentagePointsValidation[i]; 
                            }
                            if(serialDebugEnabled) Serial.println("[SYSTEM] Fan curve updated and validated via WebSocket.");
                            saveFanCurveToNVS(); 
                            needsImmediateBroadcast = true; 
                        } else {
                            if(serialDebugEnabled) Serial.println("[SYSTEM_ERR] New fan curve from web rejected due to invalid data.");
                        }
                    } else {
                         if(serialDebugEnabled) Serial.printf("[WS_ERR] 'setCurve' action received, but 'curve' array missing, invalid, or wrong size (got %d points).\n", newCurve.size());
                    }
                } 
                else if (strcmp(action, "setMqttConfig") == 0) {
                    if (serialDebugEnabled) Serial.println("[WS] Received MQTT configuration update.");
                    
                    bool newMqttEnabled = doc["isMqttEnabled"] | isMqttEnabled; 
                    String newMqttServer = doc["mqttServer"] | String(mqttServer);
                    int newMqttPort = doc["mqttPort"] | mqttPort;
                    String newMqttUser = doc["mqttUser"] | String(mqttUser);
                    String newMqttPassword = doc["mqttPassword"]; 
                    String newMqttBaseTopic = doc["mqttBaseTopic"] | String(mqttBaseTopic);

                    bool changed = false;
                    if (isMqttEnabled != newMqttEnabled) { isMqttEnabled = newMqttEnabled; changed = true; }
                    
                    if (newMqttServer.length() < sizeof(mqttServer)) {
                        if (strcmp(mqttServer, newMqttServer.c_str()) != 0) {
                            strcpy(mqttServer, newMqttServer.c_str()); changed = true;
                        }
                    }
                    if (mqttPort != newMqttPort && newMqttPort > 0 && newMqttPort <= 65535) {
                        mqttPort = newMqttPort; changed = true;
                    }
                     if (newMqttUser.length() < sizeof(mqttUser)) {
                        if (strcmp(mqttUser, newMqttUser.c_str()) != 0) {
                            strcpy(mqttUser, newMqttUser.c_str()); changed = true;
                        }
                    }
                    if (doc["mqttPassword"].is<const char*>() && newMqttPassword.length() < sizeof(mqttPassword)) {
                         if (strcmp(mqttPassword, newMqttPassword.c_str()) != 0) {
                            strcpy(mqttPassword, newMqttPassword.c_str()); changed = true;
                         }
                    } else if (doc["mqttPassword"].is<String>() && newMqttPassword.length() < sizeof(mqttPassword) ) {
                         if (strcmp(mqttPassword, newMqttPassword.c_str()) != 0) {
                            strcpy(mqttPassword, newMqttPassword.c_str()); changed = true;
                         }
                    }

                     if (newMqttBaseTopic.length() > 0 && newMqttBaseTopic.length() < sizeof(mqttBaseTopic)) {
                         if (strcmp(mqttBaseTopic, newMqttBaseTopic.c_str()) != 0) {
                            strcpy(mqttBaseTopic, newMqttBaseTopic.c_str()); changed = true;
                         }
                    }

                    if (changed) {
                        if (serialDebugEnabled) Serial.println("[SYSTEM] MQTT configuration updated via WebSocket. Reboot needed.");
                        saveMqttConfig();
                        rebootNeeded = true; 
                        needsImmediateBroadcast = true; 
                    } else {
                        if (serialDebugEnabled) Serial.println("[WS] MQTT configuration received, but no changes detected.");
                    }
                }
                else if (strcmp(action, "setMqttDiscoveryConfig") == 0) {
                    if (serialDebugEnabled) Serial.println("[WS] Received MQTT Discovery configuration update.");
                    bool changed = false;

                    // Corrected: Use doc[key].is<T>() for checking existence and type
                    if (doc["isMqttDiscoveryEnabled"].is<bool>()) {
                        bool newDiscoveryEnabled = doc["isMqttDiscoveryEnabled"];
                        if (isMqttDiscoveryEnabled != newDiscoveryEnabled) {
                            isMqttDiscoveryEnabled = newDiscoveryEnabled;
                            changed = true;
                        }
                    }

                    // Corrected: Use doc[key].is<T>() for checking existence and type
                    if (doc["mqttDiscoveryPrefix"].is<const char*>()) { // Or .is<String>() if that's how it's sent
                        String newPrefix = doc["mqttDiscoveryPrefix"];
                        if (newPrefix.length() < sizeof(mqttDiscoveryPrefix)) {
                            if (strcmp(mqttDiscoveryPrefix, newPrefix.c_str()) != 0) {
                                strcpy(mqttDiscoveryPrefix, newPrefix.c_str());
                                changed = true;
                            }
                        } else if (serialDebugEnabled) {
                            Serial.printf("[WS_ERR] MQTT Discovery Prefix too long: %s\n", newPrefix.c_str());
                        }
                    }
                    
                    if (changed) {
                        if (serialDebugEnabled) Serial.println("[SYSTEM] MQTT Discovery configuration updated via WebSocket. Reboot needed.");
                        saveMqttDiscoveryConfig();
                        rebootNeeded = true;
                        needsImmediateBroadcast = true;
                    } else {
                         if (serialDebugEnabled) Serial.println("[WS] MQTT Discovery configuration received, but no changes detected.");
                    }
                }
                else {
                    if(serialDebugEnabled) Serial.printf("[WS_ERR] Unknown action received: %s\n", action);
                }
            } else {
                 if(serialDebugEnabled) Serial.println("[WS_ERR] Received text, but no 'action' field found in JSON.");
            }
            break;
        }
        default: 
            if(serialDebugEnabled) Serial.printf("[WS][%u] Received unhandled WStype: %d\n", num, type);
            break;
    }
}

void setupWebServerRoutes() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!SPIFFS.exists("/index.html")){
            if(serialDebugEnabled) Serial.println("SPIFFS: index.html not found!");
            request->send(404, "text/plain", "index.html not found. Make sure to upload SPIFFS data.");
            return;
        }
        request->send(SPIFFS, "/index.html", "text/html");
    });
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!SPIFFS.exists("/style.css")){
            if(serialDebugEnabled) Serial.println("SPIFFS: style.css not found!");
            request->send(404, "text/plain", "style.css not found.");
            return;
        }
        request->send(SPIFFS, "/style.css", "text/css");
    });
    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
            if(!SPIFFS.exists("/script.js")){
            if(serialDebugEnabled) Serial.println("SPIFFS: script.js not found!");
            request->send(404, "text/plain", "script.js not found.");
            return;
        }
        request->send(SPIFFS, "/script.js", "application/javascript");
    });
    
    server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request){
        if(serialDebugEnabled) Serial.println("[HTTP] Reboot requested via /reboot endpoint.");
        request->send(200, "text/plain", "Rebooting device...");
        delay(1000); 
        ESP.restart();
    });
}
