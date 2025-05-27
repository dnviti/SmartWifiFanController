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

#endif // NVS_HANDLER_H
