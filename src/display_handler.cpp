#include "display_handler.h"
#include "config.h" // For global variables and lcd object

void updateLCD_NormalMode() { 
    lcd.clear();
    lcd.setCursor(0, 0);
    String line0 = "";
    line0 += (isAutoMode ? "AUTO" : "MANUAL");
    
    // WiFi Status
    if (isWiFiEnabled && WiFi.status() == WL_CONNECTED) {
        String ipStr = WiFi.localIP().toString();
        // Try to fit IP, otherwise show "WiFi ON"
        if (line0.length() + ipStr.length() + 1 <= 16) { // +1 for space
             line0 += " " + ipStr;
        } else if (line0.length() + 8 <= 16) { // "WiFi ON M"
             line0 += " WiFi ON";
        } else {
            // Truncate mode if necessary to show WiFi ON
            if (line0.length() > 8) line0 = line0.substring(0,7) + ".";
            line0 += " WiFi ON";
        }
    } else if (isWiFiEnabled) {
        if (line0.length() + 8 <= 16) line0 += " WiFi..."; // Connecting
        else { if (line0.length() > 7) line0 = line0.substring(0,6) + "."; line0 += " WiFi...";}
    } else { // WiFi Disabled
        if (line0.length() + 9 <= 16) line0 += " WiFi OFF";
        else { if (line0.length() > 6) line0 = line0.substring(0,5) + "."; line0 += " WiFi OFF";}
    }

    // MQTT Status Suffix (if space)
    if (isMqttEnabled && WiFi.status() == WL_CONNECTED) {
        if (line0.length() + 2 <= 16) { // Add " M" for MQTT connected
            line0 += (mqttClient.connected() ? " M" : " m"); // M=Connected, m=Attempting/Enabled
        }
    }
    lcd.print(line0.substring(0,16)); // Ensure it's not over length
    
    lcd.setCursor(0, 1);
    String line1 = "T:";
    if (!tempSensorFound || currentTemperature <= -990.0) { 
        line1 += "N/A "; 
    } else {
        line1 += String(currentTemperature, 1); // "T:25.5"
    }
    
    // Fan Speed Percentage
    line1 += " F:"; // "T:25.5 F:"
    if(fanSpeedPercentage < 10) line1 += " ";   // "T:25.5 F:  5%"
    if(fanSpeedPercentage < 100) line1 += " ";  // "T:25.5 F: 50%"
    line1 += String(fanSpeedPercentage);
    line1 += "%"; // "T:25.5 F:100%"

    // RPM (if space)
    String rpmStr;
    if (fanRpm > 0) {
        if (fanRpm >= 1000) {
            rpmStr = String(fanRpm / 1000.0, 1) + "K"; // e.g., "1.2K"
        } else {
            rpmStr = String(fanRpm); // e.g., "800"
        }
        if (line1.length() + 1 + rpmStr.length() <= 16) { // +1 for "R"
             line1 += "R"; 
             line1 += rpmStr;
        }
    }
    lcd.print(line1.substring(0,16)); 
}

void displayMenu() {
    lcd.clear();
    switch (currentMenuScreen) {
        case MAIN_MENU:             displayMainMenu(); break;
        case WIFI_SETTINGS:         displayWiFiSettingsMenu(); break;
        case WIFI_SCAN:             displayWiFiScanMenu(); break;
        case WIFI_PASSWORD_ENTRY:   displayPasswordEntryMenu(); break; 
        case WIFI_STATUS:           displayWiFiStatusMenu(); break; 
        case MQTT_SETTINGS:         displayMqttSettingsMenu(); break;
        case MQTT_SERVER_ENTRY:     displayMqttEntryMenu("MQTT Server:", mqttServer, false); break;
        case MQTT_PORT_ENTRY:       displayMqttEntryMenu("MQTT Port:", String(mqttPort).c_str(), false, true); break;
        case MQTT_USER_ENTRY:       displayMqttEntryMenu("MQTT User:", mqttUser, false); break;
        case MQTT_PASS_ENTRY:       displayMqttEntryMenu("MQTT Pass:", mqttPassword, true); break;
        case MQTT_TOPIC_ENTRY:      displayMqttEntryMenu("MQTT Topic:", mqttBaseTopic, false); break;
        case CONFIRM_REBOOT:        displayConfirmRebootMenu(); break;
        // VIEW_STATUS is handled by exiting menu mode (isInMenuMode = false)
        default: 
            lcd.print("Unknown Menu"); 
            break;
    }
}

