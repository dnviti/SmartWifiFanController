#include "tasks.h"
#include "config.h"
#include "network_handler.h" // For setupWebServerRoutes, webSocketEvent, broadcastWebSocketData
#include "input_handler.h"   // For handleSerialCommands, handleMenuInput
#include "fan_control.h"     // For calculateAutoFanPWMPercentage, setFanSpeed
#include "display_handler.h" // For updateLCD_NormalMode, displayMenu

// --- Network Task (Core 0) ---
void networkTask(void *pvParameters) {
    if(serialDebugEnabled) Serial.println("[TASK] Network Task started on Core 0.");
    unsigned long lastPeriodicBroadcastTime = 0;

    if (!isWiFiEnabled) { 
        if(serialDebugEnabled) Serial.println("[NETWORK_TASK] WiFi is disabled by NVS config. Network task ending.");
        vTaskDelete(NULL); 
        return;
    }

    if(serialDebugEnabled) { Serial.print("[WiFi] NetworkTask: Attempting connection to SSID: '"); Serial.print(current_ssid); Serial.println("'");}
    
    if (strlen(current_ssid) > 0 && strcmp(current_ssid, "YOUR_WIFI_SSID") != 0 && strcmp(current_ssid, "") != 0 ) {
        WiFi.mode(WIFI_STA); 
        WiFi.begin(current_ssid, current_password);
        int wifiTimeout = 0;
        while (WiFi.status() != WL_CONNECTED && wifiTimeout < 30) { 
            delay(500); if(serialDebugEnabled) Serial.print("."); 
            wifiTimeout++;
        }
    } else {
        if(serialDebugEnabled) Serial.println("[WiFi] NetworkTask: SSID not configured or is default. Skipping connection attempt.");
    }

    if (WiFi.status() == WL_CONNECTED) {
        if(serialDebugEnabled) { Serial.println("\n[WiFi] NetworkTask: Connected successfully!");
        Serial.print("[WiFi] NetworkTask: IP Address: "); Serial.println(WiFi.localIP());}
        
        setupWebServerRoutes(); // Setup routes to serve from SPIFFS
        webSocket.begin();
        webSocket.onEvent(webSocketEvent);
        server.begin();
        if(serialDebugEnabled) Serial.println("[SYSTEM] HTTP server and WebSocket started on Core 0.");
    } else { 
        if(serialDebugEnabled) Serial.println("\n[WiFi] NetworkTask: Connection Failed or not attempted. Web services may not be available.");
    }
    
    for(;;) {
        if (isWiFiEnabled && WiFi.status() == WL_CONNECTED) { 
            webSocket.loop();
            unsigned long currentTime = millis();
            if (needsImmediateBroadcast || (currentTime - lastPeriodicBroadcastTime > 5000)) { 
                broadcastWebSocketData();
                needsImmediateBroadcast = false; 
                lastPeriodicBroadcastTime = currentTime;
            }
        } else if (isWiFiEnabled && WiFi.status() != WL_CONNECTED) {
            // Optional: More robust retry or status update
        }
        vTaskDelay(pdMS_TO_TICKS(50)); 
    }
}

// --- Main Application Task (Core 1) ---
void mainAppTask(void *pvParameters) {
    if(serialDebugEnabled) Serial.println("[TASK] Main Application Task started on Core 1.");
    unsigned long lastTempReadTime = 0;
    unsigned long lastRpmCalculationTime = 0;
    unsigned long lastLcdUpdateTime = 0;
    lastRpmReadTime_Task = millis();

    if (isInMenuMode) displayMenu(); else updateLCD_NormalMode();

    for(;;) {
        unsigned long currentTime = millis();

        if(serialDebugEnabled) { 
            handleSerialCommands(); 
        }
        handleMenuInput();      

        if (!isInMenuMode) {
            if (tempSensorFound) {
                if (currentTime - lastTempReadTime > 2000) { 
                    lastTempReadTime = currentTime;
                    float newTemp = bmp.readTemperature();
                    if (!isnan(newTemp)) { 
                        currentTemperature = newTemp;
                    } else {
                        if(serialDebugEnabled) Serial.println("[SENSOR] Failed to read from BMP280 sensor!");
                        currentTemperature = -999.0; 
                    }
                }
            } else {
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

                if (elapsedMillis > 0 && PULSES_PER_REVOLUTION > 0) {
                    fanRpm = (currentPulses / (float)PULSES_PER_REVOLUTION) * (60000.0f / elapsedMillis);
                } else {
                    fanRpm = 0;
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
            
            if (currentTime - lastLcdUpdateTime > 1000 || oldFanSpeedPercentage != fanSpeedPercentage || needsImmediateBroadcast) { 
                updateLCD_NormalMode();
                lastLcdUpdateTime = currentTime;
            }
        } 
        
        vTaskDelay(pdMS_TO_TICKS(50)); 
    }
}
