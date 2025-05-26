#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include "config.h"

void handleMenuInput();
void handleSerialCommands();
void performWiFiScan(); // Called by menu
void attemptWiFiConnection(); // Called by menu and serial
void disconnectWiFi(); // Called by menu and serial

#endif // INPUT_HANDLER_H
