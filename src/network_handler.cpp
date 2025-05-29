#include "network_handler.h"
#include "config.h"      
#include "nvs_handler.h" 
#include <SPIFFS.h>
#include <ArduinoJson.h> 
#include "ota_updater.h" // For triggerOTAUpdateCheck

void broadcastWebSocketData() {
    if (!isWiFiEnabled || WiFi.status() != WL_CONNECTED) return;
    
    ArduinoJson::JsonDocument jsonDoc; 

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
    jsonDoc["firmwareVersion"] = FIRMWARE_VERSION; // Send current firmware version

    // OTA Status
    jsonDoc["otaInProgress"] = ota_in_progress;
    jsonDoc["otaStatusMessage"] = ota_status_message;


    ArduinoJson::JsonArray curveArray = jsonDoc["fanCurve"].to<ArduinoJson::JsonArray>();
    for (int i = 0; i < numCurvePoints; i++) {
        ArduinoJson::JsonObject point = curveArray.add<ArduinoJson::JsonObject>();
        point["temp"] = tempPoints[i];
        point["pwmPercent"] = pwmPercentagePoints[i];
    }

    jsonDoc["isMqttEnabled"] = isMqttEnabled;
    jsonDoc["mqttServer"] = mqttServer;
    jsonDoc["mqttPort"] = mqttPort;
    jsonDoc["mqttUser"] = mqttUser;
    jsonDoc["mqttBaseTopic"] = mqttBaseTopic;
    jsonDoc["isMqttDiscoveryEnabled"] = isMqttDiscoveryEnabled;
    jsonDoc["mqttDiscoveryPrefix"] = mqttDiscoveryPrefix;


    String jsonString;
    serializeJson(jsonDoc, jsonString);
    webSocket.broadcastTXT(jsonString);
    if (serialDebugEnabled && millis() % 60000 < 100) { 
        // Avoid printing very long JSON strings too often if they become large
        if (jsonString.length() < 256) {
             Serial.print("[WS_BCAST] "); Serial.println(jsonString);
        } else {
             Serial.print("[WS_BCAST] Sent (length: "); Serial.print(jsonString.length()); Serial.println(")");
        }
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

                        for(int k=0; k < newNumPoints; ++k) { // Changed i to k
                            ArduinoJson::JsonObject point = newCurve[k];
                            if (!point["temp"].is<int>() || !point["pwmPercent"].is<int>()) {
                                curveValid = false;
                                if(serialDebugEnabled) Serial.printf("[WS_ERR] Curve point %d missing temp or pwmPercent, or wrong type.\n", k);
                                break;
                            }
                            int t = point["temp"];
                            int p = point["pwmPercent"];

                            if (t < 0 || t > 120 || p < 0 || p > 100 || (k > 0 && t <= lastTemp) ) {
                                curveValid = false;
                                if(serialDebugEnabled) Serial.printf("[WS_ERR] Invalid curve point %d received: Temp=%d, PWM=%d. LastTemp=%d\n", k, t, p, lastTemp);
                                break;
                            }
                            tempTempPointsValidation[k] = t;
                            tempPwmPercentagePointsValidation[k] = p; 
                            lastTemp = t;
                        }

                        if (curveValid) {
                            numCurvePoints = newNumPoints;
                            for(int k=0; k < numCurvePoints; ++k) { // Changed i to k
                                tempPoints[k] = tempTempPointsValidation[k];
                                pwmPercentagePoints[k] = tempPwmPercentagePointsValidation[k]; 
                            }
                            if(serialDebugEnabled) Serial.println("[SYSTEM] Fan curve updated and validated via WebSocket.");
                            saveFanCurveToNVS(); 
                            fanCurveChanged = true; // Signal MQTT to publish new curve
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

                    if (doc["isMqttDiscoveryEnabled"].is<bool>()) {
                        bool newDiscoveryEnabled = doc["isMqttDiscoveryEnabled"];
                        if (isMqttDiscoveryEnabled != newDiscoveryEnabled) {
                            isMqttDiscoveryEnabled = newDiscoveryEnabled;
                            changed = true;
                        }
                    }

                    if (doc["mqttDiscoveryPrefix"].is<const char*>()) { 
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
                else if (strcmp(action, "triggerOtaUpdate") == 0) { 
                    if (serialDebugEnabled) Serial.println("[WS] Received OTA Update trigger.");
                    if (ota_in_progress) {
                        ota_status_message = "OTA already in progress.";
                        needsImmediateBroadcast = true;
                         if(serialDebugEnabled) Serial.println("[WS_OTA] " + ota_status_message);
                    } else if (!isWiFiEnabled || WiFi.status() != WL_CONNECTED) {
                        ota_status_message = "Error: WiFi not connected for OTA.";
                        needsImmediateBroadcast = true;
                        if(serialDebugEnabled) Serial.println("[WS_OTA] " + ota_status_message);
                    } else {
                        // This function call is blocking. It will run in the networkTask context.
                        // UI updates for ota_status_message will happen via broadcastWebSocketData
                        // which is called by needsImmediateBroadcast = true; within ota_updater functions.
                        triggerOTAUpdateCheck();
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
