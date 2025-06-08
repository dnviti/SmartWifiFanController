#ifndef NETWORK_HANDLER_H
#define NETWORK_HANDLER_H

#include "config.h"

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
void broadcastWebSocketData();
void setupWebServerRoutes(); // For SPIFFS
void setupApiRoutes();       // For REST API

#endif // NETWORK_HANDLER_H
