#include "input_handler.h"
#include "config.h"         
#include "nvs_handler.h"    
#include "display_handler.h"
#include "fan_control.h" 
#include "mqtt_handler.h" 
#include "ota_updater.h" // For triggerOTAUpdateCheck()

// --- Button Input Handling for LCD Menu ---
void handleMenuInput() {
    bool button_states[5]; 
    int button_pins[5] = {BTN_MENU_PIN, BTN_UP_PIN, BTN_DOWN_PIN, BTN_SELECT_PIN, BTN_BACK_PIN};
    unsigned long currentTime = millis();

    for(int i=0; i<5; ++i) {
        button_states[i] = digitalRead(button_pins[i]) == LOW;
        if (button_states[i] && !buttonPressedState[i] && (currentTime - buttonPressTime[i] > debounceDelay)) {
            buttonPressTime[i] = currentTime;
            buttonPressedState[i] = true;

            if (i == 0) { // BTN_MENU_PIN
                isInMenuMode = !isInMenuMode;
                if (isInMenuMode) {
                    if(serialDebugEnabled) Serial.println("[MENU_LCD] Entered Menu Mode.");
                    currentMenuScreen = MAIN_MENU; 
                    selectedMenuItem = 0;
                    ota_status_message = "OTA Idle"; // Reset OTA message when entering menu
                } else {
                    if(serialDebugEnabled) Serial.println("[MENU_LCD] Exited Menu Mode.");
                    if (rebootNeeded) { 
                        currentMenuScreen = CONFIRM_REBOOT; 
                        isInMenuMode = true; 
                        selectedMenuItem = 0;
                    }
                }
            }

            if (!isInMenuMode && i > 0) continue; 
            if (!isInMenuMode && i == 0) { 
                 if(!rebootNeeded) updateLCD_NormalMode(); else displayMenu(); 
                 return;
            }
            
            if (!isInMenuMode) return; 

            // --- Button Actions in Menu Mode ---
            switch(i) { 
                case 1: // UP
                    if (currentMenuScreen == WIFI_PASSWORD_ENTRY) {
                        currentPasswordEditChar++;
                        if (currentPasswordEditChar > '~') currentPasswordEditChar = ' '; 
                    } else if (currentMenuScreen == MQTT_SERVER_ENTRY || currentMenuScreen == MQTT_USER_ENTRY || 
                               currentMenuScreen == MQTT_PASS_ENTRY || currentMenuScreen == MQTT_TOPIC_ENTRY ||
                               currentMenuScreen == MQTT_DISCOVERY_PREFIX_ENTRY) { 
                        currentGeneralEditChar++;
                        if (currentGeneralEditChar > '~') currentGeneralEditChar = ' ';
                    } else if (currentMenuScreen == MQTT_PORT_ENTRY) {
                        currentGeneralEditChar++; 
                        if (currentGeneralEditChar > '9') currentGeneralEditChar = '0';
                    } else if (currentMenuScreen == OTA_UPDATE_SCREEN && !ota_in_progress) {
                        if (selectedMenuItem > 0) selectedMenuItem--;
                    }
                     else if (selectedMenuItem > 0) {
                        selectedMenuItem--;
                    }
                    break;
                case 2: // DOWN
                    if (currentMenuScreen == WIFI_PASSWORD_ENTRY) {
                        currentPasswordEditChar--;
                        if (currentPasswordEditChar < ' ') currentPasswordEditChar = '~';
                    } else if (currentMenuScreen == MQTT_SERVER_ENTRY || currentMenuScreen == MQTT_USER_ENTRY || 
                               currentMenuScreen == MQTT_PASS_ENTRY || currentMenuScreen == MQTT_TOPIC_ENTRY ||
                               currentMenuScreen == MQTT_DISCOVERY_PREFIX_ENTRY) { 
                        currentGeneralEditChar--;
                        if (currentGeneralEditChar < ' ') currentGeneralEditChar = '~';
                    } else if (currentMenuScreen == MQTT_PORT_ENTRY) {
                        currentGeneralEditChar--; 
                        if (currentGeneralEditChar < '0') currentGeneralEditChar = '9';
                    } else if (currentMenuScreen == OTA_UPDATE_SCREEN && !ota_in_progress) {
                        // Max items on OTA screen: "Check & Update" (0), "Back" (1)
                        if (selectedMenuItem < 1) selectedMenuItem++; 
                    }
                     else {
                        int maxItems = 0;
                        // Determine max items based on current screen
                        if (currentMenuScreen == MAIN_MENU) maxItems = 3; // WiFi, MQTT, OTA, Exit
                        else if (currentMenuScreen == WIFI_SETTINGS) maxItems = 6; // Last item is "Back"
                        else if (currentMenuScreen == MQTT_SETTINGS) maxItems = 7; // Last item is "Back"
                        else if (currentMenuScreen == MQTT_DISCOVERY_SETTINGS) maxItems = 2; // Last item is "Back"
                        else if (currentMenuScreen == WIFI_SCAN) maxItems = scanResultCount > 0 ? scanResultCount - 1 : 0; // Scan results are 0-indexed
                        else if (currentMenuScreen == CONFIRM_REBOOT) maxItems = 1; // Yes, No

                        if (selectedMenuItem < maxItems) selectedMenuItem++;
                    }
                    break;
                case 3: // SELECT
                    if(serialDebugEnabled) Serial.printf("[MENU_LCD_ACTION] Screen: %d, Item: %d\n", currentMenuScreen, selectedMenuItem);
                    
                    if (currentMenuScreen == MAIN_MENU) {
                        if (selectedMenuItem == 0) { currentMenuScreen = WIFI_SETTINGS; selectedMenuItem = 0; }
                        else if (selectedMenuItem == 1) { currentMenuScreen = MQTT_SETTINGS; selectedMenuItem = 0; }
                        else if (selectedMenuItem == 2) { currentMenuScreen = OTA_UPDATE_SCREEN; selectedMenuItem = 0; ota_status_message = "Press SEL to check"; } 
                        else if (selectedMenuItem == 3) { isInMenuMode = false; if(rebootNeeded){currentMenuScreen = CONFIRM_REBOOT; isInMenuMode=true; selectedMenuItem=0;}}
                    } 
                    else if (currentMenuScreen == WIFI_SETTINGS) {
                        if (selectedMenuItem == 0) { isWiFiEnabled = !isWiFiEnabled; saveWiFiConfig(); rebootNeeded = true; currentMenuScreen = CONFIRM_REBOOT; selectedMenuItem = 0; } 
                        else if (selectedMenuItem == 1) { currentMenuScreen = WIFI_SCAN; selectedMenuItem = 0; performWiFiScan(); } 
                        else if (selectedMenuItem == 2) { currentMenuScreen = WIFI_SCAN; selectedMenuItem = 0; performWiFiScan(); } 
                        else if (selectedMenuItem == 3) { passwordCharIndex = 0; currentPasswordEditChar = 'a'; memset(passwordInputBuffer, 0, sizeof(passwordInputBuffer)); currentMenuScreen = WIFI_PASSWORD_ENTRY; selectedMenuItem = 0; } 
                        else if (selectedMenuItem == 4) { attemptWiFiConnection(); } 
                        else if (selectedMenuItem == 5) { disconnectWiFi(); } 
                        else if (selectedMenuItem == 6) { currentMenuScreen = MAIN_MENU; selectedMenuItem = 0; }
                    }
                    else if (currentMenuScreen == WIFI_SCAN) {
                        if (scanResultCount > 0 && selectedMenuItem < scanResultCount) {
                            strncpy(current_ssid, scannedSSIDs[selectedMenuItem].c_str(), sizeof(current_ssid) - 1);
                            current_ssid[sizeof(current_ssid)-1] = '\0';
                            if(serialDebugEnabled) Serial.printf("[MENU_LCD] SSID Selected: %s\n", current_ssid);
                            currentMenuScreen = WIFI_SETTINGS; selectedMenuItem = 3; // Highlight Password Set
                        }
                    }
                    else if (currentMenuScreen == WIFI_PASSWORD_ENTRY) { 
                        if (passwordCharIndex < sizeof(passwordInputBuffer) - 2) { 
                            passwordInputBuffer[passwordCharIndex] = currentPasswordEditChar;
                            passwordCharIndex++;
                            currentPasswordEditChar = 'a'; 
                        } else { 
                             strncpy(current_password, passwordInputBuffer, sizeof(current_password) -1);
                             current_password[sizeof(current_password)-1] = '\0';
                             if(serialDebugEnabled) Serial.printf("[MENU_LCD] WiFi Password Entered (length %d)\n", strlen(current_password));
                             saveWiFiConfig(); 
                             currentMenuScreen = WIFI_SETTINGS; selectedMenuItem = 4; 
                        }
                    }
                    else if (currentMenuScreen == MQTT_SETTINGS) {
                        if (selectedMenuItem == 0) { isMqttEnabled = !isMqttEnabled; saveMqttConfig(); rebootNeeded = true; currentMenuScreen = CONFIRM_REBOOT; selectedMenuItem = 0; } 
                        else if (selectedMenuItem == 1) { memset(generalInputBuffer, 0, sizeof(generalInputBuffer)); strncpy(generalInputBuffer, mqttServer, sizeof(generalInputBuffer)-1); generalInputCharIndex = strlen(generalInputBuffer); currentGeneralEditChar = (generalInputCharIndex > 0 && generalInputBuffer[generalInputCharIndex-1] != ' ') ? generalInputBuffer[generalInputCharIndex-1] : 'a'; currentMenuScreen = MQTT_SERVER_ENTRY; selectedMenuItem = 0; } 
                        else if (selectedMenuItem == 2) { memset(generalInputBuffer, 0, sizeof(generalInputBuffer)); String(mqttPort).toCharArray(generalInputBuffer, sizeof(generalInputBuffer)); generalInputCharIndex = strlen(generalInputBuffer); currentGeneralEditChar = '0'; currentMenuScreen = MQTT_PORT_ENTRY; selectedMenuItem = 0; } 
                        else if (selectedMenuItem == 3) { memset(generalInputBuffer, 0, sizeof(generalInputBuffer)); strncpy(generalInputBuffer, mqttUser, sizeof(generalInputBuffer)-1); generalInputCharIndex = strlen(generalInputBuffer); currentGeneralEditChar = 'a'; currentMenuScreen = MQTT_USER_ENTRY; selectedMenuItem = 0; } 
                        else if (selectedMenuItem == 4) { memset(generalInputBuffer, 0, sizeof(generalInputBuffer)); strncpy(generalInputBuffer, mqttPassword, sizeof(generalInputBuffer)-1); generalInputCharIndex = strlen(generalInputBuffer); currentGeneralEditChar = 'a'; currentMenuScreen = MQTT_PASS_ENTRY; selectedMenuItem = 0; } 
                        else if (selectedMenuItem == 5) { memset(generalInputBuffer, 0, sizeof(generalInputBuffer)); strncpy(generalInputBuffer, mqttBaseTopic, sizeof(generalInputBuffer)-1); generalInputCharIndex = strlen(generalInputBuffer); currentGeneralEditChar = 'a'; currentMenuScreen = MQTT_TOPIC_ENTRY; selectedMenuItem = 0; } 
                        else if (selectedMenuItem == 6) { currentMenuScreen = MQTT_DISCOVERY_SETTINGS; selectedMenuItem = 0; }
                        else if (selectedMenuItem == 7) { currentMenuScreen = MAIN_MENU; selectedMenuItem = 0; }
                    }
                    else if (currentMenuScreen == MQTT_SERVER_ENTRY || currentMenuScreen == MQTT_USER_ENTRY || 
                             currentMenuScreen == MQTT_TOPIC_ENTRY || currentMenuScreen == MQTT_PASS_ENTRY ||
                             currentMenuScreen == MQTT_DISCOVERY_PREFIX_ENTRY ) { 
                        if (generalInputCharIndex < ( (currentMenuScreen == MQTT_DISCOVERY_PREFIX_ENTRY) ? sizeof(mqttDiscoveryPrefix) : sizeof(generalInputBuffer) ) - 2) { 
                            generalInputBuffer[generalInputCharIndex] = currentGeneralEditChar;
                            generalInputCharIndex++;
                            currentGeneralEditChar = (currentGeneralEditChar == '.') ? 'a' : currentGeneralEditChar; // Cycle char or reset
                        } else { // Entry complete for this field
                            if (currentMenuScreen == MQTT_SERVER_ENTRY) { strncpy(mqttServer, generalInputBuffer, sizeof(mqttServer)-1); mqttServer[sizeof(mqttServer)-1] = '\0'; saveMqttConfig(); rebootNeeded = true; currentMenuScreen = MQTT_SETTINGS; selectedMenuItem = 1;}
                            else if (currentMenuScreen == MQTT_USER_ENTRY) { strncpy(mqttUser, generalInputBuffer, sizeof(mqttUser)-1); mqttUser[sizeof(mqttUser)-1] = '\0'; saveMqttConfig(); rebootNeeded = true; currentMenuScreen = MQTT_SETTINGS; selectedMenuItem = 3;}
                            else if (currentMenuScreen == MQTT_TOPIC_ENTRY) { strncpy(mqttBaseTopic, generalInputBuffer, sizeof(mqttBaseTopic)-1); mqttBaseTopic[sizeof(mqttBaseTopic)-1] = '\0'; saveMqttConfig(); rebootNeeded = true; currentMenuScreen = MQTT_SETTINGS; selectedMenuItem = 5;}
                            else if (currentMenuScreen == MQTT_PASS_ENTRY) { strncpy(mqttPassword, generalInputBuffer, sizeof(mqttPassword)-1); mqttPassword[sizeof(mqttPassword)-1] = '\0'; saveMqttConfig(); rebootNeeded = true; currentMenuScreen = MQTT_SETTINGS; selectedMenuItem = 4;}
                            else if (currentMenuScreen == MQTT_DISCOVERY_PREFIX_ENTRY) { strncpy(mqttDiscoveryPrefix, generalInputBuffer, sizeof(mqttDiscoveryPrefix)-1); mqttDiscoveryPrefix[sizeof(mqttDiscoveryPrefix)-1] = '\0'; saveMqttDiscoveryConfig(); rebootNeeded = true; currentMenuScreen = MQTT_DISCOVERY_SETTINGS; selectedMenuItem = 1;}
                        }
                    }
                    else if (currentMenuScreen == MQTT_PORT_ENTRY) {
                        if (generalInputCharIndex < 5) { // Max 5 digits for port
                            generalInputBuffer[generalInputCharIndex] = currentGeneralEditChar;
                            generalInputCharIndex++;
                            currentGeneralEditChar = '0'; // Reset for next digit
                        } else { // Port entry complete
                            mqttPort = atoi(generalInputBuffer);
                            if (mqttPort == 0 && strlen(generalInputBuffer) > 0 && generalInputBuffer[0] != '0') mqttPort = 1883; // Basic validation
                            saveMqttConfig();
                            rebootNeeded = true;
                            currentMenuScreen = MQTT_SETTINGS; selectedMenuItem = 2;
                        }
                    }
                    else if (currentMenuScreen == MQTT_DISCOVERY_SETTINGS) {
                        if (selectedMenuItem == 0) { isMqttDiscoveryEnabled = !isMqttDiscoveryEnabled; saveMqttDiscoveryConfig(); rebootNeeded = true; currentMenuScreen = CONFIRM_REBOOT; selectedMenuItem = 0; } 
                        else if (selectedMenuItem == 1) { memset(generalInputBuffer, 0, sizeof(generalInputBuffer)); strncpy(generalInputBuffer, mqttDiscoveryPrefix, sizeof(generalInputBuffer)-1); generalInputCharIndex = strlen(generalInputBuffer); currentGeneralEditChar = (generalInputCharIndex > 0 && generalInputBuffer[generalInputCharIndex-1] != ' ') ? generalInputBuffer[generalInputCharIndex-1] : 'h'; currentMenuScreen = MQTT_DISCOVERY_PREFIX_ENTRY; selectedMenuItem = 0; } 
                        else if (selectedMenuItem == 2) { currentMenuScreen = MQTT_SETTINGS; selectedMenuItem = 6; }
                    }
                    else if (currentMenuScreen == OTA_UPDATE_SCREEN) { 
                        if (ota_in_progress) {
                            if(serialDebugEnabled) Serial.println("[MENU_LCD] OTA already in progress. Button ignored.");
                        } else {
                            if (selectedMenuItem == 0) { // "Check & Update"
                                if(serialDebugEnabled) Serial.println("[MENU_LCD] Triggering OTA Update Check from LCD.");
                                triggerOTAUpdateCheck(); 
                                selectedMenuItem = 0; 
                            } else if (selectedMenuItem == 1) { // "Back to Main"
                                currentMenuScreen = MAIN_MENU; selectedMenuItem = 2; 
                                ota_status_message = "OTA Idle"; 
                            }
                        }
                    }
                    else if (currentMenuScreen == CONFIRM_REBOOT) {
                        if(selectedMenuItem == 0) { 
                            if(serialDebugEnabled) Serial.println("[SYSTEM_LCD] Rebooting now...");
                            lcd.clear(); lcd.print("Rebooting..."); 
                            delay(1000); ESP.restart();
                        } else { 
                            rebootNeeded = false; currentMenuScreen = MAIN_MENU; selectedMenuItem = 0;
                        }
                    }
                    break; 
                case 4: // BACK
                     if (currentMenuScreen == OTA_UPDATE_SCREEN) { 
                        if (ota_in_progress) {
                             if(serialDebugEnabled) Serial.println("[MENU_LCD] Cannot go back, OTA in progress.");
                        } else {
                            currentMenuScreen = MAIN_MENU; selectedMenuItem = 2; 
                            ota_status_message = "OTA Idle";
                        }
                     }
                     else if (currentMenuScreen == WIFI_PASSWORD_ENTRY) { 
                        if (passwordCharIndex > 0) { passwordCharIndex--; passwordInputBuffer[passwordCharIndex] = '\0'; currentPasswordEditChar = (passwordCharIndex > 0) ? passwordInputBuffer[passwordCharIndex-1] : 'a'; } 
                        else { currentMenuScreen = WIFI_SETTINGS; selectedMenuItem = 3; }
                    } else if (currentMenuScreen == MQTT_SERVER_ENTRY || currentMenuScreen == MQTT_PORT_ENTRY ||
                               currentMenuScreen == MQTT_USER_ENTRY || currentMenuScreen == MQTT_PASS_ENTRY ||
                               currentMenuScreen == MQTT_TOPIC_ENTRY || currentMenuScreen == MQTT_DISCOVERY_PREFIX_ENTRY) { 
                        if (generalInputCharIndex > 0) {
                            generalInputCharIndex--; generalInputBuffer[generalInputCharIndex] = '\0';
                            currentGeneralEditChar = (generalInputCharIndex > 0) ? generalInputBuffer[generalInputCharIndex-1] : 'a';
                            if(currentMenuScreen == MQTT_PORT_ENTRY) currentGeneralEditChar = (generalInputCharIndex > 0 && generalInputBuffer[generalInputCharIndex-1] >= '0' && generalInputBuffer[generalInputCharIndex-1] <= '9') ? generalInputBuffer[generalInputCharIndex-1] : '0';
                            if(currentMenuScreen == MQTT_DISCOVERY_PREFIX_ENTRY) currentGeneralEditChar = (generalInputCharIndex > 0 && generalInputBuffer[generalInputCharIndex-1] != ' ') ? generalInputBuffer[generalInputCharIndex-1] : 'h';
                        } else { // Exiting input field with BACK when buffer is empty
                            if (currentMenuScreen == MQTT_SERVER_ENTRY) { currentMenuScreen = MQTT_SETTINGS; selectedMenuItem = 1;}
                            else if (currentMenuScreen == MQTT_PORT_ENTRY) { currentMenuScreen = MQTT_SETTINGS; selectedMenuItem = 2;}
                            else if (currentMenuScreen == MQTT_USER_ENTRY) { currentMenuScreen = MQTT_SETTINGS; selectedMenuItem = 3;}
                            else if (currentMenuScreen == MQTT_PASS_ENTRY) { currentMenuScreen = MQTT_SETTINGS; selectedMenuItem = 4;}
                            else if (currentMenuScreen == MQTT_TOPIC_ENTRY) { currentMenuScreen = MQTT_SETTINGS; selectedMenuItem = 5;}
                            else if (currentMenuScreen == MQTT_DISCOVERY_PREFIX_ENTRY) { currentMenuScreen = MQTT_DISCOVERY_SETTINGS; selectedMenuItem = 1; }
                        }
                    }
                     else if (currentMenuScreen == WIFI_SETTINGS || currentMenuScreen == WIFI_STATUS || currentMenuScreen == MQTT_SETTINGS) {
                        currentMenuScreen = MAIN_MENU; selectedMenuItem = (currentMenuScreen == MQTT_SETTINGS ? 1:0) ; 
                    } else if (currentMenuScreen == MQTT_DISCOVERY_SETTINGS) { 
                        currentMenuScreen = MQTT_SETTINGS; selectedMenuItem = 6; 
                    }
                     else if (currentMenuScreen == WIFI_SCAN) {
                        currentMenuScreen = WIFI_SETTINGS; selectedMenuItem = 1; 
                    } else if (currentMenuScreen == CONFIRM_REBOOT) {
                        rebootNeeded = false; currentMenuScreen = MAIN_MENU; selectedMenuItem = 0;
                    } else if (currentMenuScreen == MAIN_MENU) {
                        isInMenuMode = false;
                         if (rebootNeeded) { currentMenuScreen = CONFIRM_REBOOT; isInMenuMode = true; selectedMenuItem = 0;}
                    } else { 
                        currentMenuScreen = MAIN_MENU; selectedMenuItem = 0;
                    }
                    break; 
            } 
            
            if(isInMenuMode) {
                displayMenu(); 
            } else {
                updateLCD_NormalMode(); 
            }
        } 
        if (!button_states[i] && buttonPressedState[i]) { 
            buttonPressedState[i] = false; 
        }
    } 
}

