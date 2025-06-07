#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include "config.h"
#include "ota_updater.h" 

void handleMenuInput();
void handleSerialCommands();
void performWiFiScan(); 
void attemptWiFiConnection(); 
void disconnectWiFi(); 

// FIX: Add declaration for the new I2C scanning function
void scanI2C();

#endif // INPUT_HANDLER_H