void displayMainMenu() {
    // This function assumes MAIN_MENU has 3 items:
    // 0: WiFi Settings
    // 1: MQTT Settings
    // 2: View Status (exits menu)
    // selectedMenuItem will be 0, 1, or 2.

    if (selectedMenuItem == 0) { // WiFi Settings selected
        lcd.setCursor(0,0); lcd.print(">WiFi Settings");
        lcd.setCursor(0,1); lcd.print(" MQTT Settings"); // Show next item for context
    } else if (selectedMenuItem == 1) { // MQTT Settings selected
        lcd.setCursor(0,0); lcd.print(" WiFi Settings"); // Show previous item for context
        lcd.setCursor(0,1); lcd.print(">MQTT Settings");
    } else if (selectedMenuItem == 2) { // View Status selected
        lcd.setCursor(0,0); lcd.print(" MQTT Settings"); // Show previous item for context
        lcd.setCursor(0,1); lcd.print(">View Status");
    }
    // Ensure no reference to MAIN_MENU_ITEMS here
}

void displayWiFiSettingsMenu() {
    // Items: "WiFi:", "Scan Networks", "SSID:", "Password Set", "Connect WiFi", "DisconnectWiFi", "Back to Main"
    const char* items[] = {"WiFi:", "Scan Networks", "SSID:", "Password Set", "Connect WiFi", "DisconnectWiFi", "Back to Main"};
    const int numItems = 7; 

    for (int i = 0; i < 2; ++i) { 
        int itemIndexToDisplay;
        // Logic to display two items on the screen, with the selected one highlighted
        if (i == 0) { // Top line of LCD
            itemIndexToDisplay = selectedMenuItem;
            // If selected is the last item, show the second to last on the top line to keep context
            if (selectedMenuItem == numItems - 1 && numItems > 1) {
                 itemIndexToDisplay = selectedMenuItem - 1;
            }
        } else { // Bottom line of LCD
            itemIndexToDisplay = selectedMenuItem + 1;
            // If selected is the last item, show the last item on the bottom line
            if (selectedMenuItem == numItems - 1 && numItems > 0) {
                 itemIndexToDisplay = selectedMenuItem;
            }
            // If selected is the first item, the top line shows item 0, bottom shows item 1
            if (selectedMenuItem == 0 && numItems > 1) {
                itemIndexToDisplay = 1;
            }
        }
        
        if (numItems == 1 && i == 1) continue; // If only one item, don't try to print on the second line for "next"

        if (itemIndexToDisplay < 0 || itemIndexToDisplay >= numItems) {
            lcd.setCursor(0,i); lcd.print("                "); // Clear line if out of bounds
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

void displayPasswordEntryMenu() { 
    lcd.setCursor(0,0); lcd.print("WiFi Password:");
    String passMask = "";
    for(int i=0; i < passwordCharIndex; ++i) passMask += "*";
    passMask += currentPasswordEditChar; 
    
    if (passwordCharIndex >= sizeof(passwordInputBuffer) - 2) { 
        passMask += " [OK?]";
    }

    lcd.setCursor(0,1); lcd.print(passMask.substring(0,16));
}

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

void displayConfirmRebootMenu() {
    lcd.setCursor(0,0); lcd.print("Reboot needed!");
    String line1 = (selectedMenuItem == 0 ? ">Yes " : " Yes ");
    line1 += (selectedMenuItem == 1 ? ">No" : " No");
    lcd.setCursor(0,1); lcd.print(line1);
}

void displayMqttSettingsMenu() {
    const char* items[] = {"MQTT:", "Server:", "Port:", "User:", "Password:", "Base Topic:", "Back to Main"};
    const int numItems = 7;

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

void displayMqttEntryMenu(const char* prompt, const char* currentValue, bool isPassword, bool isNumericOnly) {
    lcd.setCursor(0,0); lcd.print(String(prompt).substring(0,16));
    
    String valueToShow = "";
    if (isPassword) {
        for(int i=0; i < generalInputCharIndex; ++i) valueToShow += "*";
    } else {
        valueToShow = String(generalInputBuffer).substring(0, generalInputCharIndex);
    }
    valueToShow += currentGeneralEditChar; 
    
    int maxLength = isNumericOnly ? 5 : (sizeof(generalInputBuffer) - 2);
    if (generalInputCharIndex >= maxLength) {
        valueToShow += " [OK?]";
    }

    lcd.setCursor(0,1); lcd.print(valueToShow.substring(0,16));
}
