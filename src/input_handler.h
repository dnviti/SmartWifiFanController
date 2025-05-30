#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include "config.h"
#include "ota_updater.h" // Include for triggerOTAUpdateCheck

void handleMenuInput();
void handleSerialCommands();
void performWiFiScan(); 
void attemptWiFiConnection(); 
void disconnectWiFi(); 

#endif // INPUT_HANDLER_H
