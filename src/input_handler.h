#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include "config.h"
#include "ota_updater.h" // Include for triggerOTAUpdateCheck

/// @brief Reads user input from the menu buttons and navigates/executes actions within the LCD menu.
/// @note Must be called frequently in the main application loop to ensure responsiveness and proper debouncing.
void handleMenuInput();

/// @brief Processes commands received over the serial interface.
/// @note This function is only active if `serialDebugEnabled` is true.
void handleSerialCommands();

/// @brief Initiates a scan for available WiFi networks.
/// @note Updates global variables `scanResultCount` and `scannedSSIDs`.
///       Can update the LCD display if the device is currently in menu mode.
void performWiFiScan();

/// @brief Attempts to connect the ESP32 to the WiFi network configured in NVS.
/// @note Updates `rebootNeeded` to true upon successful connection, indicating a system reboot is recommended.
///       Can update the LCD display with connection status.
void attemptWiFiConnection();

/// @brief Disconnects the ESP32 from the currently connected WiFi network.
/// @note Can update the LCD display with the disconnection status.
void disconnectWiFi();

#endif // INPUT_HANDLER_H
