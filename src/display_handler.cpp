#include "display_handler.h"
#include "config.h" // For global variables and lcd object

/**
 * @brief Renders the two lines of information in normal operating mode (not in menu).
 * 
 * This function clears the LCD and displays the current operating mode (AUTO/MANUAL),
 * WiFi status (IP address, "WiFi ON/OFF/...", MQTT status (M/mD),
 * current temperature, fan speed percentage, and fan RPM.
 * 
 * @note Depends on global flags: `isAutoMode`, `isWiFiEnabled`, `isMqttEnabled`,
 * `isMqttDiscoveryEnabled`, `tempSensorFound`.
 * @note Depends on global variables: `currentTemperature`, `fanSpeedPercentage`, `fanRpm`,
 * `WiFi.status()`, `WiFi.localIP()`, `mqttClient.connected()`.
 * @sideeffect Clears and prints to the global `lcd` object.
 */
void updateLCD_NormalMode() { 
    lcd.clear();
    lcd.setCursor(0, 0);
    String line0 = "";
    line0 += (isAutoMode ? "AUTO" : "MANUAL");
    
    if (isWiFiEnabled && WiFi.status() == WL_CONNECTED) {
        String ipStr = WiFi.localIP().toString();
        if (line0.length() + ipStr.length() + 1 <= 16) { 
             line0 += " " + ipStr;
        } else if (line0.length() + 8 <= 16) { 
             line0 += " WiFi ON";
        } else {
            if (line0.length() > 8) line0 = line0.substring(0,7) + ".";
            line0 += " WiFi ON";
        }
    } else if (isWiFiEnabled) {
        if (line0.length() + 8 <= 16) line0 += " WiFi..."; 
        else { if (line0.length() > 7) line0 = line0.substring(0,6) + "."; line0 += " WiFi...";}
    } else { 
        if (line0.length() + 9 <= 16) line0 += " WiFi OFF";
        else { if (line0.length() > 6) line0 = line0.substring(0,5) + "."; line0 += " WiFi OFF";}
    }

    if (isMqttEnabled && WiFi.status() == WL_CONNECTED) {
        if (line0.length() + 2 <= 16) { 
            line0 += (mqttClient.connected() ? " M" : " m"); 
            if (isMqttDiscoveryEnabled && line0.length() + 1 <= 16){ 
                 line0 += "D";
            }
        }
    }
    lcd.print(line0.substring(0,16)); 
    
    lcd.setCursor(0, 1);
    String line1 = "T:";
    if (!tempSensorFound || currentTemperature <= -990.0) { 
        line1 += "N/A "; 
    } else {
        line1 += String(currentTemperature, 1); 
    }
    
    line1 += " F:"; 
    if(fanSpeedPercentage < 10) line1 += " ";   
    if(fanSpeedPercentage < 100) line1 += " ";  
    line1 += String(fanSpeedPercentage);
    line1 += "%"; 

    String rpmStr;
    if (fanRpm > 0) {
        if (fanRpm >= 1000) {
            rpmStr = String(fanRpm / 1000.0, 1) + "K"; 
        } else {
            rpmStr = String(fanRpm); 
        }
        if (line1.length() + 1 + rpmStr.length() <= 16) { 
             line1 += "R"; 
             line1 += rpmStr;
        }
    }
    lcd.print(line1.substring(0,16)); 
}

/**
 * @brief Dispatches to the appropriate menu display function based on the current menu screen.
 * 
 * This function clears the LCD and then calls the specific display function
 * corresponding to the `currentMenuScreen` global variable.
 * 
 * @note Depends on the global `currentMenuScreen` variable.
 * @sideeffect Clears the global `lcd` object and calls other display functions.
 */
