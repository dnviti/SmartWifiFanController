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
void displayMqttEntryMenu(const char* prompt, const char* currentValue, bool isPassword, bool isNumericOnly = false, int maxLength = 16); // Added maxLength

// MQTT Discovery Menu Display Functions - ADDED DECLARATIONS
void displayMqttDiscoverySettingsMenu();
// void displayMqttDiscoveryPrefixEntryMenu(); // Will reuse displayMqttEntryMenu


#endif // DISPLAY_HANDLER_H
