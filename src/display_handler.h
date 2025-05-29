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
void displayWiFiStatusMenu(); 
void displayConfirmRebootMenu();

// MQTT Menu Display Functions
void displayMqttSettingsMenu();
void displayMqttEntryMenu(const char* prompt, const char* currentValue, bool isPassword, bool isNumericOnly = false, int maxLength = 16); 

// MQTT Discovery Menu Display Functions
void displayMqttDiscoverySettingsMenu();

// OTA Update Menu Display Function - NEW
void displayOtaUpdateMenu();


#endif // DISPLAY_HANDLER_H