void displayMenu() {
    lcd.clear();
    switch (currentMenuScreen) {
        case MAIN_MENU:             displayMainMenu(); break;
        case WIFI_SETTINGS:         displayWiFiSettingsMenu(); break;
        case WIFI_SCAN:             displayWiFiScanMenu(); break;
        case WIFI_PASSWORD_ENTRY:   displayPasswordEntryMenu(); break; 
        case WIFI_STATUS:           displayWiFiStatusMenu(); break; 
        case MQTT_SETTINGS:         displayMqttSettingsMenu(); break;
        case MQTT_SERVER_ENTRY:     displayMqttEntryMenu("MQTT Server:", mqttServer, false, false, sizeof(mqttServer)-1); break;
        case MQTT_PORT_ENTRY:       displayMqttEntryMenu("MQTT Port:", String(mqttPort).c_str(), false, true, 5); break;
        case MQTT_USER_ENTRY:       displayMqttEntryMenu("MQTT User:", mqttUser, false, false, sizeof(mqttUser)-1); break;
        case MQTT_PASS_ENTRY:       displayMqttEntryMenu("MQTT Pass:", mqttPassword, true, false, sizeof(mqttPassword)-1); break;
        case MQTT_TOPIC_ENTRY:      displayMqttEntryMenu("MQTT Topic:", mqttBaseTopic, false, false, sizeof(mqttBaseTopic)-1); break;
        case MQTT_DISCOVERY_SETTINGS: displayMqttDiscoverySettingsMenu(); break; 
        case MQTT_DISCOVERY_PREFIX_ENTRY: displayMqttEntryMenu("Discovery Pfx:", mqttDiscoveryPrefix, false, false, sizeof(mqttDiscoveryPrefix)-1); break; 
        case OTA_UPDATE_SCREEN:     displayOtaUpdateMenu(); break; // NEW
        case CONFIRM_REBOOT:        displayConfirmRebootMenu(); break;
        default: 
            lcd.print("Unknown Menu"); 
            break;
    }
}

/**
 * @brief Displays the main menu options on the LCD.
 * 
 * The main menu includes options for WiFi Settings, MQTT Settings, OTA Update,
 * and View Status (which exits the menu). The currently selected item
 * (indicated by `selectedMenuItem`) is prefixed with a '>'.
 * 
 * @note Depends on the global `selectedMenuItem` variable.
 * @sideeffect Prints to the global `lcd` object.
 */
void displayMainMenu() {
    // Adjusted for 3 items: WiFi, MQTT, OTA Update, View Status (Exit)
    const char* items[] = {"WiFi Settings", "MQTT Settings", "OTA Update", "View Status"};
    const int numItems = 4;

    if (selectedMenuItem < 2) { // Display first two items
        lcd.setCursor(0,0); lcd.print((selectedMenuItem == 0 ? ">" : " ") + String(items[0]));
        lcd.setCursor(0,1); lcd.print((selectedMenuItem == 1 ? ">" : " ") + String(items[1]));
    } else { // Display items 2 and 3 (OTA and View Status)
        lcd.setCursor(0,0); lcd.print((selectedMenuItem == 2 ? ">" : " ") + String(items[2]));
        lcd.setCursor(0,1); lcd.print((selectedMenuItem == 3 ? ">" : " ") + String(items[3]));
    }
}

/**
 * @brief Displays the WiFi settings menu on the LCD.
 * 
 * This menu allows enabling/disabling WiFi, scanning networks, viewing/setting SSID,
 * setting password, connecting/disconnecting, and returning to the main menu.
 * It shows the current state of `isWiFiEnabled` and `current_ssid`.
 * 
 * @note Depends on global variables: `selectedMenuItem`, `isWiFiEnabled`, `current_ssid`.
 * @sideeffect Prints to the global `lcd` object.
 */
void displayWiFiSettingsMenu() {
    const char* items[] = {"WiFi:", "Scan Networks", "SSID:", "Password Set", "Connect WiFi", "DisconnectWiFi", "Back to Main"};
    const int numItems = 7; 

    for (int i = 0; i < 2; ++i) { 
        int itemIndexToDisplay;
        if (i == 0) { 
            itemIndexToDisplay = selectedMenuItem;
            if (selectedMenuItem == numItems - 1 && numItems > 1) {
                 itemIndexToDisplay = selectedMenuItem - 1;
            }
        } else { 
            itemIndexToDisplay = selectedMenuItem + 1;
            if (selectedMenuItem == numItems - 1 && numItems > 0) {
                 itemIndexToDisplay = selectedMenuItem;
            }
            if (selectedMenuItem == 0 && numItems > 1) {
                itemIndexToDisplay = 1;
            }
        }
        
        if (numItems == 1 && i == 1) continue; 

        if (itemIndexToDisplay < 0 || itemIndexToDisplay >= numItems) {
            lcd.setCursor(0,i); lcd.print("                "); 
            continue; 
        }

        lcd.setCursor(0, i);
        String line = "";
        if (itemIndexToDisplay == selectedMenuItem) line += ">"; else line += " ";

        if (itemIndexToDisplay == 0) { 
            line += items[itemIndexToDisplay]; line += " "; line += (isWiFiEnabled ? "Enabled" : "Disabled");
        } else if (itemIndexToDisplay == 2) { 
            line += items[itemIndexToDisplay]; line += " "; line += String(current_ssid).substring(0, 16 - line.length());
        } else {
            line += items[itemIndexToDisplay];
        }
        lcd.print(line.substring(0,16));
    }
}

