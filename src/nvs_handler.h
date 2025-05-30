#pragma once

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
