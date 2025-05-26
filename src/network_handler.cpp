#include "network_handler.h"
#include "config.h" // For global variables, server, webSocket objects
#include "nvs_handler.h" // For saveFanCurveToNVS
#include <SPIFFS.h>


void broadcastWebSocketData() {
    if (!isWiFiEnabled || WiFi.status() != WL_CONNECTED) return;
    
    ArduinoJson::JsonDocument jsonDoc; 

    jsonDoc["temperature"] = tempSensorFound ? currentTemperature : -999.0; 
    jsonDoc["tempSensorFound"] = tempSensorFound; 
    jsonDoc["fanSpeed"] = fanSpeedPercentage;
    jsonDoc["isAutoMode"] = isAutoMode;
    jsonDoc["manualFanSpeed"] = manualFanSpeedPercentage;
    jsonDoc["fanRpm"] = fanRpm;
    jsonDoc["isWiFiEnabled"] = isWiFiEnabled; 
    jsonDoc["serialDebugEnabled"] = serialDebugEnabled; 

    ArduinoJson::JsonArray curveArray = jsonDoc["fanCurve"].to<ArduinoJson::JsonArray>();
    for (int i = 0; i < numCurvePoints; i++) {
        ArduinoJson::JsonObject point = curveArray.add<ArduinoJson::JsonObject>();
        point["temp"] = tempPoints[i];
        point["pwmPercent"] = pwmPercentagePoints[i];
    }
    String jsonString;
    serializeJson(jsonDoc, jsonString);
    webSocket.broadcastTXT(jsonString);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    if (!isWiFiEnabled) return; 

    switch(type) {
        case WStype_DISCONNECTED:
            if(serialDebugEnabled) Serial.printf("[WS][%u] Client Disconnected!\n", num);
            break;
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            if(serialDebugEnabled) Serial.printf("[WS][%u] Client Connected from %s, URL: %s\n", num, ip.toString().c_str(), payload);
            needsImmediateBroadcast = true;
            break;
        }
        case WStype_TEXT: {
            if(serialDebugEnabled) Serial.printf("[WS][%u] Received Text: %s\n", num, payload);
            ArduinoJson::JsonDocument doc; 
            
            DeserializationError error = deserializeJson(doc, payload, length);

            if (error) {
                if(serialDebugEnabled) Serial.print(F("[WS] deserializeJson() failed: "));
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
                        if(serialDebugEnabled) Serial.println("[WS] Ignored setManualSpeed, not in manual mode.");
                    }
                } else if (strcmp(action, "setCurve") == 0) {
                    if (!tempSensorFound) {
                        if(serialDebugEnabled) Serial.println("[WS] Ignored setCurve, temperature sensor not found.");
                        break; 
                    }
                    ArduinoJson::JsonArray newCurve = doc["curve"];
                    if (newCurve) { 
                        if(serialDebugEnabled) Serial.println("[WS] Received new fan curve data.");
                        int newNumPoints = newCurve.size();
                        if (newNumPoints >= 2 && newNumPoints <= MAX_CURVE_POINTS) {
                            bool curveValid = true;
                            int lastTemp = -100; 
                            int tempTempPointsValidation[MAX_CURVE_POINTS];
                            int tempPwmPercentagePointsValidation[MAX_CURVE_POINTS]; 

                            for(int i=0; i < newNumPoints; ++i) {
                                ArduinoJson::JsonObject point = newCurve[i];
                                int t = point["temp"];
                                int p = point["pwmPercent"];

                                if (t < 0 || t > 120 || p < 0 || p > 100 || (i > 0 && t <= lastTemp) ) {
                                    curveValid = false;
                                    if(serialDebugEnabled) Serial.printf("[WS] Invalid curve point %d received: Temp=%d, PWM=%d\n", i, t, p);
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
                                if(serialDebugEnabled) Serial.println("[SYSTEM] New fan curve from web rejected due to invalid data.");
                            }
                        } else {
                             if(serialDebugEnabled) Serial.printf("[SYSTEM] Invalid number of curve points (%d) from web. Must be 2-%d.\n", newNumPoints, MAX_CURVE_POINTS);
                        }
                    } else {
                        if(serialDebugEnabled) Serial.println("[WS] 'setCurve' action received, but 'curve' array missing or invalid.");
                    }
                } else {
                    if(serialDebugEnabled) Serial.printf("[WS] Unknown action received: %s\n", action);
                }
            } else {
                 if(serialDebugEnabled) Serial.println("[WS] Received text, but no 'action' field found in JSON.");
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
}
