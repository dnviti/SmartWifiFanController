#include "display_handler.h"
#include "config.h" // For global variables and lcd object

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
    for(int k=0; k < passwordCharIndex; ++k) passMask += "*"; 
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

// New function to display OTA Update screen
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
