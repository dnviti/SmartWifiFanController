#pragma once

#include "config.h"
#include "ota_updater.h" // Include for triggerOTAUpdateCheck

void handleMenuInput();
void handleSerialCommands();
void performWiFiScan(); 
void attemptWiFiConnection(); 
void disconnectWiFi();
