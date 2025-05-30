#ifndef NVS_HANDLER_H
#define NVS_HANDLER_H

#include "config.h"
#include <nvs_flash.h> // Explicit NVS dependency for NVS operations
#include <nvs.h>       // Explicit NVS dependency for NVS operations

void saveWiFiConfig();
void loadWiFiConfig();
void saveFanCurveToNVS();
void loadFanCurveFromNVS();

// MQTT NVS Functions
void saveMqttConfig();
void loadMqttConfig();

// MQTT Discovery NVS Functions - ADDED
void saveMqttDiscoveryConfig();
void loadMqttDiscoveryConfig();

#endif // NVS_HANDLER_H
