#include "tasks.h"
#include "config.h"
#include "network_handler.h" 
#include "input_handler.h"   
#include "fan_control.h"     
#include "display_handler.h" 
#include "mqtt_handler.h"    
#include <ElegantOTA.h>      
#include <WiFi.h>            

// --- Network Task (Core 0) ---
void networkTask(void *pvParameters) {
    if(serialDebugEnabled) Serial.println("[TASK] Network Task started on Core 0.");
    unsigned long lastPeriodicBroadcastTime = 0;
    unsigned long lastMqttStatusPublishTime = 0;
    unsigned long lastMqttCurvePublishTime = 0; 
    bool wasConnected = false;

    // --- WiFi Connection Handling ---
    if (isWiFiEnabled) {
        uint8_t mac[6];
        char hostname[32]; 
        WiFi.macAddress(mac);
        sprintf(hostname, "fancontrol-%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        
        if(serialDebugEnabled) Serial.printf("[WiFi] Setting hostname to: %s\n", hostname);
        if (!WiFi.setHostname(hostname)) {
            if(serialDebugEnabled) Serial.println("[WiFi_ERR] Failed to set hostname.");
        }

        if(serialDebugEnabled) { Serial.print("[WiFi] NetworkTask: Attempting connection to SSID: '"); Serial.print(current_ssid); Serial.println("'");}
        
        if (strlen(current_ssid) > 0 && strcmp(current_ssid, "YOUR_WIFI_SSID") != 0 && strcmp(current_ssid, "") != 0 ) {
            WiFi.mode(WIFI_STA); 
            WiFi.begin(current_ssid, current_password);
        } else {
            if(serialDebugEnabled) Serial.println("[WiFi] NetworkTask: SSID not configured or is default. Skipping WiFi connection attempt.");
        }
    } else {
         if(serialDebugEnabled) Serial.println("[WiFi] NetworkTask: WiFi is disabled by NVS config. Skipping WiFi connection.");
    }
    
    // --- Main Loop for Network Task ---
    for(;;) {
        // Handle WiFi connection state changes
        if (isWiFiEnabled && WiFi.status() != WL_CONNECTED && wasConnected) {
            if(serialDebugEnabled) Serial.println("[WiFi] NetworkTask: WiFi disconnected.");
            wasConnected = false;
            displayUpdateNeeded = true; // Request screen update on disconnect
        }

        if (isWiFiEnabled && WiFi.status() == WL_CONNECTED) { 
            if (!wasConnected) {
                // This block runs only once upon successful connection
                wasConnected = true;
                if(serialDebugEnabled) { 
                    Serial.println("\n[WiFi] NetworkTask: Connected successfully!");
                    Serial.print("[WiFi] NetworkTask: IP Address: "); Serial.println(WiFi.localIP());
                    Serial.print("[WiFi] NetworkTask: Hostname: "); Serial.println(WiFi.getHostname());
                }
                
                displayUpdateNeeded = true;
                
                // Initialize services that depend on WiFi
                setupWebServerRoutes(); 
                webSocket.begin();
                webSocket.onEvent(webSocketEvent);
                server.begin(); 
                ElegantOTA.begin(&server);
                if (isMqttEnabled) {
                    setupMQTT(); 
                }
                if(serialDebugEnabled) Serial.println("[SYSTEM] Network services (Web, OTA, MQTT) started.");
            }

            // Continuous network loops
            webSocket.loop();
            ElegantOTA.loop(); 
            unsigned long currentTime = millis();

            if (needsImmediateBroadcast) { 
                broadcastWebSocketData(); 
                if (isMqttEnabled && mqttClient.connected()) {
                    publishStatusMQTT(); 
                    if (fanCurveChanged) { 
                        publishFanCurveMQTT();
                        fanCurveChanged = false; 
                        lastMqttCurvePublishTime = currentTime; 
                    }
                }
                needsImmediateBroadcast = false; 
                lastPeriodicBroadcastTime = currentTime; 
                if (isMqttEnabled) lastMqttStatusPublishTime = currentTime; 
            }
            else if (currentTime - lastPeriodicBroadcastTime > 5000) {
                 broadcastWebSocketData();
                 lastPeriodicBroadcastTime = currentTime;
            }

            if (isMqttEnabled) {
                loopMQTT(); 
                if (mqttClient.connected() && (currentTime - lastMqttStatusPublishTime > 30000)) { 
                     publishStatusMQTT();
                     lastMqttStatusPublishTime = currentTime;
                }
                if (mqttClient.connected() && (currentTime - lastMqttCurvePublishTime > 300000)) { 
                     publishFanCurveMQTT();
                     lastMqttCurvePublishTime = currentTime;
                }
            }
        } else if (isWiFiEnabled) {
            // Attempt to reconnect if enabled but not connected
            if (millis() % 15000 < 50) { // Log occasionally
                 if(serialDebugEnabled) Serial.print(".");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Increased delay for stability
    }
}

// --- Main Application Task (Core 1) ---
void mainAppTask(void *pvParameters) {
    if(serialDebugEnabled) Serial.println("[TASK] Main Application Task started on Core 1.");
    unsigned long lastTempReadTime = 0;
    unsigned long lastRpmCalculationTime = 0;
    unsigned long lastDisplayUpdateTime = 0; // For periodic display refresh
    lastRpmReadTime_Task = millis(); 

    for(;;) {
        unsigned long currentTime = millis();

        if (showMenuHint && (currentTime - menuHintStartTime > 3000)) {
            showMenuHint = false;
            displayUpdateNeeded = true;
        }

        if(serialDebugEnabled) { 
            handleSerialCommands(); 
        }
        handleMenuInput();      

        if (!isInMenuMode && !showMenuHint) { 
            if (tempSensorFound) {
                if (currentTime - lastTempReadTime > 2000) { 
                    lastTempReadTime = currentTime;
                    float newTemp = bmp.readTemperature();
                    if (!isnan(newTemp)) { 
                        if (abs(newTemp - currentTemperature) > 0.05 || currentTemperature <= -990.0) {
                           currentTemperature = newTemp;
                           needsImmediateBroadcast = true;
                           displayUpdateNeeded = true;
                        }
                    } else {
                        if(serialDebugEnabled) Serial.println("[SENSOR_ERR] Failed to read from BMP280 sensor!");
                        if (currentTemperature > -990.0) { needsImmediateBroadcast = true; displayUpdateNeeded = true; }
                        currentTemperature = -999.0; 
                    }
                }
            } else { 
                if (currentTemperature > -990.0) { needsImmediateBroadcast = true; displayUpdateNeeded = true; }
                currentTemperature = -999.0; 
            }

            if (currentTime - lastRpmCalculationTime > 1000) { 
                lastRpmCalculationTime = currentTime;
                noInterrupts(); 
                unsigned long currentPulses = pulseCount;
                pulseCount = 0; 
                interrupts(); 
                
                unsigned long elapsedMillis = currentTime - lastRpmReadTime_Task;
                lastRpmReadTime_Task = currentTime; 

                int newRpm = 0;
                if (elapsedMillis > 0 && PULSES_PER_REVOLUTION > 0) {
                    newRpm = (currentPulses / (float)PULSES_PER_REVOLUTION) * (60000.0f / elapsedMillis);
                }
                if (newRpm != fanRpm) {
                    fanRpm = newRpm;
                    needsImmediateBroadcast = true;
                    displayUpdateNeeded = true;
                }
            }

            int oldFanSpeedPercentage = fanSpeedPercentage; 

            if (isAutoMode) {
                if (tempSensorFound) {
                    int autoPwmPerc = calculateAutoFanPWMPercentage(currentTemperature);
                    if (autoPwmPerc != fanSpeedPercentage) {
                        setFanSpeed(autoPwmPerc); 
                    }
                } else { 
                    if (AUTO_MODE_NO_SENSOR_FAN_PERCENTAGE != fanSpeedPercentage) {
                        setFanSpeed(AUTO_MODE_NO_SENSOR_FAN_PERCENTAGE);
                    }
                }
            } else { 
                if (manualFanSpeedPercentage != fanSpeedPercentage) {
                    setFanSpeed(manualFanSpeedPercentage);
                }
            }
        } 
        
        // FIX: Implement hybrid display update logic.
        // This will update the display immediately when an action requires it,
        // OR it will update periodically every 500ms to refresh live data.
        if (displayUpdateNeeded || (currentTime - lastDisplayUpdateTime > 500)) {
            updateDisplay();
            displayUpdateNeeded = false; // Reset the flag
            lastDisplayUpdateTime = currentTime;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
