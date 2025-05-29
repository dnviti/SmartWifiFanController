#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include "config.h" 
#include <PubSubClient.h> 

// MQTT Topics 
extern String mqttStatusTopic;
extern String mqttModeCommandTopic;
extern String mqttSpeedCommandTopic;
extern String mqttAvailabilityTopic;
extern String mqttFanCurveGetTopic;    
extern String mqttFanCurveStatusTopic; 
extern String mqttFanCurveSetTopic;    
extern String mqttFanCommandTopic;     

// Topics for controllable entities (settings that make sense to control via HA)
extern String mqttDiscoveryConfigCommandTopic; // For enabling/disabling discovery (the boolean setting)
extern String mqttRebootCommandTopic;          
extern String mqttDiscoveryPrefixSetCommandTopic; // To set the discovery prefix string

// REMOVED topics for WiFi and core MQTT client config:
// extern String mqttWifiEnableCommandTopic;
// extern String mqttWifiSsidCommandTopic;
// extern String mqttWifiConnectCommandTopic;
// extern String mqttWifiDisconnectCommandTopic;
// extern String mqttMqttEnableCommandTopic; // For the MQTT client setting itself
// extern String mqttBrokerServerCommandTopic;
// extern String mqttBrokerPortCommandTopic;
// extern String mqttBrokerUserCommandTopic;
// extern String mqttBaseTopicCommandTopic;


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
