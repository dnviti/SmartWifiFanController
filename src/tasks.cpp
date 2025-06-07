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
            int wifiTimeout = 0;
            while (WiFi.status() != WL_CONNECTED && wifiTimeout < 30) { 
                vTaskDelay(pdMS_TO_TICKS(500)); 
                if(serialDebugEnabled) Serial.print("."); 
                wifiTimeout++;
            }
        } else {
            if(serialDebugEnabled) Serial.println("[WiFi] NetworkTask: SSID not configured or is default. Skipping WiFi connection attempt.");
        }
    } else {
         if(serialDebugEnabled) Serial.println("[WiFi] NetworkTask: WiFi is disabled by NVS config. Skipping WiFi connection.");
    }


    // --- Service Initialization ---
    if (isWiFiEnabled && WiFi.status() == WL_CONNECTED) {
        if(serialDebugEnabled) { 
            Serial.println("\n[WiFi] NetworkTask: Connected successfully!");
            Serial.print("[WiFi] NetworkTask: IP Address: "); Serial.println(WiFi.localIP());
            Serial.print("[WiFi] NetworkTask: Hostname: "); Serial.println(WiFi.getHostname());
        }
        
        setupWebServerRoutes(); 
        webSocket.begin();
        webSocket.onEvent(webSocketEvent);
        
        server.begin(); 
        if(serialDebugEnabled) Serial.println("[SYSTEM] HTTP server and WebSocket started on Core 0.");

        ElegantOTA.begin(&server);
        if(serialDebugEnabled) Serial.println("[SYSTEM] ElegantOTA started. Update endpoint: /update");

        if (isMqttEnabled) {
            setupMQTT(); 
        } else {
            if(serialDebugEnabled) Serial.println("[MQTT] MQTT is disabled. Skipping MQTT setup in NetworkTask.");
        }

    } else { 
        if(isWiFiEnabled && serialDebugEnabled) Serial.println("\n[WiFi] NetworkTask: WiFi Connection Failed or not attempted. Web/MQTT/OTA services may not be available.");
    }
    
    // --- Main Loop for Network Task ---
    for(;;) {
        if (isWiFiEnabled && WiFi.status() == WL_CONNECTED) { 
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
        } else if (isWiFiEnabled && WiFi.status() != WL_CONNECTED) {
            if (serialDebugEnabled && millis() % 15000 < 50) { 
                 Serial.println("[WiFi] NetworkTask: WiFi disconnected. Waiting for reconnection or config change. OTA/Web/MQTT unavailable.");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// --- Main Application Task (Core 1) ---
void mainAppTask(void *pvParameters) {
    if(serialDebugEnabled) Serial.println("[TASK] Main Application Task started on Core 1.");
    unsigned long lastTempReadTime = 0;
    unsigned long lastRpmCalculationTime = 0;
    lastRpmReadTime_Task = millis(); 

    for(;;) {
        unsigned long currentTime = millis();

        // FIX: Handle the timeout for the menu hint screen
        if (showMenuHint && (currentTime - menuHintStartTime > 3000)) {
            showMenuHint = false;
            displayUpdateNeeded = true; // Request a redraw to show the status screen again
        }

        if(serialDebugEnabled) { 
            handleSerialCommands(); 
        }
        handleMenuInput();      

        if (!isInMenuMode && !showMenuHint) { // Only run main logic if not in menu and not showing hint
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
                        setFanSpeed(autoPwmPerc); // setFanSpeed handles displayUpdateNeeded
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
        
        // Update display if a redraw has been requested
        if (displayUpdateNeeded) {
            updateDisplay();
            displayUpdateNeeded = false; // Reset the flag
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