void handleSerialCommands() {
    if (!serialDebugEnabled) return; 

    if (Serial.available() > 0) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        Serial.print("[SERIAL_CMD_RCVD] << "); Serial.println(command);

        if (command.equalsIgnoreCase("help")) {
            Serial.println("--- Available Serial Commands ---");
            Serial.println("status                     : View current status");
            Serial.println("set_mode auto              : Set Auto fan mode");
            Serial.println("set_mode manual <0-100>    : Set Manual fan mode and speed %");
            Serial.println("wifi_enable                : Enable WiFi (reboot needed)");
            Serial.println("wifi_disable               : Disable WiFi (reboot needed)");
            Serial.println("set_ssid <your_ssid>       : Set WiFi SSID");
            Serial.println("set_pass <your_password>   : Set WiFi Password");
            Serial.println("connect_wifi               : Attempt WiFi connection (prompts reboot on success)");
            Serial.println("disconnect_wifi            : Disconnect from current WiFi");
            Serial.println("scan_wifi                  : Scan for WiFi networks");
            Serial.println("mqtt_enable                : Enable MQTT (reboot needed)");
            Serial.println("mqtt_disable               : Disable MQTT (reboot needed)");
            Serial.println("set_mqtt_server <addr>     : Set MQTT Broker Address");
            Serial.println("set_mqtt_port <port>       : Set MQTT Broker Port");
            Serial.println("set_mqtt_user <user>       : Set MQTT Username");
            Serial.println("set_mqtt_pass <pass>       : Set MQTT Password");
            Serial.println("set_mqtt_topic <b_topic>   : Set MQTT Base Topic");
            Serial.println("mqtt_discovery_enable      : Enable MQTT HA Discovery (reboot needed)"); 
            Serial.println("mqtt_discovery_disable     : Disable MQTT HA Discovery (reboot needed)");
            Serial.println("set_mqtt_discovery_prefix <prefix> : Set MQTT Discovery Prefix (reboot needed)");
            Serial.println("ota_update                 : Check for and apply OTA firmware update from GitHub"); 
            Serial.println("view_curve                 : View current fan curve");
            Serial.println("clear_staging_curve        : Clear temporary fan curve for editing");
            Serial.println("stage_curve_point <t> <p>  : Add point (temp pwm%) to staging curve");
            Serial.println("apply_staged_curve         : Apply and save staged fan curve");
            Serial.println("load_default_curve         : Load default fan curve");
            Serial.println("reboot                     : Reboot the ESP32");
            Serial.println("-------------------------------");
        } else if (command.equalsIgnoreCase("status")) {
            Serial.println("--- Current Status ---");
            Serial.printf("Mode: %s\n", isAutoMode ? "AUTO" : "MANUAL");
            Serial.printf("Fan Speed: %d%%\n", fanSpeedPercentage);
            Serial.printf("Temperature: %.1f C %s\n", tempSensorFound ? currentTemperature : -999.0, tempSensorFound ? "" : "(N/A)");
            Serial.printf("Fan RPM: %d\n", fanRpm);
            Serial.printf("WiFi Enabled: %s\n", isWiFiEnabled ? "Yes" : "No");
            if (isWiFiEnabled) {
                Serial.printf("WiFi Status: %s\n", WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected/Connecting");
                if (WiFi.status() == WL_CONNECTED) {
                    Serial.print("IP Address: "); Serial.println(WiFi.localIP());
                }
                Serial.print("Configured SSID: "); Serial.println(current_ssid);
            }
            Serial.printf("MQTT Enabled: %s\n", isMqttEnabled ? "Yes" : "No");
            if (isMqttEnabled) {
                Serial.printf("MQTT Server: %s:%d\n", mqttServer, mqttPort);
                Serial.printf("MQTT User: %s\n", strlen(mqttUser) > 0 ? mqttUser : "N/A");
                Serial.printf("MQTT Base Topic: %s\n", mqttBaseTopic);
                Serial.printf("MQTT Connected: %s\n", mqttClient.connected() ? "Yes" : "No");
                Serial.printf("MQTT Discovery Enabled: %s\n", isMqttDiscoveryEnabled ? "Yes" : "No");
                Serial.printf("MQTT Discovery Prefix: %s\n", mqttDiscoveryPrefix);
            }
            Serial.printf("Reboot Needed: %s\n", rebootNeeded ? "Yes" : "No");
            Serial.printf("OTA Status: %s\n", ota_status_message.c_str());
            Serial.printf("OTA In Progress: %s\n", ota_in_progress ? "Yes" : "No");
            Serial.println("----------------------");
        } else if (command.equalsIgnoreCase("set_mode auto")) {
            isAutoMode = true;
            needsImmediateBroadcast = true; 
            Serial.println("[SERIAL_CMD] Mode set to AUTO.");
        } else if (command.startsWith("set_mode manual ")) {
            int val = command.substring(16).toInt();
            if (val >= 0 && val <= 100) {
                isAutoMode = false;
                manualFanSpeedPercentage = val;
                needsImmediateBroadcast = true;
                Serial.printf("[SERIAL_CMD] Mode set to MANUAL, speed %d%%.\n", val);
            } else {
                Serial.println("[SERIAL_CMD_ERR] Invalid percentage for manual mode (0-100).");
            }
        } else if (command.equalsIgnoreCase("wifi_enable")) {
            if (!isWiFiEnabled) { isWiFiEnabled = true; saveWiFiConfig(); rebootNeeded = true; Serial.println("[SERIAL_CMD] WiFi ENABLED. Reboot required. Type 'reboot'."); } 
            else { Serial.println("[SERIAL_CMD] WiFi is already enabled."); }
        } else if (command.equalsIgnoreCase("wifi_disable")) {
            if (isWiFiEnabled) { isWiFiEnabled = false; saveWiFiConfig(); rebootNeeded = true; Serial.println("[SERIAL_CMD] WiFi DISABLED. Reboot required. Type 'reboot'."); } 
            else { Serial.println("[SERIAL_CMD] WiFi is already disabled."); }
        } else if (command.startsWith("set_ssid ")) {
            String newSsid = command.substring(9); newSsid.trim();
            if (newSsid.length() > 0 && newSsid.length() < sizeof(current_ssid)) { strcpy(current_ssid, newSsid.c_str()); saveWiFiConfig(); Serial.printf("[SERIAL_CMD] SSID set to: '%s'.\n", current_ssid); } 
            else { Serial.println("[SERIAL_CMD_ERR] Invalid SSID length."); }
        } else if (command.startsWith("set_pass ")) {
            String newPass = command.substring(9);
             if (newPass.length() < sizeof(current_password)) { strcpy(current_password, newPass.c_str()); saveWiFiConfig(); Serial.println("[SERIAL_CMD] Password set."); } 
             else { Serial.println("[SERIAL_CMD_ERR] Password too long."); }
        } else if (command.equalsIgnoreCase("connect_wifi")) {
            if (!isWiFiEnabled) { Serial.println("[SERIAL_CMD] Cannot connect, WiFi is disabled. Use 'wifi_enable' then 'reboot'."); } 
            else if (strlen(current_ssid) == 0 || strcmp(current_ssid, "YOUR_WIFI_SSID") == 0) { Serial.println("[SERIAL_CMD] Cannot connect, SSID not configured. Use 'set_ssid'."); } 
            else { Serial.println("[SERIAL_CMD] Attempting WiFi connection..."); attemptWiFiConnection(); if (WiFi.status() == WL_CONNECTED && rebootNeeded) { Serial.println("[SERIAL_CMD] Connection successful. Reboot recommended. Type 'reboot'."); } else if (WiFi.status() != WL_CONNECTED) { Serial.println("[SERIAL_CMD] Connection attempt finished. Check status."); } }
        } else if (command.equalsIgnoreCase("disconnect_wifi")) {
            Serial.println("[SERIAL_CMD] Disconnecting WiFi..."); WiFi.disconnect(true); delay(100); Serial.println("[SERIAL_CMD] WiFi disconnected.");
        } else if (command.equalsIgnoreCase("scan_wifi")) {
            Serial.println("[SERIAL_CMD] Starting WiFi Scan..."); WiFi.disconnect(); delay(100); int n = WiFi.scanNetworks(); Serial.printf("[WiFi_SCAN_SERIAL] Scan found %d networks:\n", n);
            if (n == 0) { Serial.println("  No networks found."); } 
            else { for (int k = 0; k < min(n, 15); ++k) { Serial.printf("  %d: %s (%d dBm) %s\n", k + 1, WiFi.SSID(k).c_str(), WiFi.RSSI(k), WiFi.encryptionType(k) == WIFI_AUTH_OPEN ? " " : "*"); } }
        } 
        else if (command.equalsIgnoreCase("mqtt_enable")) {
            if (!isMqttEnabled) { isMqttEnabled = true; saveMqttConfig(); rebootNeeded = true; Serial.println("[SERIAL_CMD] MQTT ENABLED. Reboot required. Type 'reboot'."); } 
            else { Serial.println("[SERIAL_CMD] MQTT is already enabled."); }
        } else if (command.equalsIgnoreCase("mqtt_disable")) {
            if (isMqttEnabled) { isMqttEnabled = false; saveMqttConfig(); rebootNeeded = true; Serial.println("[SERIAL_CMD] MQTT DISABLED. Reboot required. Type 'reboot'."); } 
            else { Serial.println("[SERIAL_CMD] MQTT is already disabled."); }
        } else if (command.startsWith("set_mqtt_server ")) {
            String val = command.substring(16); val.trim();
            if (val.length() > 0 && val.length() < sizeof(mqttServer)) { strcpy(mqttServer, val.c_str()); saveMqttConfig(); rebootNeeded = true; Serial.printf("[SERIAL_CMD] MQTT Server set to: %s. Reboot needed.\n", mqttServer); } 
            else Serial.println("[SERIAL_CMD_ERR] Invalid MQTT server address length.");
        } else if (command.startsWith("set_mqtt_port ")) {
            int val = command.substring(14).toInt();
            if (val > 0 && val <= 65535) { mqttPort = val; saveMqttConfig(); rebootNeeded = true; Serial.printf("[SERIAL_CMD] MQTT Port set to: %d. Reboot needed.\n", mqttPort); } 
            else Serial.println("[SERIAL_CMD_ERR] Invalid MQTT port (1-65535).");
        } else if (command.startsWith("set_mqtt_user ")) {
            String val = command.substring(14); val.trim();
            if (val.length() < sizeof(mqttUser)) { strcpy(mqttUser, val.c_str()); saveMqttConfig(); rebootNeeded = true; Serial.printf("[SERIAL_CMD] MQTT User set to: %s. Reboot needed.\n", strlen(mqttUser) > 0 ? mqttUser : "N/A"); } 
            else Serial.println("[SERIAL_CMD_ERR] MQTT username too long.");
        } else if (command.startsWith("set_mqtt_pass ")) {
            String val = command.substring(14); 
            if (val.length() < sizeof(mqttPassword)) { strcpy(mqttPassword, val.c_str()); saveMqttConfig(); rebootNeeded = true; Serial.println("[SERIAL_CMD] MQTT Password set. Reboot needed."); } 
            else Serial.println("[SERIAL_CMD_ERR] MQTT password too long.");
        } else if (command.startsWith("set_mqtt_topic ")) {
            String val = command.substring(15); val.trim();
            if (val.length() > 0 && val.length() < sizeof(mqttBaseTopic)) { strcpy(mqttBaseTopic, val.c_str()); saveMqttConfig(); rebootNeeded = true; Serial.printf("[SERIAL_CMD] MQTT Base Topic set to: %s. Reboot needed.\n", mqttBaseTopic); } 
            else Serial.println("[SERIAL_CMD_ERR] Invalid MQTT base topic length.");
        }
        else if (command.equalsIgnoreCase("mqtt_discovery_enable")) {
            if (!isMqttDiscoveryEnabled) { isMqttDiscoveryEnabled = true; saveMqttDiscoveryConfig(); rebootNeeded = true; Serial.println("[SERIAL_CMD] MQTT Discovery ENABLED. Reboot required. Type 'reboot'."); } 
            else { Serial.println("[SERIAL_CMD] MQTT Discovery is already enabled."); }
        } else if (command.equalsIgnoreCase("mqtt_discovery_disable")) {
            if (isMqttDiscoveryEnabled) { isMqttDiscoveryEnabled = false; saveMqttDiscoveryConfig(); rebootNeeded = true; Serial.println("[SERIAL_CMD] MQTT Discovery DISABLED. Reboot required. Type 'reboot'."); } 
            else { Serial.println("[SERIAL_CMD] MQTT Discovery is already disabled."); }
        } else if (command.startsWith("set_mqtt_discovery_prefix ")) {
            String val = command.substring(26); val.trim();
            if (val.length() > 0 && val.length() < sizeof(mqttDiscoveryPrefix)) { strcpy(mqttDiscoveryPrefix, val.c_str()); saveMqttDiscoveryConfig(); rebootNeeded = true; Serial.printf("[SERIAL_CMD] MQTT Discovery Prefix set to: %s. Reboot needed.\n", mqttDiscoveryPrefix); } 
            else { Serial.println("[SERIAL_CMD_ERR] Invalid MQTT Discovery Prefix length."); }
        }
        else if (command.equalsIgnoreCase("view_curve")) {
            Serial.println("--- Current Fan Curve ---");
            if (numCurvePoints == 0) { Serial.println("  No curve points defined."); }
            for (int k = 0; k < numCurvePoints; k++) { Serial.printf("  Point %d: Temp = %d C, PWM = %d%%\n", k, tempPoints[k], pwmPercentagePoints[k]); }
            Serial.println("-------------------------");
        } else if (command.equalsIgnoreCase("clear_staging_curve")) {
            stagingNumCurvePoints = 0; Serial.println("[SERIAL_CMD] Staging fan curve cleared.");
        } else if (command.startsWith("stage_curve_point ")) {
            if (stagingNumCurvePoints >= MAX_CURVE_POINTS) { Serial.println("[SERIAL_CMD_ERR] Max staging curve points reached."); } 
            else { int temp, pwm; if (sscanf(command.c_str(), "stage_curve_point %d %d", &temp, &pwm) == 2) { if (temp >= 0 && temp <= 120 && pwm >= 0 && pwm <= 100) { if (stagingNumCurvePoints > 0 && temp <= stagingTempPoints[stagingNumCurvePoints -1]){ Serial.println("[SERIAL_CMD_ERR] Temperature must be greater than previous point."); } else { stagingTempPoints[stagingNumCurvePoints] = temp; stagingPwmPercentagePoints[stagingNumCurvePoints] = pwm; stagingNumCurvePoints++; Serial.printf("[SERIAL_CMD] Staged point %d: Temp=%d, PWM=%d. Total: %d\n", stagingNumCurvePoints -1, temp, pwm, stagingNumCurvePoints); } } else { Serial.println("[SERIAL_CMD_ERR] Invalid temp (0-120) or PWM (0-100)."); } } else { Serial.println("[SERIAL_CMD_ERR] Format: stage_curve_point <temp> <pwm%>"); } }
        } else if (command.equalsIgnoreCase("apply_staged_curve")) {
            if (stagingNumCurvePoints < 2) { Serial.println("[SERIAL_CMD_ERR] Need at least 2 points."); } 
            else { numCurvePoints = stagingNumCurvePoints; for (int k = 0; k < numCurvePoints; k++) { tempPoints[k] = stagingTempPoints[k]; pwmPercentagePoints[k] = stagingPwmPercentagePoints[k]; } saveFanCurveToNVS(); stagingNumCurvePoints = 0; needsImmediateBroadcast = true; fanCurveChanged = true; Serial.println("[SERIAL_CMD] Staged fan curve applied and saved."); }
        } else if (command.equalsIgnoreCase("load_default_curve")) {
            setDefaultFanCurve(); saveFanCurveToNVS(); needsImmediateBroadcast = true; fanCurveChanged = true; Serial.println("[SERIAL_CMD] Default fan curve loaded and saved.");
        }
         else if (command.equalsIgnoreCase("reboot")) {
            Serial.println("[SERIAL_CMD] Rebooting device now..."); delay(100); ESP.restart();
        }
        else if (command.equalsIgnoreCase("ota_update")) { 
            if (ota_in_progress) {
                Serial.println("[SERIAL_CMD_ERR] OTA update is already in progress.");
            } else if (!isWiFiEnabled || WiFi.status() != WL_CONNECTED) {
                Serial.println("[SERIAL_CMD_ERR] WiFi must be enabled and connected to perform OTA update.");
            }
            else {
                Serial.println("[SERIAL_CMD] Starting OTA update check...");
                triggerOTAUpdateCheck(); 
            }
        }
         else {
            Serial.println("[SERIAL_CMD_ERR] Unknown command. Type 'help' for a list of commands.");
        }
    }
}