/**
 * @brief Displays the WiFi network scan results or a "scanning" message on the LCD.
 * 
 * If a scan is in progress (`scanResultCount == -1`), it shows a "Scanning..." message.
 * If no networks are found (`scanResultCount == 0`), it indicates that.
 * Otherwise, it displays the currently selected network from the `scannedSSIDs` list.
 * 
 * @note Depends on global variables: `scanResultCount`, `selectedMenuItem`, `scannedSSIDs`.
 * @sideeffect Prints to the global `lcd` object.
 */
void displayWiFiScanMenu() {
    lcd.setCursor(0,0);
    if (scanResultCount == -1) { 
        lcd.print("Scanning WiFi..");
        lcd.setCursor(0,1); lcd.print("Please wait...");
        return;
    }
    if (scanResultCount == 0) {
        lcd.print("No Networks Found");
        lcd.setCursor(0,1); lcd.print("Press BACK");
        return;
    }
    lcd.print("Select Network:"); 
    
    lcd.setCursor(0,1);
    if (selectedMenuItem >= 0 && selectedMenuItem < scanResultCount) {
        lcd.print(">"); 
        lcd.print(scannedSSIDs[selectedMenuItem].substring(0,15)); 
    } else if (scanResultCount > 0) { 
        lcd.print(">"); 
        lcd.print(scannedSSIDs[0].substring(0,15)); 
    } else {
        lcd.print(" (No Networks)  "); 
    }
}

/**
 * @brief Displays the WiFi password entry screen on the LCD.
 * 
 * Shows "WiFi Password:" on the first line and the masked password input
 * (asterisks for entered characters, plus the `currentPasswordEditChar`)
 * on the second line. Includes a "[OK?]" prompt when the buffer is nearly full.
 * 
 * @note Depends on global variables: `passwordCharIndex`, `currentPasswordEditChar`,
 * `passwordInputBuffer`.
 * @sideeffect Prints to the global `lcd` object.
 */
void displayPasswordEntryMenu() { 
    lcd.setCursor(0,0); lcd.print("WiFi Password:");
    String passMask = "";
    for(int k=0; k < passwordCharIndex; ++k) passMask += "*"; 
    passMask += currentPasswordEditChar; 
    
    if (passwordCharIndex >= sizeof(passwordInputBuffer) - 2) { 
        passMask += " [OK?]";
    }

    lcd.setCursor(0,1); lcd.print(passMask.substring(0,16));
}

/**
 * @brief Displays the current WiFi connection status on the LCD.
 * 
 * Shows whether WiFi is enabled, and if so, its connection status (connected/connecting)
 * and the assigned IP address if connected.
 * 
 * @note Depends on global variables: `isWiFiEnabled`, `WiFi.status()`, `WiFi.localIP()`.
 * @sideeffect Prints to the global `lcd` object.
 */
void displayWiFiStatusMenu(){ 
    lcd.setCursor(0,0); lcd.print("WiFi: "); lcd.print(isWiFiEnabled ? "ON" : "OFF");
    lcd.setCursor(0,1);
    if(isWiFiEnabled && WiFi.status() == WL_CONNECTED){
        lcd.print(WiFi.localIP());
    } else if (isWiFiEnabled) {
        lcd.print("Connecting...");
    } else {
        lcd.print("Disabled.");
    }
}

/**
 * @brief Displays a confirmation screen for rebooting the device.
 * 
 * Presents "Reboot needed!" on the first line and "Yes" / "No" options
 * on the second line, with the `selectedMenuItem` indicating the chosen option.
 * 
 * @note Depends on the global `selectedMenuItem` variable.
 * @sideeffect Prints to the global `lcd` object.
 */
