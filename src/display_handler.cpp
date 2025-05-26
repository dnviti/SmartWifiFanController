#include "display_handler.h"
#include "config.h" // For global variables and lcd object

void updateLCD_NormalMode() { 
    lcd.clear();
    lcd.setCursor(0, 0);
    String line0 = "";
    line0 += (isAutoMode ? "AUTO" : "MANUAL");
    if (isWiFiEnabled && WiFi.status() == WL_CONNECTED) {
        String ipStr = WiFi.localIP().toString();
        if (ipStr.length() <= 9) {
             line0 += (isAutoMode ? " " : "  "); 
             line0 += ipStr;
        } else if (ipStr.length() <= 6) {
             line0 += "      "; 
             line0 += ipStr;
        }
    } else if (isWiFiEnabled) {
        line0 += (isAutoMode ? " " : "  ");
        line0 += "WiFi...";
    } else {
        line0 += (isAutoMode ? " " : "  ");
        line0 += "WiFi OFF";
    }
    lcd.print(line0);
    
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
        line1 += " R"; 
        line1 += rpmStr;
    }
    lcd.print(line1.substring(0,16)); 
}

void displayMenu() {
    lcd.clear();
    switch (currentMenuScreen) {
        case MAIN_MENU: displayMainMenu(); break;
        case WIFI_SETTINGS: displayWiFiSettingsMenu(); break;
        case WIFI_SCAN: displayWiFiScanMenu(); break;
        case WIFI_PASSWORD_ENTRY: displayPasswordEntryMenu(); break;
        case WIFI_STATUS: displayWiFiStatusMenu(); break;
        case CONFIRM_REBOOT: displayConfirmRebootMenu(); break;
        default: 
            lcd.print("Unknown Menu"); 
            break;
    }
}

void displayMainMenu() {
    lcd.setCursor(0, 0); lcd.print(selectedMenuItem == 0 ? ">WiFi Settings" : " WiFi Settings");
    lcd.setCursor(0, 1); lcd.print(selectedMenuItem == 1 ? ">View Status" : " View Status");
}

void displayWiFiSettingsMenu() {
    String line0 = (selectedMenuItem == 0 ? ">WiFi: " : " WiFi: ");
    line0 += (isWiFiEnabled ? "Enabled" : "Disabled");
    String line1 = "";

    switch(selectedMenuItem) {
        case 0: line1 = " (Toggle State)"; break;
        case 1: line1 = ">Scan Networks"; break;
        case 2: line1 = ">SSID: "; line1 += String(current_ssid).substring(0,8); break;
        case 3: line1 = ">Password Set"; break;
        case 4: line1 = ">Connect WiFi"; break;
        case 5: line1 = ">DisconnectWiFi"; break;
        case 6: line1 = ">Back to Main"; break;
        default: line1 = " Select Option"; break;
    }
    if (selectedMenuItem > 0 && selectedMenuItem <=6) { 
        line0 = " WiFi: "; line0 += (isWiFiEnabled ? "Enabled" : "Disabled");
    } else if (selectedMenuItem != 0) { 
        line0 = " WiFi: "; line0 += (isWiFiEnabled ? "Enabled" : "Disabled");
    }

    lcd.setCursor(0, 0); lcd.print(line0.substring(0,16));
    lcd.setCursor(0, 1); lcd.print(line1.substring(0,16));
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
    if (selectedMenuItem < scanResultCount) {
        lcd.print(">");
        lcd.print(scannedSSIDs[selectedMenuItem].substring(0,15)); 
    } else {
        lcd.print("Scroll Error");
    }
}

void displayPasswordEntryMenu() {
    lcd.setCursor(0,0); lcd.print("Enter Pass Char:");
    String passMask = "";
    for(int i=0; i < passwordCharIndex; ++i) passMask += "*";
    passMask += currentPasswordEditChar;
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
