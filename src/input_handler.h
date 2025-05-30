#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

// No includes needed here as function declarations do not depend on types from config.h or ota_updater.h

void handleMenuInput();
void handleSerialCommands();
void performWiFiScan(); 
void attemptWiFiConnection(); 
void disconnectWiFi(); 

#endif // INPUT_HANDLER_H
