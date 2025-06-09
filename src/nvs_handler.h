#ifndef NVS_HANDLER_H
#define NVS_HANDLER_H

#include "config.h"

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

// Security NVS Functions
void saveOtaConfig();
void loadOtaConfig();

#endif // NVS_HANDLER_H
