#include "display_handler.h"
#include "config.h" // For global variables and u8g2 object

// Helper function to draw the main status screen on the OLED
void drawStatusScreen() {
    u8g2.setFont(u8g2_font_profont12_tr);

    // Line 1: Mode and Temperature
    String line1 = isAutoMode ? "AUTO" : "MAN";
    u8g2.drawStr(0, 11, line1.c_str());

    String tempStr;
    // FIX: Prioritize PC temperature on the display if available
    if (pcTempDataReceived && pcTemperature > -990.0) {
        tempStr = "PC " + String(pcTemperature, 0) + "C";
    } else if (tempSensorFound && currentTemperature > -990.0) {
        tempStr = String(currentTemperature, 0) + "C";
    } else {
        tempStr = "N/A";
    }
    u8g2.drawStr(128 - u8g2.getStrWidth(tempStr.c_str()), 11, tempStr.c_str());

    // Line 2: Fan Speed and RPM
    String fanStr = String(fanSpeedPercentage) + "%";
    u8g2.drawStr(0, 22, fanStr.c_str());

    String rpmStr = String(fanRpm) + "rpm";
    u8g2.drawStr(128 - u8g2.getStrWidth(rpmStr.c_str()), 22, rpmStr.c_str());
    
    // Line 3: Cycling Information View
    u8g2.setFont(u8g2_font_6x10_tr); // Use a text font
    switch(currentStatusScreenView) {
        case INFO_IP:
            if (isWiFiEnabled) {
                if (WiFi.status() == WL_CONNECTED) {
                    u8g2.drawStr(0, 31, WiFi.localIP().toString().c_str());
                } else {
                    u8g2.drawStr(0, 31, "WiFi Connecting...");
                }
            } else {
                u8g2.drawStr(0, 31, "WiFi OFF");
            }
            break;
        case INFO_MQTT:
            if (isMqttEnabled) {
                String mqtt_str = "MQTT: ";
                mqtt_str += mqttClient.connected() ? "OK" : "Connecting";
                u8g2.drawStr(0, 31, mqtt_str.c_str());
            } else {
                u8g2.drawStr(0, 31, "MQTT OFF");
            }
            break;
        case INFO_UPTIME: {
            unsigned long now = millis();
            unsigned long sec = now / 1000;
            unsigned long min = sec / 60;
            unsigned long hr = min / 60;
            unsigned long days = hr / 24;
            String uptimeStr = "Up: " + String(days) + "d " + String(hr % 24) + "h " + String(min % 60) + "m";
            u8g2.drawStr(0, 31, uptimeStr.c_str());
            break;
        }
        case INFO_VERSION:
            u8g2.drawStr(0, 31, FIRMWARE_VERSION);
            break;
        case INFO_MENU_HINT:
            u8g2.setFont(u8g2_font_open_iconic_all_1x_t);
            u8g2.drawGlyph(0, 32, 155); // Menu icon
            u8g2.setFont(u8g2_font_6x10_tr);
            u8g2.drawStr(10, 31, "Menu");
            break;
        default:
             u8g2.drawStr(0, 31, "...");
             break;
    }

    // Reboot needed indicator is always on the right if needed
    if (rebootNeeded) {
        u8g2.setFont(u8g2_font_open_iconic_all_1x_t);
        u8g2.drawGlyph(120, 32, 178); // Reload icon
    }
}


// Helper function to draw menu screens on the OLED
void drawMenuScreen() {
    u8g2.setFont(u8g2_font_6x12_tr); 

    const int max_items_on_screen = 2;
    const int item_height = 10;
    
    auto draw_menu_items = [&](const char* title, const char* items[], int count) {
        u8g2.drawStr(0, item_height - 2, title);
        u8g2.drawHLine(0, item_height, 128);

        int start_item = 0;
        if (selectedMenuItem >= max_items_on_screen) {
            start_item = selectedMenuItem - (max_items_on_screen - 1);
        }

        for (int i = 0; i < max_items_on_screen; i++) {
            int current_item_index = start_item + i;
            if (current_item_index >= count) break;

            int y_pos = (i * item_height) + (item_height * 2);
            
            String item_text = items[current_item_index];

            if (current_item_index == selectedMenuItem) {
                u8g2.drawBox(0, y_pos - item_height + 2, 128, item_height);
                u8g2.setDrawColor(0); 
                u8g2.drawStr(2, y_pos, item_text.c_str());
                u8g2.setDrawColor(1); 
            } else {
                u8g2.drawStr(2, y_pos, item_text.c_str());
            }
        }
    };
    
    // --- Specific Menu Screen Logic ---
    switch (currentMenuScreen) {
        case MAIN_MENU: {
            const char* items[] = {"WiFi Settings", "MQTT Settings", "OTA Update", "Exit Menu"};
            draw_menu_items("Main Menu", items, 4);
            break;
        }
        case WIFI_SETTINGS: {
            String wifi_status = String("WiFi: ") + (isWiFiEnabled ? "Enabled" : "Disabled");
            String ssid_status = String("SSID: ") + String(current_ssid);
            const char* items[] = {wifi_status.c_str(), "Scan Networks", ssid_status.c_str(), "Set Password", "Connect", "Disconnect", "Back"};
            draw_menu_items("WiFi Settings", items, 7);
            break;
        }
        case WIFI_SCAN: {
            u8g2.drawStr(0, 10, "Scanning...");
            if (scanResultCount > 0) {
                 const char* item_pointers[scanResultCount];
                 for(int i = 0; i < scanResultCount; i++) {
                     item_pointers[i] = scannedSSIDs[i].c_str();
                 }
                draw_menu_items("Select WiFi", item_pointers, scanResultCount);
            } else if (scanResultCount == 0) {
                 u8g2.drawStr(0, 22, "No networks found");
            }
            break;
        }
        case WIFI_PASSWORD_ENTRY: {
             u8g2.drawStr(0, 10, "Enter Password:");
            String passMask = "";
            for(int i = 0; i < passwordCharIndex; ++i) passMask += "*";
            passMask += currentPasswordEditChar;
            if (passwordCharIndex >= sizeof(passwordInputBuffer) - 2) { passMask += " [OK?]"; }
            u8g2.drawStr(0, 22, passMask.c_str());
            break;
        }
        case OTA_UPDATE_SCREEN: {
            u8g2.drawStr(0, 10, "Firmware Update");
             if (ota_in_progress) {
                 u8g2.drawStr(0, 22, "In Progress...");
                 u8g2.drawStr(0, 32, ota_status_message.substring(0,21).c_str());
             } else {
                const char* items[] = {"Check & Update", "Back"};
                draw_menu_items("OTA", items, 2);
             }
            break;
        }
        case CONFIRM_REBOOT: {
             u8g2.drawStr(0, 10, "Reboot Needed!");
             const char* items[] = {"Reboot Now", "Cancel"};
             draw_menu_items("Confirm", items, 2);
             break;
        }
        case MQTT_SETTINGS: {
            String mqtt_status = String("MQTT: ") + (isMqttEnabled ? "Enabled" : "Disabled");
            const char* items[] = {mqtt_status.c_str(), "Server/Port", "User/Pass", "Base Topic", "Discovery Cfg", "Back"};
            draw_menu_items("MQTT Settings", items, 6);
            break;
        }
        default: {
            u8g2.drawStr(0, 10, "Menu");
            u8g2.drawStr(0, 22, "Not Implemented");
            break;
        }
    }
}


void updateDisplay() {
  u8g2.firstPage();
  do {
    if (isInMenuMode) {
      drawMenuScreen();
    } else {
      drawStatusScreen();
    }
  } while (u8g2.nextPage());
}
