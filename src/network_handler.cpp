#include "network_handler.h"
#include "config.h"      
#include "nvs_handler.h" 
#include <SPIFFS.h>
#include <ArduinoJson.h> 
#include "ota_updater.h" // For triggerOTAUpdateCheck
#include <AsyncJson.h>   // For API JSON body parsing

void broadcastWebSocketData() {
    if (!isWiFiEnabled || WiFi.status() != WL_CONNECTED) return;
    
    ArduinoJson::JsonDocument jsonDoc; 

    if (tempSensorFound) {
        jsonDoc["temperature"] = currentTemperature;
    } else {
        jsonDoc["temperature"] = nullptr; 
    }
    jsonDoc["tempSensorFound"] = tempSensorFound; 
    
    // Add PC Temp data
    jsonDoc["pcTempDataReceived"] = pcTempDataReceived;
    if (pcTempDataReceived) {
        jsonDoc["pcTemperature"] = pcTemperature;
    } else {
        jsonDoc["pcTemperature"] = nullptr;
    }

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
                    if (!tempSensorFound && !pcTempDataReceived) {
                        if(serialDebugEnabled) Serial.println("[WS_WARN] Ignored setCurve, no temperature source available.");
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
                            JsonObject point = newCurve[k];
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
    
    // FIX: Add routes to serve the local chart libraries
    server.on("/chart.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!SPIFFS.exists("/chart.min.js")){
            if(serialDebugEnabled) Serial.println("SPIFFS: chart.min.js not found!");
            request->send(404, "text/plain", "chart.min.js not found.");
            return;
        }
        request->send(SPIFFS, "/chart.min.js", "application/javascript");
    });

    server.on("/chartjs-plugin-dragdata.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!SPIFFS.exists("/chartjs-plugin-dragdata.min.js")){
            if(serialDebugEnabled) Serial.println("SPIFFS: chartjs-plugin-dragdata.min.js not found!");
            request->send(404, "text/plain", "chartjs-plugin-dragdata.min.js not found.");
            return;
        }
        request->send(SPIFFS, "/chartjs-plugin-dragdata.min.js", "application/javascript");
    });


    server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request){
        if(serialDebugEnabled) Serial.println("[HTTP] Reboot requested via /reboot endpoint.");
        request->send(200, "text/plain", "Rebooting device...");
        delay(1000); 
        ESP.restart();
    });

    // Catch-all for non-API routes. This should be the last web server route defined.
    server.onNotFound([](AsyncWebServerRequest *request) {
        if (request->url().startsWith("/api/")) {
            // This case should be handled by the API catch-all, but as a fallback:
            request->send(404, "application/json", "{\"error\":\"API route not found.\"}");
        } else {
            request->send(404, "text/plain", "Not found");
        }
    });
}

