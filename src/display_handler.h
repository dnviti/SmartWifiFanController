#ifndef DISPLAY_HANDLER_H
#define DISPLAY_HANDLER_H

#include "config.h"

// Standard display functions
void updateLCD_NormalMode(); 
void displayMenu();

// Main menu and WiFi menu display functions
void displayMainMenu();
void displayWiFiSettingsMenu();
void displayWiFiScanMenu();
void displayPasswordEntryMenu();
void displayWiFiStatusMenu(); // Keep if used, or remove if "View Status" just exits menu
void displayConfirmRebootMenu();

// MQTT Menu Display Functions - ADDED DECLARATIONS
void displayMqttSettingsMenu();
void displayMqttEntryMenu(const char* prompt, const char* currentValue, bool isPassword, bool isNumericOnly = false);


#endif // DISPLAY_HANDLER_H
