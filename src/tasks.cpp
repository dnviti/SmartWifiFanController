#include "tasks.h"
#include "config.h"
#include "network_handler.h" 
#include "input_handler.h"   
#include "fan_control.h"     
#include "display_handler.h" 
#include "mqtt_handler.h"    // Added for MQTT
#include <ElegantOTA.h>      // Added for OTA Updates
#include <WiFi.h>            // Ensure WiFi is included for MAC address and hostname

// --- Network Task (Core 0) ---
void networkTask(void *pvParameters) {
    if(serialDebugEnabled) Serial.println("[TASK] Network Task started on Core 0.");
    unsigned long lastPeriodicBroadcastTime = 0;
    unsigned long lastMqttStatusPublishTime = 0;
    unsigned long lastMqttCurvePublishTime = 0; // ADDED: For periodic curve publish

    // --- WiFi Connection Handling ---
    if (isWiFiEnabled) {
        // Generate and set hostname before WiFi.begin()
        uint8_t mac[6];
        char hostname[32]; // "fancontrol-" is 11 chars, MAC is 12 chars, plus null terminator
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
            // Wait for connection, but don't block indefinitely if WiFi settings are bad
            while (WiFi.status() != WL_CONNECTED && wifiTimeout < 30) { // Approx 15 seconds timeout
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


    // --- Service Initialization (Websocket, MQTT, OTA) if WiFi is connected ---
    if (isWiFiEnabled && WiFi.status() == WL_CONNECTED) {
        if(serialDebugEnabled) { 
            Serial.println("\n[WiFi] NetworkTask: Connected successfully!");
            Serial.print("[WiFi] NetworkTask: IP Address: "); Serial.println(WiFi.localIP());
            Serial.print("[WiFi] NetworkTask: Hostname: "); Serial.println(WiFi.getHostname()); // Log the effective hostname
        }
        
        // Setup WebSockets
        setupWebServerRoutes(); 
        webSocket.begin();
        webSocket.onEvent(webSocketEvent);
        
        // Start AsyncWebServer
        server.begin(); 
        if(serialDebugEnabled) Serial.println("[SYSTEM] HTTP server and WebSocket started on Core 0.");

        // Initialize ElegantOTA
        ElegantOTA.begin(&server);
        // You can also set a custom ID for ElegantOTA if desired, which could include the hostname
        // ElegantOTA.setID(hostname); // Optional: If you want ElegantOTA to display this ID
        if(serialDebugEnabled) Serial.println("[SYSTEM] ElegantOTA started. Update endpoint: /update");


        // Setup MQTT if enabled
        if (isMqttEnabled) {
            setupMQTT(); // Initialize MQTT client, topics, server etc.
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
            ElegantOTA.loop(); // Handles OTA requests, important for some versions/modes
            unsigned long currentTime = millis();

            // Combined broadcast/publish logic for immediate updates
            if (needsImmediateBroadcast) { 
                broadcastWebSocketData(); // Send data to web clients
                if (isMqttEnabled && mqttClient.connected()) {
                    publishStatusMQTT(); 
                    if (fanCurveChanged) { // Only publish curve if it actually changed
                        publishFanCurveMQTT();
                        fanCurveChanged = false; // Reset flag
                        lastMqttCurvePublishTime = currentTime; // Update time of last curve publish
                    }
                }
                needsImmediateBroadcast = false; 
                lastPeriodicBroadcastTime = currentTime; // Reset periodic timer
                if (isMqttEnabled) lastMqttStatusPublishTime = currentTime; // Reset MQTT periodic timer
            }
            // Periodic WebSocket broadcast
            else if (currentTime - lastPeriodicBroadcastTime > 5000) {
                 broadcastWebSocketData();
                 lastPeriodicBroadcastTime = currentTime;
            }


            // MQTT Loop and Periodic Publishing
            if (isMqttEnabled) {
                loopMQTT(); // Handles connection, client.loop(), and reconnections
                
                // Periodic status publish if not covered by needsImmediateBroadcast
                if (mqttClient.connected() && (currentTime - lastMqttStatusPublishTime > 30000)) { // e.g., every 30 seconds
                     publishStatusMQTT();
                     lastMqttStatusPublishTime = currentTime;
                }
                // ADDED: Periodic fan curve publish (less frequent)
                if (mqttClient.connected() && (currentTime - lastMqttCurvePublishTime > 300000)) { // e.g., every 5 minutes
                     publishFanCurveMQTT();
                     lastMqttCurvePublishTime = currentTime;
                }
            }
        } else if (isWiFiEnabled && WiFi.status() != WL_CONNECTED) {
            // Optional: Attempt to reconnect WiFi if it drops
            // For now, MQTT will also disconnect if WiFi drops (handled in loopMQTT).
            // ElegantOTA also requires WiFi.
            if (serialDebugEnabled && millis() % 15000 < 50) { // Log occasionally
                 Serial.println("[WiFi] NetworkTask: WiFi disconnected. Waiting for reconnection or config change. OTA/Web/MQTT unavailable.");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // Standard delay for cooperative multitasking
    }
}

// --- Main Application Task (Core 1) ---
void mainAppTask(void *pvParameters) {
    if(serialDebugEnabled) Serial.println("[TASK] Main Application Task started on Core 1.");
    unsigned long lastTempReadTime = 0;
    unsigned long lastRpmTimestamp = 0; // Renamed from lastRpmCalculationTime, initialized to 0
    unsigned long lastLcdUpdateTime = 0;
    // Removed: lastRpmReadTime_Task = millis(); // This line is removed

    if (isInMenuMode) displayMenu(); else updateLCD_NormalMode();

    for(;;) {
        unsigned long currentTime = millis();

        if(serialDebugEnabled) { 
            handleSerialCommands(); 
        }
        handleMenuInput();      

        if (!isInMenuMode) { // Only perform these actions if not in menu
            // Read Temperature
            if (tempSensorFound) {
                if (currentTime - lastTempReadTime > 2000) { // Read every 2 seconds
                    lastTempReadTime = currentTime;
                    float newTemp = bmp.readTemperature();
                    if (!isnan(newTemp)) { 
                        if (abs(newTemp - currentTemperature) > 0.05 || currentTemperature <= -990.0) { // Update if changed significantly or first read
                           currentTemperature = newTemp;
                           needsImmediateBroadcast = true; // Temperature changed, signal update
                        }
                    } else {
                        if(serialDebugEnabled) Serial.println("[SENSOR_ERR] Failed to read from BMP280 sensor!");
                        if (currentTemperature > -990.0) needsImmediateBroadcast = true; // Was valid, now not
                        currentTemperature = -999.0; 
                    }
                }
            } else { // Sensor not found
                if (currentTemperature > -990.0) needsImmediateBroadcast = true; // Was valid, now not
                currentTemperature = -999.0; 
            }

            // Calculate RPM
            if (currentTime - lastRpmTimestamp >= 1000) { // Calculate every 1 second
                unsigned long previousRpmTimestamp = lastRpmTimestamp; // Store the timestamp from the *start* of the previous cycle
                lastRpmTimestamp = currentTime; // Update for the *next* cycle's start

                noInterrupts(); 
                unsigned long currentPulses = pulseCount;
                pulseCount = 0; 
                interrupts(); 
                
                unsigned long elapsedMillis = currentTime - previousRpmTimestamp; // This is the actual duration over which pulses were counted
                
                int newRpm = 0;
                if (elapsedMillis > 0 && PULSES_PER_REVOLUTION > 0) {
                    newRpm = (currentPulses / (float)PULSES_PER_REVOLUTION) * (60000.0f / elapsedMillis);
                }
                if (newRpm != fanRpm) {
                    fanRpm = newRpm;
                    needsImmediateBroadcast = true; // RPM changed
                }
            }

            // Fan Control Logic
            int oldFanSpeedPercentage = fanSpeedPercentage; 

            if (isAutoMode) {
                if (tempSensorFound) {
                    int autoPwmPerc = calculateAutoFanPWMPercentage(currentTemperature);
                    if (autoPwmPerc != fanSpeedPercentage) {
                        setFanSpeed(autoPwmPerc); // setFanSpeed sets needsImmediateBroadcast
                    }
                } else { // Auto mode but no sensor
                    if (AUTO_MODE_NO_SENSOR_FAN_PERCENTAGE != fanSpeedPercentage) {
                        setFanSpeed(AUTO_MODE_NO_SENSOR_FAN_PERCENTAGE);
                    }
                }
            } else { // Manual mode
                if (manualFanSpeedPercentage != fanSpeedPercentage) {
                    setFanSpeed(manualFanSpeedPercentage);
                }
            }
            
            // Update LCD
            // Update more frequently if something changed, or every second regardless
            if (currentTime - lastLcdUpdateTime > 1000 || needsImmediateBroadcast) { 
                updateLCD_NormalMode();
                lastLcdUpdateTime = currentTime;
            }
        } 
        // If in menu mode, displayMenu() is called by handleMenuInput() when changes occur.
        
        vTaskDelay(pdMS_TO_TICKS(50)); // Standard delay for cooperative multitasking
    }
}
