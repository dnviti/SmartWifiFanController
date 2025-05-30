#ifndef NVS_HANDLER_H
#define NVS_HANDLER_H

#include "config.h"

/**
 * @brief Saves the current WiFi configuration to NVS.
 *
 * Stores the global `current_ssid`, `current_password`, and `isWiFiEnabled` settings.
 * A reboot is typically required for these changes to take full effect.
 */
void saveWiFiConfig();

/**
 * @brief Loads WiFi configuration from NVS.
 *
 * Retrieves `current_ssid`, `current_password`, and `isWiFiEnabled` from NVS.
 * If no configuration is found, default values are used.
 */
void loadWiFiConfig();

/**
 * @brief Saves the current fan curve points to NVS.
 *
 * Stores the `numCurvePoints`, `tempPoints`, and `pwmPercentagePoints` arrays.
 */
void saveFanCurveToNVS();

/**
 * @brief Loads fan curve points from NVS.
 *
 * Retrieves `numCurvePoints`, `tempPoints`, and `pwmPercentagePoints` from NVS.
 * If loading fails or data is invalid, the default fan curve is set.
 */
void loadFanCurveFromNVS();

// MQTT NVS Functions
/**
 * @brief Saves the current MQTT client configuration to NVS.
 *
 * Stores `isMqttEnabled`, `mqttServer`, `mqttPort`, `mqttUser`, `mqttPassword`,
 * and `mqttBaseTopic` settings. A reboot is typically required for these changes to take full effect.
 */
void saveMqttConfig();

/**
 * @brief Loads MQTT client configuration from NVS.
 *
 * Retrieves `isMqttEnabled`, `mqttServer`, `mqttPort`, `mqttUser`, `mqttPassword`,
 * and `mqttBaseTopic` from NVS. If no configuration is found, default values are used.
 */
void loadMqttConfig();

// MQTT Discovery NVS Functions - ADDED
/**
 * @brief Saves the current MQTT Home Assistant Discovery configuration to NVS.
 *
 * Stores `isMqttDiscoveryEnabled` and `mqttDiscoveryPrefix` settings.
 * A reboot is typically required for these changes to take full effect.
 */
void saveMqttDiscoveryConfig();

/**
 * @brief Loads MQTT Home Assistant Discovery configuration from NVS.
 *
 * Retrieves `isMqttDiscoveryEnabled` and `mqttDiscoveryPrefix` from NVS.
 * If no configuration is found, default values are used.
 */
void loadMqttDiscoveryConfig();

#endif // NVS_HANDLER_H
