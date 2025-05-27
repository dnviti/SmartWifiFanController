#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include "config.h" // Includes PubSubClient via config.h -> Arduino.h (indirectly if PubSubClient is added to config.h includes)
#include <PubSubClient.h> // Explicit include is better

// MQTT Topics (will be constructed with base topic)
extern String mqttStatusTopic;
extern String mqttCommandTopicSuffix; // e.g., "/set"
extern String mqttModeCommandTopic;
extern String mqttSpeedCommandTopic;
extern String mqttAvailabilityTopic;

// Function declarations
void setupMQTT();
void connectMQTT();
void loopMQTT();
void publishStatusMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishMqttAvailability(bool available);

#endif // MQTT_HANDLER_H