void displayConfirmRebootMenu() {
    lcd.setCursor(0,0); lcd.print("Reboot needed!");
    String line1 = (selectedMenuItem == 0 ? ">Yes " : " Yes ");
    line1 += (selectedMenuItem == 1 ? ">No" : " No");
    lcd.setCursor(0,1); lcd.print(line1);
}

/**
 * @brief Displays the MQTT settings menu on the LCD.
 * 
 * This menu allows enabling/disabling MQTT, setting broker server, port,
 * username, password, base topic, and navigating to discovery settings.
 * It shows the current state of `isMqttEnabled` and other MQTT configuration variables.
 * 
 * @note Depends on global variables: `selectedMenuItem`, `isMqttEnabled`, `mqttServer`,
 * `mqttPort`, `mqttUser`, `mqttPassword`, `mqttBaseTopic`.
 * @sideeffect Prints to the global `lcd` object.
 */
void displayMqttSettingsMenu() {
    const char* items[] = {"MQTT:", "Server:", "Port:", "User:", "Password:", "Base Topic:", "Discovery Cfg", "Back to Main"};
    const int numItems = 8; 

    for (int i = 0; i < 2; ++i) { 
        int itemIndexToDisplay;
        if (i == 0) { 
            itemIndexToDisplay = selectedMenuItem;
            if (selectedMenuItem == numItems - 1 && numItems > 1) itemIndexToDisplay = selectedMenuItem - 1; 
        } else { 
            itemIndexToDisplay = selectedMenuItem + 1;
            if (selectedMenuItem == numItems - 1 && numItems > 0) itemIndexToDisplay = selectedMenuItem; 
            if (selectedMenuItem == 0 && numItems > 1) itemIndexToDisplay = selectedMenuItem +1; 
        }
        
        if (numItems == 1 && i == 1) continue;

        if (itemIndexToDisplay < 0 || itemIndexToDisplay >= numItems) {
            lcd.setCursor(0,i); lcd.print("                ");
            continue; 
        }

        lcd.setCursor(0, i);
        String line = "";
        if (itemIndexToDisplay == selectedMenuItem) line += ">"; else line += " ";

        if (itemIndexToDisplay == 0) { 
            line += items[itemIndexToDisplay]; line += " "; line += (isMqttEnabled ? "Enabled" : "Disabled");
        } else if (itemIndexToDisplay == 1) { 
            line += items[itemIndexToDisplay]; line += " "; line += String(mqttServer).substring(0, 16 - line.length());
        } else if (itemIndexToDisplay == 2) { 
            line += items[itemIndexToDisplay]; line += " "; line += String(mqttPort);
        } else if (itemIndexToDisplay == 3) { 
            line += items[itemIndexToDisplay]; line += " "; line += String(mqttUser).substring(0, 16 - line.length());
        } else if (itemIndexToDisplay == 4) { 
            line += items[itemIndexToDisplay]; line += " "; 
            for(int k=0; k < strlen(mqttPassword) && line.length() < 15; ++k) line += "*";
        } else if (itemIndexToDisplay == 5) { 
            line += items[itemIndexToDisplay]; line += " "; line += String(mqttBaseTopic).substring(0, 16 - line.length());
        }
         else { 
            line += items[itemIndexToDisplay]; 
        }
        lcd.print(line.substring(0,16));
    }
}

/**
 * @brief Displays a generic input screen for various MQTT settings.
 * 
 * This function is used for entering text or numeric values for MQTT server,
 * port, user, password, base topic, and discovery prefix. It shows a prompt
 * on the first line and the currently entered characters (masked if `isPassword`)
 * on the second line.
 * 
 * @param prompt The text prompt to display on the first line (e.g., "MQTT Server:").
 * @param currentValue The current value of the field being edited. This parameter
 *                     is primarily used for initialization in `input_handler.cpp`
 *                     and not directly for display within this function, which
 *                     uses `generalInputBuffer`.
 * @param isPassword True if the input should be masked with asterisks.
 * @param isNumericOnly True if only numeric characters are expected (affects character cycling in `input_handler`).
 * @param maxLength The maximum allowed length for the input string.
 * 
 * @note Depends on global variables: `generalInputCharIndex`, `currentGeneralEditChar`,
 * `generalInputBuffer`.
 * @sideeffect Prints to the global `lcd` object.
 */