// --- REST API Route Setup ---
void setupApiRoutes() {
    // Helper lambda to send a JSON response
    auto sendJsonResponse = [](AsyncWebServerRequest *request, JsonDocument &doc, int code = 200) {
        String response;
        serializeJson(doc, response);
        request->send(code, "application/json", response);
    };

    // --- GET Endpoints ---

    server.on("/api/status", HTTP_GET, [sendJsonResponse](AsyncWebServerRequest *request){
        ArduinoJson::JsonDocument doc;
        doc["firmwareVersion"] = FIRMWARE_VERSION;
        doc["isAutoMode"] = isAutoMode;
        // FIX: Copy volatile variables to local variables before use in JSON
        int currentFanSpeedPercentage = fanSpeedPercentage;
        int currentFanRpm = fanRpm;
        int currentManualFanSpeedPercentage = manualFanSpeedPercentage;
        float tempOnboard = currentTemperature;
        float tempPc = pcTemperature;

        doc["fanSpeedPercent"] = currentFanSpeedPercentage;
        doc["fanRpm"] = currentFanRpm;
        doc["manualSetSpeed"] = currentManualFanSpeedPercentage;

        // Temperature status
        doc["tempSensorFound"] = tempSensorFound;
        if (tempSensorFound && tempOnboard > -990.0) {
            doc["onboardTemperature"] = tempOnboard;
        } else {
            doc["onboardTemperature"] = nullptr;
        }
        
        doc["pcTempDataReceived"] = pcTempDataReceived;
        if (pcTempDataReceived && tempPc > -990.0) {
            doc["pcTemperature"] = tempPc;
        } else {
            doc["pcTemperature"] = nullptr;
        }
        
        // Network status
        doc["isWiFiEnabled"] = isWiFiEnabled;
        doc["wifiConnected"] = (WiFi.status() == WL_CONNECTED);
        if (WiFi.status() == WL_CONNECTED) {
            doc["ipAddress"] = WiFi.localIP().toString();
            doc["wifiRssi"] = WiFi.RSSI();
        }
        
        // MQTT status
        doc["isMqttEnabled"] = isMqttEnabled;
        doc["isMqttConnected"] = mqttClient.connected();
        doc["isMqttDiscoveryEnabled"] = isMqttDiscoveryEnabled;

        // System status
        doc["rebootNeeded"] = rebootNeeded;
        doc["otaInProgress"] = ota_in_progress;
        doc["otaStatusMessage"] = ota_status_message;

        sendJsonResponse(request, doc);
    });

    server.on("/api/config/curve", HTTP_GET, [sendJsonResponse](AsyncWebServerRequest *request){
        ArduinoJson::JsonDocument doc;
        JsonArray curveArray = doc.to<JsonArray>();
        for (int i = 0; i < numCurvePoints; i++) {
            JsonObject point = curveArray.add<JsonObject>();
            point["temp"] = tempPoints[i];
            point["pwmPercent"] = pwmPercentagePoints[i];
        }
        sendJsonResponse(request, doc);
    });

    server.on("/api/config/all", HTTP_GET, [sendJsonResponse](AsyncWebServerRequest *request){
        ArduinoJson::JsonDocument doc;
        // FIX: Use modern syntax for creating nested objects
        JsonObject wifiObj = doc["wifi"].to<JsonObject>();
        wifiObj["ssid"] = current_ssid;

        JsonObject mqttObj = doc["mqtt"].to<JsonObject>();
        mqttObj["enabled"] = isMqttEnabled;
        mqttObj["server"] = mqttServer;
        mqttObj["port"] = mqttPort;
        mqttObj["user"] = mqttUser;
        mqttObj["baseTopic"] = mqttBaseTopic;
        
        JsonObject discoveryObj = mqttObj["discovery"].to<JsonObject>();
        discoveryObj["enabled"] = isMqttDiscoveryEnabled;
        discoveryObj["prefix"] = mqttDiscoveryPrefix;

        sendJsonResponse(request, doc);
    });
    
    // --- POST/Body Endpoints ---
    
    // Handler for PC temperature updates from test client
    AsyncCallbackJsonWebHandler* pcStatsHandler = new AsyncCallbackJsonWebHandler("/api/pc_stats", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject jsonObj = json.as<JsonObject>();
        // FIX: Use modern syntax for checking key existence and type
        if (jsonObj["cpu_temp"].is<float>() || jsonObj["cpu_temp"].is<int>()) {
            pcTemperature = jsonObj["cpu_temp"].as<float>();
            pcTempDataReceived = true;
            lastPcTempDataTime = millis();
            needsImmediateBroadcast = true;
            displayUpdateNeeded = true;
            request->send(200, "application/json", "{\"status\":\"ok\"}");
             if(serialDebugEnabled) Serial.printf("[API] Received PC Temp: %.1f C\n", (float)pcTemperature);
        } else {
            request->send(400, "application/json", "{\"error\":\"Invalid or missing 'cpu_temp' numeric value\"}");
        }
    });
    server.addHandler(pcStatsHandler);

    // Handler for mode control
    AsyncCallbackJsonWebHandler* modeHandler = new AsyncCallbackJsonWebHandler("/api/control/mode", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject jsonObj = json.as<JsonObject>();
        // FIX: Use modern syntax for checking key existence and type
        if (jsonObj["mode"].is<const char*>()) {
            String mode = jsonObj["mode"].as<String>();
            mode.toUpperCase();
            if (mode == "AUTO") {
                isAutoMode = true;
                needsImmediateBroadcast = true;
                request->send(200, "application/json", "{\"status\":\"ok\",\"mode\":\"auto\"}");
            } else if (mode == "MANUAL") {
                isAutoMode = false;
                needsImmediateBroadcast = true;
                request->send(200, "application/json", "{\"status\":\"ok\",\"mode\":\"manual\"}");
            } else {
                request->send(400, "application/json", "{\"error\":\"Invalid mode. Use 'auto' or 'manual'.\"}");
            }
        } else {
            request->send(400, "application/json", "{\"error\":\"Missing 'mode' key.\"}");
        }
    });
    server.addHandler(modeHandler);

    // Handler for speed control
    AsyncCallbackJsonWebHandler* speedHandler = new AsyncCallbackJsonWebHandler("/api/control/speed", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject jsonObj = json.as<JsonObject>();
        // FIX: Use modern syntax for checking key existence and type
        if (jsonObj["speed"].is<int>()) {
            int speed = jsonObj["speed"];
            if (speed >= 0 && speed <= 100) {
                isAutoMode = false;
                manualFanSpeedPercentage = speed;
                needsImmediateBroadcast = true;
                request->send(200, "application/json", "{\"status\":\"ok\",\"speed\":" + String(speed) + "}");
            } else {
                request->send(400, "application/json", "{\"error\":\"Speed must be an integer between 0 and 100.\"}");
            }
        } else {
            request->send(400, "application/json", "{\"error\":\"Missing 'speed' integer key.\"}");
        }
    });
    server.addHandler(speedHandler);
    
    // Handler for fan curve setting
    AsyncCallbackJsonWebHandler* curveHandler = new AsyncCallbackJsonWebHandler("/api/config/curve", [](AsyncWebServerRequest *request, JsonVariant &json) {
        if (!tempSensorFound && !pcTempDataReceived) {
            request->send(400, "application/json", "{\"error\":\"Cannot set curve without a temperature source.\"}");
            return;
        }
        JsonArray newCurve = json.as<JsonArray>();
        if (newCurve && newCurve.size() >= 2 && newCurve.size() <= MAX_CURVE_POINTS) {
            bool curveValid = true;
            int lastTemp = -100;
            int tempTempPointsValidation[MAX_CURVE_POINTS];
            int tempPwmPercentagePointsValidation[MAX_CURVE_POINTS];
            int newNumPoints = newCurve.size();

            for(int k=0; k < newNumPoints; ++k) {
                JsonObject point = newCurve[k];
                if (!point["temp"].is<int>() || !point["pwmPercent"].is<int>()) {
                    curveValid = false; break;
                }
                int t = point["temp"];
                int p = point["pwmPercent"];
                if (t < 0 || t > 120 || p < 0 || p > 100 || (k > 0 && t <= lastTemp) ) {
                    curveValid = false; break;
                }
                tempTempPointsValidation[k] = t;
                tempPwmPercentagePointsValidation[k] = p;
                lastTemp = t;
            }

            if (curveValid) {
                numCurvePoints = newNumPoints;
                for(int k=0; k < numCurvePoints; ++k) {
                    tempPoints[k] = tempTempPointsValidation[k];
                    pwmPercentagePoints[k] = tempPwmPercentagePointsValidation[k];
                }
                saveFanCurveToNVS();
                fanCurveChanged = true;
                needsImmediateBroadcast = true;
                request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Fan curve updated.\"}");
            } else {
                request->send(400, "application/json", "{\"error\":\"Invalid curve data. Points must be sorted by temp, and values must be in range.\"}");
            }
        } else {
            request->send(400, "application/json", "{\"error\":\"Curve must be an array with 2 to " + String(MAX_CURVE_POINTS) + " points.\"}");
        }
    });
    server.addHandler(curveHandler);

    // Handler for MQTT config
    AsyncCallbackJsonWebHandler* mqttConfigHandler = new AsyncCallbackJsonWebHandler("/api/config/mqtt", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject jsonObj = json.as<JsonObject>();
        bool changed = false;

        if(jsonObj["enabled"].is<bool>()) {
            bool newMqttEnabled = jsonObj["enabled"];
            if (isMqttEnabled != newMqttEnabled) { isMqttEnabled = newMqttEnabled; changed = true; }
        }
        if(jsonObj["server"].is<const char*>()) {
            String newMqttServer = jsonObj["server"];
            if (newMqttServer.length() < sizeof(mqttServer) && strcmp(mqttServer, newMqttServer.c_str()) != 0) {
                strcpy(mqttServer, newMqttServer.c_str()); changed = true;
            }
        }
        if(jsonObj["port"].is<int>()) {
            int newMqttPort = jsonObj["port"];
            if (mqttPort != newMqttPort && newMqttPort > 0 && newMqttPort <= 65535) {
                mqttPort = newMqttPort; changed = true;
            }
        }
        if(jsonObj["user"].is<const char*>()) {
            String newMqttUser = jsonObj["user"];
            if (newMqttUser.length() < sizeof(mqttUser) && strcmp(mqttUser, newMqttUser.c_str()) != 0) {
                strcpy(mqttUser, newMqttUser.c_str()); changed = true;
            }
        }
        if(jsonObj["password"].is<const char*>()) {
            String newMqttPassword = jsonObj["password"];
            if (newMqttPassword.length() < sizeof(mqttPassword) && strcmp(mqttPassword, newMqttPassword.c_str()) != 0) {
                strcpy(mqttPassword, newMqttPassword.c_str()); changed = true;
            }
        }
        if(jsonObj["baseTopic"].is<const char*>()) {
            String newMqttBaseTopic = jsonObj["baseTopic"];
            if (newMqttBaseTopic.length() > 0 && newMqttBaseTopic.length() < sizeof(mqttBaseTopic) && strcmp(mqttBaseTopic, newMqttBaseTopic.c_str()) != 0) {
                strcpy(mqttBaseTopic, newMqttBaseTopic.c_str()); changed = true;
            }
        }
        
        if (changed) {
            saveMqttConfig();
            rebootNeeded = true;
            needsImmediateBroadcast = true;
            request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"MQTT config updated. Reboot needed.\"}");
        } else {
            request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"No changes detected.\"}");
        }
    });
    server.addHandler(mqttConfigHandler);

    // Handler for MQTT Discovery config
    AsyncCallbackJsonWebHandler* mqttDiscoveryHandler = new AsyncCallbackJsonWebHandler("/api/config/mqtt/discovery", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject jsonObj = json.as<JsonObject>();
        bool changed = false;

        if (jsonObj["enabled"].is<bool>()) {
            bool newDiscoveryEnabled = jsonObj["enabled"];
            if (isMqttDiscoveryEnabled != newDiscoveryEnabled) {
                isMqttDiscoveryEnabled = newDiscoveryEnabled;
                changed = true;
            }
        }
        if (jsonObj["prefix"].is<const char*>()) {
            String newPrefix = jsonObj["prefix"];
            if (newPrefix.length() < sizeof(mqttDiscoveryPrefix) && strcmp(mqttDiscoveryPrefix, newPrefix.c_str()) != 0) {
                strcpy(mqttDiscoveryPrefix, newPrefix.c_str());
                changed = true;
            }
        }
        
        if (changed) {
            saveMqttDiscoveryConfig();
            rebootNeeded = true;
            needsImmediateBroadcast = true;
            request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"MQTT Discovery config updated. Reboot needed.\"}");
        } else {
            request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"No changes detected.\"}");
        }
    });
    server.addHandler(mqttDiscoveryHandler);

    // System actions
    server.on("/api/system/reboot", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Rebooting...\"}");
        delay(1000); 
        ESP.restart();
    });

    server.on("/api/system/ota", HTTP_POST, [](AsyncWebServerRequest *request){
        if (ota_in_progress) {
            request->send(409, "application/json", "{\"error\":\"OTA already in progress.\"}");
        } else if (!isWiFiEnabled || WiFi.status() != WL_CONNECTED) {
            request->send(400, "application/json", "{\"error\":\"WiFi not connected for OTA.\"}");
        } else {
            request->send(202, "application/json", "{\"status\":\"ok\",\"message\":\"OTA check initiated.\"}");
            triggerOTAUpdateCheck();
        }
    });

    if(serialDebugEnabled) Serial.println("[SYSTEM] REST API routes initialized.");
}
