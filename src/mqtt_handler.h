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

// Function declarations
void setupMQTT();
void connectMQTT();
void loopMQTT();
void publishStatusMQTT();
void publishFanCurveMQTT(); // New: Function to publish the fan curve
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishMqttAvailability(bool available);

#endif // MQTT_HANDLER_H