// --- WiFi Menu Helper Functions (called by input_handler) ---
void performWiFiScan() { 
    if(serialDebugEnabled) Serial.println("[WiFi_Util] Starting WiFi Scan for LCD Menu...");
    scanResultCount = -1; 
    if(isInMenuMode) displayMenu(); 
    
    WiFi.disconnect(); 
    delay(100);
    int n = WiFi.scanNetworks(); 
    if(serialDebugEnabled) Serial.printf("[WiFi_Util] Scan found %d networks.\n", n);

    if (n == 0) {
        scanResultCount = 0;
    } else {
        scanResultCount = min(n, 10); 
        for (int k = 0; k < scanResultCount; ++k) { // Changed i to k
            scannedSSIDs[k] = WiFi.SSID(k);
        }
    }
    selectedMenuItem = 0; 
    if(isInMenuMode) displayMenu(); 
}

void attemptWiFiConnection() { 
    if (!isWiFiEnabled) {
        if(serialDebugEnabled) Serial.println("[WiFi_Util] Attempt connection: WiFi is disabled by user setting.");
        if(isInMenuMode) { lcd.clear(); lcd.print("WiFi Disabled!"); delay(1500); currentMenuScreen = WIFI_SETTINGS; selectedMenuItem = 0; displayMenu();}
        return;
    }
    if (strlen(current_ssid) == 0 || strcmp(current_ssid, "YOUR_WIFI_SSID") == 0) {
        if(serialDebugEnabled) Serial.println("[WiFi_Util] Attempt connection: SSID not set or is default.");
        if(isInMenuMode) { lcd.clear(); lcd.print("SSID Not Set!"); delay(1500); currentMenuScreen = WIFI_SETTINGS; selectedMenuItem = 2; displayMenu();}
        return;
    }

    if(serialDebugEnabled) Serial.printf("[WiFi_Util] Attempting to connect to SSID: %s\n", current_ssid);
    if(isInMenuMode) { lcd.clear(); lcd.print("Connecting to:"); lcd.setCursor(0,1); lcd.print(String(current_ssid).substring(0,16));} 
    
    WiFi.disconnect(true); 
    delay(100);
    WiFi.begin(current_ssid, current_password);
    
    int timeout = 0;
    while(WiFi.status() != WL_CONNECTED && timeout < 30) { 
        delay(500);
        if(serialDebugEnabled) Serial.print("."); 
        if(isInMenuMode) { /* could update LCD with dots */ }
        timeout++;
    }

    if(WiFi.status() == WL_CONNECTED) {
        if(serialDebugEnabled) Serial.println("\n[WiFi_Util] Connection successful!");
        if(isInMenuMode) {lcd.clear(); lcd.print("Connected!"); lcd.setCursor(0,1); lcd.print(WiFi.localIP());}
        rebootNeeded = true; 
        if(isInMenuMode) {currentMenuScreen = CONFIRM_REBOOT; selectedMenuItem = 0;}
    } else {
        if(serialDebugEnabled) Serial.println("\n[WiFi_Util] Connection failed.");
        if(isInMenuMode) {lcd.clear(); lcd.print("Connect Failed"); delay(2000); currentMenuScreen = WIFI_SETTINGS; selectedMenuItem = 0; }
    }
    if(isInMenuMode) displayMenu(); // Update menu after attempt
}

void disconnectWiFi(){ 
    if(serialDebugEnabled) Serial.println("[WiFi_Util] Disconnecting WiFi via menu/serial...");
    WiFi.disconnect(true); 
    delay(100); 
    if(isInMenuMode) {lcd.clear(); lcd.print("WiFi Dscnnctd"); delay(1000); currentMenuScreen = WIFI_SETTINGS; selectedMenuItem = 0; displayMenu();}
    else if(serialDebugEnabled) {Serial.println("[WiFi_Util] Disconnected.");}
}