void displayMqttEntryMenu(const char* prompt, const char* currentValue, bool isPassword, bool isNumericOnly, int maxLength) {
    lcd.setCursor(0,0); lcd.print(String(prompt).substring(0,16));
    
    String valueToShow = "";
    if (isPassword) {
        for(int k=0; k < generalInputCharIndex; ++k) valueToShow += "*"; 
    } else {
        for(int k=0; k<generalInputCharIndex; ++k) valueToShow += generalInputBuffer[k];
    }
    valueToShow += currentGeneralEditChar; 
    
    if (generalInputCharIndex >= maxLength -1 ) { 
        valueToShow += " [OK?]";
    }

    lcd.setCursor(0,1); lcd.print(valueToShow.substring(0,16));
}

/**
 * @brief Displays the MQTT Home Assistant Discovery settings menu on the LCD.
 * 
 * This menu allows enabling/disabling MQTT Discovery and setting the discovery prefix.
 * It shows the current state of `isMqttDiscoveryEnabled` and `mqttDiscoveryPrefix`.
 * 
 * @note Depends on global variables: `selectedMenuItem`, `isMqttDiscoveryEnabled`,
 * `mqttDiscoveryPrefix`.
 * @sideeffect Prints to the global `lcd` object.
 */
void displayMqttDiscoverySettingsMenu() {
    const char* items[] = {"Discovery:", "Prefix:", "Back"};
    const int numItems = 3;

    for (int i = 0; i < 2; ++i) { 
        int itemIndexToDisplay;
        if (i == 0) { 
            itemIndexToDisplay = selectedMenuItem;
            if (selectedMenuItem == numItems - 1 && numItems > 1) itemIndexToDisplay = selectedMenuItem - 1; 
        } else { 
            itemIndexToDisplay = selectedMenuItem + 1;
            if (selectedMenuItem == numItems - 1 && numItems > 0) itemIndexToDisplay = selectedMenuItem; 
            if (selectedMenuItem == 0 && numItems > 1) itemIndexToDisplay = selectedMenuItem +1; 
        }
        
        if (numItems == 1 && i == 1) continue;

        if (itemIndexToDisplay < 0 || itemIndexToDisplay >= numItems) {
            lcd.setCursor(0,i); lcd.print("                ");
            continue; 
        }

        lcd.setCursor(0, i);
        String line = "";
        if (itemIndexToDisplay == selectedMenuItem) line += ">"; else line += " ";

        if (itemIndexToDisplay == 0) { 
            line += items[itemIndexToDisplay]; line += " "; line += (isMqttDiscoveryEnabled ? "Enabled" : "Disabled");
        } else if (itemIndexToDisplay == 1) { 
            line += items[itemIndexToDisplay]; line += " "; line += String(mqttDiscoveryPrefix).substring(0, 16 - line.length());
        } else { 
            line += items[itemIndexToDisplay];
        }
        lcd.print(line.substring(0,16));
    }
}

/**
 * @brief Displays the OTA (Over-The-Air) update screen on the LCD.
 * 
 * If an OTA update is in progress, it shows "OTA In Progress:" and the
 * `ota_status_message`. Otherwise, it displays "Firmware Update" and
 * options to "Check & Update" or "Back to Main", or the last status message.
 * 
 * @note Depends on global variables: `ota_in_progress`, `ota_status_message`,
 * `selectedMenuItem`.
 * @sideeffect Prints to the global `lcd` object.
 */
void displayOtaUpdateMenu() {
    lcd.setCursor(0, 0);
    if (ota_in_progress) {
        lcd.print("OTA In Progress:");
        lcd.setCursor(0, 1);
        lcd.print(ota_status_message.substring(0, 16));
    } else {
        lcd.print("Firmware Update");
        lcd.setCursor(0, 1);
        if (selectedMenuItem == 0) {
            lcd.print(">Check & Update");
        } else if (selectedMenuItem == 1) {
            lcd.print(">Back to Main");
        } else { // Default or when status is shown
             lcd.print(ota_status_message.substring(0,16));
        }
    }
}
