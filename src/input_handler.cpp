#include "input_handler.h"
#include "config.h"         
#include "nvs_handler.h"    
#include "display_handler.h"
#include "fan_control.h" // Added for setDefaultFanCurve

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

            if (i == 0) { 
                isInMenuMode = !isInMenuMode;
                if (isInMenuMode) {
                    if(serialDebugEnabled) Serial.println("[MENU_LCD] Entered Menu Mode.");
                    currentMenuScreen = MAIN_MENU; 
                    selectedMenuItem = 0;
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

            switch(i) { 
                case 1: // UP
                    if (currentMenuScreen == WIFI_PASSWORD_ENTRY) {
                        currentPasswordEditChar++;
                        if (currentPasswordEditChar > '~') currentPasswordEditChar = ' '; 
                    } else if (selectedMenuItem > 0) {
                        selectedMenuItem--;
                    }
                    break;
                case 2: // DOWN
                    if (currentMenuScreen == WIFI_PASSWORD_ENTRY) {
                        currentPasswordEditChar--;
                        if (currentPasswordEditChar < ' ') currentPasswordEditChar = '~';
                    } else {
                        int maxItems = 0;
                        if (currentMenuScreen == MAIN_MENU) maxItems = 1;
                        else if (currentMenuScreen == WIFI_SETTINGS) maxItems = 6; 
                        else if (currentMenuScreen == WIFI_SCAN) maxItems = scanResultCount > 0 ? scanResultCount - 1 : 0;
                        else if (currentMenuScreen == CONFIRM_REBOOT) maxItems = 1;
                        if (selectedMenuItem < maxItems) selectedMenuItem++;
                    }
                    break;
                case 3: // SELECT
                    if(serialDebugEnabled) Serial.printf("[MENU_LCD_ACTION] Screen: %d, Item: %d\n", currentMenuScreen, selectedMenuItem);
                    if (currentMenuScreen == MAIN_MENU) {
                        if (selectedMenuItem == 0) { currentMenuScreen = WIFI_SETTINGS; selectedMenuItem = 0; }
                        else if (selectedMenuItem == 1) { isInMenuMode = false; if(rebootNeeded){currentMenuScreen = CONFIRM_REBOOT; isInMenuMode=true; selectedMenuItem=0;}}
                    } else if (currentMenuScreen == WIFI_SETTINGS) {
                        if (selectedMenuItem == 0) { 
                            isWiFiEnabled = !isWiFiEnabled;
                            saveWiFiConfig();
                            rebootNeeded = true;
                            currentMenuScreen = CONFIRM_REBOOT; selectedMenuItem = 0;
                        } else if (selectedMenuItem == 1) { 
                            currentMenuScreen = WIFI_SCAN; selectedMenuItem = 0; performWiFiScan();
                        } else if (selectedMenuItem == 2) { 
                            currentMenuScreen = WIFI_SCAN; selectedMenuItem = 0; performWiFiScan(); 
                        } else if (selectedMenuItem == 3) { 
                            passwordCharIndex = 0; currentPasswordEditChar = 'a'; memset(passwordInputBuffer, 0, sizeof(passwordInputBuffer));
                            currentMenuScreen = WIFI_PASSWORD_ENTRY; selectedMenuItem = 0;
                        } else if (selectedMenuItem == 4) { 
                            attemptWiFiConnection(); 
                        } else if (selectedMenuItem == 5) { 
                            disconnectWiFi();
                        } else if (selectedMenuItem == 6) { 
                            currentMenuScreen = MAIN_MENU; selectedMenuItem = 0;
                        }
                    } else if (currentMenuScreen == WIFI_SCAN) {
                        if (scanResultCount > 0 && selectedMenuItem < scanResultCount) {
                            strncpy(current_ssid, scannedSSIDs[selectedMenuItem].c_str(), sizeof(current_ssid) - 1);
                            current_ssid[sizeof(current_ssid)-1] = '\0';
                            if(serialDebugEnabled) Serial.printf("[MENU_LCD] SSID Selected: %s\n", current_ssid);
                            currentMenuScreen = WIFI_SETTINGS; selectedMenuItem = 3; 
                        }
                    } else if (currentMenuScreen == WIFI_PASSWORD_ENTRY) { 
                        if (passwordCharIndex < sizeof(passwordInputBuffer) - 2) {
                            passwordInputBuffer[passwordCharIndex] = currentPasswordEditChar;
                            passwordCharIndex++;
                            currentPasswordEditChar = 'a'; 
                        } else { 
                             strncpy(current_password, passwordInputBuffer, sizeof(current_password) -1);
                             current_password[sizeof(current_password)-1] = '\0';
                             if(serialDebugEnabled) Serial.printf("[MENU_LCD] Password Entered (length %d)\n", strlen(current_password));
                             saveWiFiConfig(); 
                             currentMenuScreen = WIFI_SETTINGS; selectedMenuItem = 4; 
                        }
                    } else if (currentMenuScreen == CONFIRM_REBOOT) {
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
                     if (currentMenuScreen == WIFI_PASSWORD_ENTRY) { 
                        if (passwordCharIndex > 0) {
                            passwordCharIndex--;
                            passwordInputBuffer[passwordCharIndex] = '\0';
                            currentPasswordEditChar = 'a'; 
                        } else { 
                            currentMenuScreen = WIFI_SETTINGS; selectedMenuItem = 3;
                        }
                    } else if (currentMenuScreen == WIFI_SETTINGS || currentMenuScreen == WIFI_STATUS) {
                        currentMenuScreen = MAIN_MENU; selectedMenuItem = 0;
                    } else if (currentMenuScreen == WIFI_SCAN) {
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

// --- Serial Command Handling ---
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
            if (!isWiFiEnabled) {
                isWiFiEnabled = true;
                saveWiFiConfig();
                rebootNeeded = true;
                Serial.println("[SERIAL_CMD] WiFi ENABLED. Reboot required to start network services. Type 'reboot'.");
            } else {
                Serial.println("[SERIAL_CMD] WiFi is already enabled.");
            }
        } else if (command.equalsIgnoreCase("wifi_disable")) {
            if (isWiFiEnabled) {
                isWiFiEnabled = false;
                saveWiFiConfig();
                rebootNeeded = true;
                Serial.println("[SERIAL_CMD] WiFi DISABLED. Reboot required to stop network services. Type 'reboot'.");
            } else {
                Serial.println("[SERIAL_CMD] WiFi is already disabled.");
            }
        } else if (command.startsWith("set_ssid ")) {
            String newSsid = command.substring(9);
            newSsid.trim();
            if (newSsid.length() > 0 && newSsid.length() < sizeof(current_ssid)) {
                strcpy(current_ssid, newSsid.c_str());
                saveWiFiConfig();
                Serial.printf("[SERIAL_CMD] SSID set to: '%s'.\n", current_ssid);
            } else {
                Serial.println("[SERIAL_CMD_ERR] Invalid SSID length.");
            }
        } else if (command.startsWith("set_pass ")) {
            String newPass = command.substring(9);
             if (newPass.length() < sizeof(current_password)) { 
                strcpy(current_password, newPass.c_str());
                saveWiFiConfig();
                Serial.println("[SERIAL_CMD] Password set.");
            } else {
                 Serial.println("[SERIAL_CMD_ERR] Password too long.");
            }
        } else if (command.equalsIgnoreCase("connect_wifi")) {
            if (!isWiFiEnabled) {
                Serial.println("[SERIAL_CMD] Cannot connect, WiFi is currently disabled by setting. Use 'wifi_enable' then 'reboot'.");
            } else if (strlen(current_ssid) == 0 || strcmp(current_ssid, "YOUR_WIFI_SSID") == 0) {
                Serial.println("[SERIAL_CMD] Cannot connect, SSID not configured. Use 'set_ssid'.");
            } else {
                Serial.println("[SERIAL_CMD] Attempting WiFi connection (via Serial command)... this will set rebootNeeded if successful.");
                attemptWiFiConnection(); 
                if (WiFi.status() == WL_CONNECTED && rebootNeeded) {
                    Serial.println("[SERIAL_CMD] Connection successful. Reboot is recommended to ensure network services run correctly. Type 'reboot'.");
                } else if (WiFi.status() != WL_CONNECTED) {
                    Serial.println("[SERIAL_CMD] Connection attempt finished. Check status. If settings changed, reboot might be needed.");
                }
            }
        } else if (command.equalsIgnoreCase("disconnect_wifi")) {
            Serial.println("[SERIAL_CMD] Disconnecting WiFi...");
            WiFi.disconnect(true);
            Serial.println("[SERIAL_CMD] WiFi disconnected (if it was connected). Note: This doesn't disable WiFi. Use 'wifi_disable' for that.");
        } else if (command.equalsIgnoreCase("scan_wifi")) {
            Serial.println("[SERIAL_CMD] Starting WiFi Scan...");
            WiFi.disconnect(); 
            delay(100);
            int n = WiFi.scanNetworks();
            Serial.printf("[WiFi_SCAN_SERIAL] Scan found %d networks:\n", n);
            if (n == 0) {
                Serial.println("  No networks found.");
            } else {
                for (int i = 0; i < min(n, 15); ++i) { 
                    Serial.printf("  %d: %s (%d dBm) %s\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? " " : "*");
                }
            }
        } else if (command.equalsIgnoreCase("view_curve")) {
            Serial.println("--- Current Fan Curve ---");
            if (numCurvePoints == 0) {
                Serial.println("  No curve points defined.");
            }
            for (int i = 0; i < numCurvePoints; i++) {
                Serial.printf("  Point %d: Temp = %d C, PWM = %d%%\n", i, tempPoints[i], pwmPercentagePoints[i]);
            }
            Serial.println("-------------------------");
        } else if (command.equalsIgnoreCase("clear_staging_curve")) {
            stagingNumCurvePoints = 0;
            Serial.println("[SERIAL_CMD] Staging fan curve cleared.");
        } else if (command.startsWith("stage_curve_point ")) {
            if (stagingNumCurvePoints >= MAX_CURVE_POINTS) {
                Serial.println("[SERIAL_CMD_ERR] Max staging curve points reached. Apply or clear first.");
            } else {
                int temp, pwm;
                if (sscanf(command.c_str(), "stage_curve_point %d %d", &temp, &pwm) == 2) {
                    if (temp >= 0 && temp <= 120 && pwm >= 0 && pwm <= 100) {
                        if (stagingNumCurvePoints > 0 && temp <= stagingTempPoints[stagingNumCurvePoints -1]){
                             Serial.println("[SERIAL_CMD_ERR] Temperature must be greater than previous point.");
                        } else {
                            stagingTempPoints[stagingNumCurvePoints] = temp;
                            stagingPwmPercentagePoints[stagingNumCurvePoints] = pwm;
                            stagingNumCurvePoints++;
                            Serial.printf("[SERIAL_CMD] Staged point %d: Temp=%d, PWM=%d. Total staged: %d\n", stagingNumCurvePoints -1, temp, pwm, stagingNumCurvePoints);
                        }
                    } else {
                        Serial.println("[SERIAL_CMD_ERR] Invalid temp (0-120) or PWM (0-100) value.");
                    }
                } else {
                    Serial.println("[SERIAL_CMD_ERR] Invalid format. Use: stage_curve_point <temp> <pwm_percent>");
                }
            }
        } else if (command.equalsIgnoreCase("apply_staged_curve")) {
            if (stagingNumCurvePoints < 2) {
                Serial.println("[SERIAL_CMD_ERR] Need at least 2 points to apply a curve.");
            } else {
                numCurvePoints = stagingNumCurvePoints;
                for (int i = 0; i < numCurvePoints; i++) {
                    tempPoints[i] = stagingTempPoints[i];
                    pwmPercentagePoints[i] = stagingPwmPercentagePoints[i];
                }
                saveFanCurveToNVS();
                stagingNumCurvePoints = 0; 
                needsImmediateBroadcast = true; 
                Serial.println("[SERIAL_CMD] Staged fan curve applied and saved.");
            }
        } else if (command.equalsIgnoreCase("load_default_curve")) {
            setDefaultFanCurve(); // This is now declared in fan_control.h
            saveFanCurveToNVS(); 
            needsImmediateBroadcast = true;
            Serial.println("[SERIAL_CMD] Default fan curve loaded and saved.");
        }
         else if (command.equalsIgnoreCase("reboot")) {
            Serial.println("[SERIAL_CMD] Rebooting device now...");
            delay(100);
            ESP.restart();
        } else {
            Serial.println("[SERIAL_CMD_ERR] Unknown command. Type 'help' for a list of commands.");
        }
    }
}

// --- WiFi Menu Helper Functions (called by input_handler) ---
void performWiFiScan() { // Renamed from menu-specific to general
    if(serialDebugEnabled) Serial.println("[WiFi_Util] Starting WiFi Scan...");
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
        for (int i = 0; i < scanResultCount; ++i) {
            scannedSSIDs[i] = WiFi.SSID(i);
            if(serialDebugEnabled && isInMenuMode) { 
                 Serial.printf("  LCD Menu Scan %d: %s (%d)\n", i + 1, scannedSSIDs[i].c_str(), WiFi.RSSI(i));
            }
        }
    }
    selectedMenuItem = 0; 
    if(isInMenuMode) displayMenu(); 
}

void attemptWiFiConnection() { // Renamed from menu-specific to general
    if (!isWiFiEnabled) {
        if(serialDebugEnabled) Serial.println("[WiFi_Util] Attempt connection: WiFi is disabled by user.");
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
        timeout++;
    }
    if(WiFi.status() == WL_CONNECTED) {
        if(serialDebugEnabled) Serial.println("\n[WiFi_Util] Connection successful!");
        if(isInMenuMode) {lcd.clear(); lcd.print("Connected!"); lcd.setCursor(0,1); lcd.print(WiFi.localIP());}
        saveWiFiConfig(); 
        rebootNeeded = true; 
        if(isInMenuMode) {currentMenuScreen = CONFIRM_REBOOT; selectedMenuItem = 0; displayMenu();}
    } else {
        if(serialDebugEnabled) Serial.println("\n[WiFi_Util] Connection failed.");
        if(isInMenuMode) {lcd.clear(); lcd.print("Connect Failed"); currentMenuScreen = WIFI_SETTINGS; selectedMenuItem = 0; displayMenu();}
    }
    if(isInMenuMode) delay(2000); 
}

void disconnectWiFi(){ // Renamed from menu-specific to general
    if(serialDebugEnabled) Serial.println("[WiFi_Util] Disconnecting WiFi...");
    WiFi.disconnect(true);
    delay(100);
    if(isInMenuMode) {lcd.clear(); lcd.print("WiFi Dscnnctd"); delay(1000); displayMenu();}
    else if(serialDebugEnabled) {Serial.println("[WiFi_Util] Disconnected.");}
}
