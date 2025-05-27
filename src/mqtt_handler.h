#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include "config.h" 
#include <PubSubClient.h> 

// MQTT Topics 
extern String mqttStatusTopic;
extern String mqttModeCommandTopic;
extern String mqttSpeedCommandTopic;
extern String mqttAvailabilityTopic;
extern String mqttFanCurveGetTopic;    // New: For requesting the current curve
extern String mqttFanCurveStatusTopic; // New: For publishing the current curve
extern String mqttFanCurveSetTopic;    // New: For setting a new curve
extern String mqttFanCommandTopic;     // New: For HA Fan component on/off

// ADDED: Topics for new controllable entities
extern String mqttDiscoveryConfigCommandTopic; // For enabling/disabling discovery itself via MQTT
extern String mqttRebootCommandTopic;          // For triggering a reboot via MQTT

// Function declarations
void setupMQTT();
void connectMQTT();
void loopMQTT();
void publishStatusMQTT();
void publishFanCurveMQTT(); 
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishMqttAvailability(bool available);
void publishMqttDiscovery(); 

#endif // MQTT_HANDLER_H
