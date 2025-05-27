#include "mqtt_handler.h"
#include "config.h" 
#include "fan_control.h" 
#include "nvs_handler.h" // For saveFanCurveToNVS, saveMqttDiscoveryConfig
#include <ArduinoJson.h> 

// Define MQTT Topics
String mqttStatusTopic = "";
String mqttModeCommandTopic = "";   
String mqttSpeedCommandTopic = "";  
String mqttAvailabilityTopic = ""; 
String mqttFanCurveGetTopic = "";    
String mqttFanCurveStatusTopic = ""; 
String mqttFanCurveSetTopic = "";    
String mqttFanCommandTopic = ""; 
// ADDED: Definitions for new command topics
String mqttDiscoveryConfigCommandTopic = "";
String mqttRebootCommandTopic = "";


unsigned long lastMqttConnectAttempt = 0;
const long mqttConnectRetryInterval = 5000; // Try to reconnect every 5 seconds

// Helper function to construct full topic paths
String getFullTopic(const String& suffix, bool isDiscoveryTopic = false) {
    String topic = "";
    if (isDiscoveryTopic) {
        topic = String(mqttDiscoveryPrefix); // e.g. "homeassistant"
    } else {
        topic = String(mqttBaseTopic); // e.g. "fancontroller"
    }

    if (topic.length() > 0 && !topic.endsWith("/")) {
        topic += "/";
    } else if (topic.length() == 0 && suffix.startsWith("/")) {
        // If base is empty (discovery or base topic), and suffix starts with /, remove suffix's leading /
        return suffix.substring(1);
    } else if (topic.length() == 0 && !suffix.startsWith("/")) {
        // If base is empty and suffix doesn't start with /, just use suffix
        return suffix;
    }


    String cleanSuffix = suffix;
    if (cleanSuffix.startsWith("/")) { 
        cleanSuffix = cleanSuffix.substring(1);
    }
    topic += cleanSuffix;
    return topic;
}


void setupMQTT() {
    if (!isMqttEnabled) {
        if (serialDebugEnabled) Serial.println("[MQTT] MQTT is disabled in config. Skipping setup.");
        return;
    }

    if (strlen(mqttBaseTopic) == 0 && serialDebugEnabled) {
        Serial.println("[MQTT_WARN] Base topic is empty. Device topics will be relative (e.g., 'status_json').");
    }
    if (strlen(mqttDiscoveryPrefix) == 0 && isMqttDiscoveryEnabled && serialDebugEnabled) {
        Serial.println("[MQTT_WARN] Discovery prefix is empty. HA Discovery might not work as expected.");
    }
    
    mqttStatusTopic = getFullTopic("status_json"); 
    mqttModeCommandTopic = getFullTopic("mode/set");
    mqttSpeedCommandTopic = getFullTopic("speed/set");
    mqttAvailabilityTopic = getFullTopic("online_status");
    mqttFanCurveGetTopic = getFullTopic("fancurve/get");
    mqttFanCurveStatusTopic = getFullTopic("fancurve/status");
    mqttFanCurveSetTopic = getFullTopic("fancurve/set");
    mqttFanCommandTopic = getFullTopic("fan/set"); 

    // ADDED: Initialize new command topics
    mqttDiscoveryConfigCommandTopic = getFullTopic("discovery_config/set");
    mqttRebootCommandTopic = getFullTopic("reboot/set");

    if (serialDebugEnabled) {
        Serial.printf("[MQTT] Server: %s:%d\n", mqttServer, mqttPort);
        Serial.printf("[MQTT] Base Topic (effective): '%s'\n", mqttBaseTopic); 
        Serial.printf("[MQTT] Status JSON Topic: %s\n", mqttStatusTopic.c_str());
        Serial.printf("[MQTT] Mode Command Topic: %s\n", mqttModeCommandTopic.c_str());
        Serial.printf("[MQTT] Speed Command Topic: %s\n", mqttSpeedCommandTopic.c_str());
        Serial.printf("[MQTT] Availability Topic: %s\n", mqttAvailabilityTopic.c_str());
        Serial.printf("[MQTT] Fan Curve Get Topic: %s\n", mqttFanCurveGetTopic.c_str());    
        Serial.printf("[MQTT] Fan Curve Status Topic: %s\n", mqttFanCurveStatusTopic.c_str()); 
        Serial.printf("[MQTT] Fan Curve Set Topic: %s\n", mqttFanCurveSetTopic.c_str());    
        Serial.printf("[MQTT] Fan Command Topic: %s\n", mqttFanCommandTopic.c_str());
        Serial.printf("[MQTT] Discovery Config Command Topic: %s\n", mqttDiscoveryConfigCommandTopic.c_str()); // ADDED
        Serial.printf("[MQTT] Reboot Command Topic: %s\n", mqttRebootCommandTopic.c_str()); // ADDED
        Serial.printf("[MQTT] Discovery Enabled: %s, Prefix: %s\n", isMqttDiscoveryEnabled ? "Yes" : "No", mqttDiscoveryPrefix);
    }

    mqttClient.setServer(mqttServer, mqttPort);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(1024); // Further Increased buffer size for potentially more discovery payloads + fan curve
}

void connectMQTT() {
    if (!isMqttEnabled || WiFi.status() != WL_CONNECTED) {
        return;
    }
    
    if (mqttClient.connected()) {
        return;
    }

    unsigned long now = millis();
    if (now - lastMqttConnectAttempt < mqttConnectRetryInterval && lastMqttConnectAttempt != 0) {
        return; // Wait before retrying
    }
    lastMqttConnectAttempt = now;

    if (serialDebugEnabled) Serial.print("[MQTT] Attempting connection...");

    String clientId = "ESP32FanController-";
    clientId += String(mqttDeviceId); 

    bool connected = false;
    if (strlen(mqttUser) > 0 && strlen(mqttPassword) > 0) {
        if (serialDebugEnabled) Serial.printf(" with user: %s\n", mqttUser);
        connected = mqttClient.connect(clientId.c_str(), mqttUser, mqttPassword, mqttAvailabilityTopic.c_str(), 0, true, "offline");
    } else {
        if (serialDebugEnabled) Serial.println(" anonymously");
        connected = mqttClient.connect(clientId.c_str(), mqttAvailabilityTopic.c_str(), 0, true, "offline");
    }

    if (connected) {
        if (serialDebugEnabled) Serial.println("[MQTT] Connected!");
        publishMqttAvailability(true); 

        // Subscribe to command topics
        mqttClient.subscribe(mqttModeCommandTopic.c_str());
        mqttClient.subscribe(mqttSpeedCommandTopic.c_str());
        mqttClient.subscribe(mqttFanCurveGetTopic.c_str());
        mqttClient.subscribe(mqttFanCurveSetTopic.c_str());
        mqttClient.subscribe(mqttFanCommandTopic.c_str());
        // ADDED: Subscribe to new command topics
        mqttClient.subscribe(mqttDiscoveryConfigCommandTopic.c_str());
        mqttClient.subscribe(mqttRebootCommandTopic.c_str());
        
        if(serialDebugEnabled) {
            Serial.printf("[MQTT] Subscribed to %s, %s, %s, %s, %s, %s, %s\n", 
                mqttModeCommandTopic.c_str(), mqttSpeedCommandTopic.c_str(), 
                mqttFanCurveGetTopic.c_str(), mqttFanCurveSetTopic.c_str(), 
                mqttFanCommandTopic.c_str(), mqttDiscoveryConfigCommandTopic.c_str(), 
                mqttRebootCommandTopic.c_str());
        }
        
        publishStatusMQTT(); 
        publishFanCurveMQTT(); 

        if (isMqttDiscoveryEnabled) {
            publishMqttDiscovery(); 
        }

    } else {
        if (serialDebugEnabled) {
            Serial.print("[MQTT_ERR] Failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(". Retrying later.");
        }
    }
}

void loopMQTT() {
    if (!isMqttEnabled || WiFi.status() != WL_CONNECTED) {
        if (mqttClient.connected()) { 
            publishMqttAvailability(false);
            mqttClient.disconnect();
             if (serialDebugEnabled) Serial.println("[MQTT] WiFi disconnected, MQTT client disconnected.");
        }
        return;
    }

    if (!mqttClient.connected()) {
        connectMQTT();
    }
    mqttClient.loop(); 
}

void publishStatusMQTT() {
    if (!isMqttEnabled || !mqttClient.connected()) {
        return;
    }

    ArduinoJson::JsonDocument doc; 
    if (tempSensorFound) {
        doc["temperature"] = currentTemperature;
    } else {
        doc["temperature"] = nullptr; 
    }
    doc["tempSensorFound"] = tempSensorFound;
    doc["fanSpeedPercent"] = fanSpeedPercentage;
    doc["fanRpm"] = fanRpm;
    doc["mode"] = isAutoMode ? "AUTO" : "MANUAL";
    doc["manualSetSpeed"] = manualFanSpeedPercentage; 
    doc["ipAddress"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "0.0.0.0"; // Ensure IP is valid
    doc["wifiRSSI"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0; // Ensure RSSI is valid
    doc["fan_state"] = (fanSpeedPercentage > 0) ? "ON" : "OFF"; 
    
    // ADDED: Include more states for discovery
    doc["isWiFiEnabled"] = isWiFiEnabled; // The setting
    doc["wifiConnected"] = (WiFi.status() == WL_CONNECTED); // Actual connection status
    doc["serialDebugEnabled"] = serialDebugEnabled;
    doc["rebootNeeded"] = rebootNeeded;
    doc["isMqttEnabled"] = isMqttEnabled; // The setting for MQTT client
    doc["isMqttDiscoveryEnabled"] = isMqttDiscoveryEnabled; // The setting for HA discovery

    String output;
    serializeJson(doc, output);

    if (mqttClient.publish(mqttStatusTopic.c_str(), output.c_str(), true)) { 
        // if (serialDebugEnabled) Serial.printf("[MQTT] Status Published to %s: %s\n", mqttStatusTopic.c_str(), output.c_str());
    } else {
        if (serialDebugEnabled) Serial.printf("[MQTT_ERR] Failed to publish status to %s\n", mqttStatusTopic.c_str());
    }
}

void publishFanCurveMQTT() {
    if (!isMqttEnabled || !mqttClient.connected()) {
        return;
    }
    ArduinoJson::JsonDocument curveDoc; 
    JsonArray curveArray = curveDoc.to<JsonArray>();
    for (int i = 0; i < numCurvePoints; i++) {
        JsonObject point = curveArray.add<JsonObject>();
        point["temp"] = tempPoints[i];
        point["pwmPercent"] = pwmPercentagePoints[i];
    }
    String curveString;
    serializeJson(curveDoc, curveString);
    if (mqttClient.publish(mqttFanCurveStatusTopic.c_str(), curveString.c_str(), true)) { 
        if (serialDebugEnabled) Serial.printf("[MQTT] Fan Curve Published to %s\n", mqttFanCurveStatusTopic.c_str());
    } else {
        if (serialDebugEnabled) Serial.printf("[MQTT_ERR] Failed to publish fan curve to %s\n", mqttFanCurveStatusTopic.c_str());
    }
}


void publishMqttAvailability(bool available) {
    if (!isMqttEnabled) return; 
    if (available && !mqttClient.connected()) {
        if (serialDebugEnabled) Serial.println("[MQTT] Cannot publish 'online' availability, client not connected to broker.");
        return;
    }
    const char* message = available ? "online" : "offline";
    String effectiveAvailabilityTopic = mqttAvailabilityTopic;
    if (strlen(mqttBaseTopic) == 0 && effectiveAvailabilityTopic.startsWith("/")) {
        effectiveAvailabilityTopic = effectiveAvailabilityTopic.substring(1);
    }
    if (mqttClient.publish(effectiveAvailabilityTopic.c_str(), message, true)) { 
         if (serialDebugEnabled) Serial.printf("[MQTT] Availability '%s' published to %s\n", message, effectiveAvailabilityTopic.c_str());
    } else {
         if (serialDebugEnabled) Serial.printf("[MQTT_ERR] Failed to publish availability to %s\n", effectiveAvailabilityTopic.c_str());
    }
}

void publishMqttDiscovery() {
    if (!isMqttEnabled || !mqttClient.connected() || !isMqttDiscoveryEnabled || strlen(mqttDiscoveryPrefix) == 0) {
        if (serialDebugEnabled && isMqttEnabled && isMqttDiscoveryEnabled && strlen(mqttDiscoveryPrefix) == 0) {
            Serial.println("[MQTT_DISCOVERY_ERR] Discovery prefix is empty. Cannot publish discovery messages.");
        }
        return;
    }
    if (serialDebugEnabled) Serial.println("[MQTT_DISCOVERY] Publishing Home Assistant discovery messages...");

    ArduinoJson::JsonDocument deviceDoc;
    JsonObject deviceObj = deviceDoc.to<JsonObject>(); 
    deviceObj["identifiers"] = mqttDeviceId; 
    deviceObj["name"] = mqttDeviceName;
    deviceObj["manufacturer"] = "Daniele Viti (ESP32 Project)";
    deviceObj["model"] = "SmartWifiFanController";
    deviceObj["sw_version"] = FIRMWARE_VERSION;
    if (WiFi.status() == WL_CONNECTED) { // Only add URL if WiFi is connected
        deviceObj["configuration_url"] = "http://" + WiFi.localIP().toString() + "/";
    }


    String entityUniqueIdBase = String(mqttDeviceId); 
    String entityNameBase = String(mqttDeviceName); 

    // --- 1. Fan Entity (Already Implemented - No change here) ---
    {
        String fanObjectId = "fan"; 
        String fanConfigTopic = getFullTopic("fan/" + String(mqttDeviceId) + "/" + fanObjectId + "/config", true);
        ArduinoJson::JsonDocument fanDoc;
        fanDoc["name"] = entityNameBase; 
        fanDoc["unique_id"] = entityUniqueIdBase + "_" + fanObjectId;
        fanDoc["availability_topic"] = mqttAvailabilityTopic;
        fanDoc["device"] = deviceObj; 
        fanDoc["state_topic"] = mqttStatusTopic;
        fanDoc["state_value_template"] = "{{ value_json.fan_state }}"; 
        fanDoc["command_topic"] = mqttFanCommandTopic; 
        fanDoc["percentage_state_topic"] = mqttStatusTopic;
        fanDoc["percentage_value_template"] = "{{ value_json.fanSpeedPercent }}";
        fanDoc["percentage_command_topic"] = mqttSpeedCommandTopic;
        fanDoc["preset_mode_state_topic"] = mqttStatusTopic;
        fanDoc["preset_mode_value_template"] = "{{ value_json.mode }}";
        fanDoc["preset_mode_command_topic"] = mqttModeCommandTopic;
        JsonArray presetModes = fanDoc["preset_modes"].to<JsonArray>();
        presetModes.add("AUTO");
        presetModes.add("MANUAL");
        fanDoc["qos"] = 0;
        fanDoc["optimistic"] = false; 
        fanDoc["speed_range_min"] = 0; 
        fanDoc["speed_range_max"] = 100;
        String fanPayload;
        serializeJson(fanDoc, fanPayload);
        if (!mqttClient.publish(fanConfigTopic.c_str(), fanPayload.c_str(), true)) {
            if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY_ERR] Failed to publish fan config to %s\n", fanConfigTopic.c_str());
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY] Fan config published to %s\n", fanConfigTopic.c_str());
        }
    }

    // --- 2. Temperature Sensor (Already Implemented - No change here, just ensure status_json has temperature) ---
    if (tempSensorFound) { 
        String tempObjectId = "temperature";
        String tempConfigTopic = getFullTopic("sensor/" + String(mqttDeviceId) + "/" + tempObjectId + "/config", true);
        ArduinoJson::JsonDocument tempDoc;
        tempDoc["name"] = entityNameBase + " Temperature";
        tempDoc["unique_id"] = entityUniqueIdBase + "_" + tempObjectId;
        tempDoc["availability_topic"] = mqttAvailabilityTopic;
        tempDoc["device"] = deviceObj;
        tempDoc["state_topic"] = mqttStatusTopic;
        tempDoc["value_template"] = "{{ value_json.temperature }}";
        tempDoc["device_class"] = "temperature";
        tempDoc["unit_of_measurement"] = "°C";
        tempDoc["qos"] = 0;
        String tempPayload;
        serializeJson(tempDoc, tempPayload);
         if (!mqttClient.publish(tempConfigTopic.c_str(), tempPayload.c_str(), true)) {
            if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY_ERR] Failed to publish temperature sensor config to %s\n", tempConfigTopic.c_str());
        } else {
             if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY] Temperature sensor config published to %s\n", tempConfigTopic.c_str());
        }
    } else {
        String tempObjectId = "temperature";
        String tempConfigTopic = getFullTopic("sensor/" + String(mqttDeviceId) + "/" + tempObjectId + "/config", true);
        mqttClient.publish(tempConfigTopic.c_str(), "", true); 
    }

    // --- 3. Fan RPM Sensor (Already Implemented - No change here, just ensure status_json has fanRpm) ---
    {
        String rpmObjectId = "rpm";
        String rpmConfigTopic = getFullTopic("sensor/" + String(mqttDeviceId) + "/" + rpmObjectId + "/config", true);
        ArduinoJson::JsonDocument rpmDoc;
        rpmDoc["name"] = entityNameBase + " Fan RPM";
        rpmDoc["unique_id"] = entityUniqueIdBase + "_" + rpmObjectId;
        rpmDoc["availability_topic"] = mqttAvailabilityTopic;
        rpmDoc["device"] = deviceObj;
        rpmDoc["state_topic"] = mqttStatusTopic;
        rpmDoc["value_template"] = "{{ value_json.fanRpm }}";
        rpmDoc["unit_of_measurement"] = "RPM";
        rpmDoc["icon"] = "mdi:fan"; 
        rpmDoc["qos"] = 0;
        String rpmPayload;
        serializeJson(rpmDoc, rpmPayload);
        if (!mqttClient.publish(rpmConfigTopic.c_str(), rpmPayload.c_str(), true)) {
            if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY_ERR] Failed to publish RPM sensor config to %s\n", rpmConfigTopic.c_str());
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY] RPM sensor config published to %s\n", rpmConfigTopic.c_str());
        }
    }

    // --- NEW ENTITIES START HERE ---

    // --- 4. Temperature Sensor Found Status (Binary Sensor) ---
    {
        String objectId = "temp_sensor_status";
        String configTopic = getFullTopic("binary_sensor/" + String(mqttDeviceId) + "/" + objectId + "/config", true);
        ArduinoJson::JsonDocument doc;
        doc["name"] = entityNameBase + " Temperature Sensor Status";
        doc["unique_id"] = entityUniqueIdBase + "_" + objectId;
        doc["availability_topic"] = mqttAvailabilityTopic;
        doc["device"] = deviceObj;
        doc["state_topic"] = mqttStatusTopic;
        doc["value_template"] = "{{ 'ON' if value_json.tempSensorFound else 'OFF' }}";
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["device_class"] = "connectivity"; // Or "problem" if OFF is an issue
        doc["qos"] = 0;
        String payloadStr;
        serializeJson(doc, payloadStr);
        if (!mqttClient.publish(configTopic.c_str(), payloadStr.c_str(), true)) {
             if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY_ERR] Failed to publish %s config\n", objectId);
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY] %s config published\n", objectId);
        }
    }
    
    // --- 5. WiFi Status (Binary Sensor) ---
    {
        String objectId = "wifi_connection_status";
        String configTopic = getFullTopic("binary_sensor/" + String(mqttDeviceId) + "/" + objectId + "/config", true);
        ArduinoJson::JsonDocument doc;
        doc["name"] = entityNameBase + " WiFi Connection";
        doc["unique_id"] = entityUniqueIdBase + "_" + objectId;
        doc["availability_topic"] = mqttAvailabilityTopic; // Device availability implies WiFi is working enough for MQTT
        doc["device"] = deviceObj;
        doc["state_topic"] = mqttStatusTopic;
        doc["value_template"] = "{{ 'ON' if value_json.wifiConnected else 'OFF' }}";
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["device_class"] = "connectivity";
        doc["icon"] = "mdi:wifi";
        doc["qos"] = 0;
        String payloadStr;
        serializeJson(doc, payloadStr);
        if (!mqttClient.publish(configTopic.c_str(), payloadStr.c_str(), true)) {
             if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY_ERR] Failed to publish %s config\n", objectId);
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY] %s config published\n", objectId);
        }
    }

    // --- 6. Serial Debug Status (Binary Sensor) ---
    {
        String objectId = "serial_debug_status";
        String configTopic = getFullTopic("binary_sensor/" + String(mqttDeviceId) + "/" + objectId + "/config", true);
        ArduinoJson::JsonDocument doc;
        doc["name"] = entityNameBase + " Serial Debug Status";
        doc["unique_id"] = entityUniqueIdBase + "_" + objectId;
        doc["availability_topic"] = mqttAvailabilityTopic;
        doc["device"] = deviceObj;
        doc["state_topic"] = mqttStatusTopic;
        doc["value_template"] = "{{ 'ON' if value_json.serialDebugEnabled else 'OFF' }}";
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["icon"] = "mdi:bug-check";
        doc["entity_category"] = "diagnostic";
        doc["qos"] = 0;
        String payloadStr;
        serializeJson(doc, payloadStr);
         if (!mqttClient.publish(configTopic.c_str(), payloadStr.c_str(), true)) {
             if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY_ERR] Failed to publish %s config\n", objectId);
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY] %s config published\n", objectId);
        }
    }

    // --- 7. Reboot Needed Status (Binary Sensor) ---
    {
        String objectId = "reboot_needed_status";
        String configTopic = getFullTopic("binary_sensor/" + String(mqttDeviceId) + "/" + objectId + "/config", true);
        ArduinoJson::JsonDocument doc;
        doc["name"] = entityNameBase + " Reboot Needed";
        doc["unique_id"] = entityUniqueIdBase + "_" + objectId;
        doc["availability_topic"] = mqttAvailabilityTopic;
        doc["device"] = deviceObj;
        doc["state_topic"] = mqttStatusTopic;
        doc["value_template"] = "{{ 'ON' if value_json.rebootNeeded else 'OFF' }}";
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["device_class"] = "problem";
        doc["entity_category"] = "diagnostic";
        doc["qos"] = 0;
        String payloadStr;
        serializeJson(doc, payloadStr);
        if (!mqttClient.publish(configTopic.c_str(), payloadStr.c_str(), true)) {
             if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY_ERR] Failed to publish %s config\n", objectId);
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY] %s config published\n", objectId);
        }
    }
    
    // --- 8. MQTT Client Setting Status (Binary Sensor) ---
    {
        String objectId = "mqtt_client_setting_status"; // Renamed to avoid confusion with actual connection
        String configTopic = getFullTopic("binary_sensor/" + String(mqttDeviceId) + "/" + objectId + "/config", true);
        ArduinoJson::JsonDocument doc;
        doc["name"] = entityNameBase + " MQTT Client Enabled Setting";
        doc["unique_id"] = entityUniqueIdBase + "_" + objectId;
        doc["availability_topic"] = mqttAvailabilityTopic;
        doc["device"] = deviceObj;
        doc["state_topic"] = mqttStatusTopic;
        doc["value_template"] = "{{ 'ON' if value_json.isMqttEnabled else 'OFF' }}";
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["icon"] = "mdi:mqtt";
        doc["entity_category"] = "config"; // This is a configuration entity
        doc["qos"] = 0;
        String payloadStr;
        serializeJson(doc, payloadStr);
        if (!mqttClient.publish(configTopic.c_str(), payloadStr.c_str(), true)) {
             if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY_ERR] Failed to publish %s config\n", objectId);
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY] %s config published\n", objectId);
        }
    }

    // --- 9. Manual Mode Target Speed (Sensor) ---
    {
        String objectId = "manual_target_speed";
        String configTopic = getFullTopic("sensor/" + String(mqttDeviceId) + "/" + objectId + "/config", true);
        ArduinoJson::JsonDocument doc;
        doc["name"] = entityNameBase + " Manual Mode Target Speed";
        doc["unique_id"] = entityUniqueIdBase + "_" + objectId;
        doc["availability_topic"] = mqttAvailabilityTopic;
        doc["device"] = deviceObj;
        doc["state_topic"] = mqttStatusTopic;
        doc["value_template"] = "{{ value_json.manualSetSpeed }}";
        doc["unit_of_measurement"] = "%";
        doc["icon"] = "mdi:speedometer-medium";
        doc["entity_category"] = "diagnostic";
        doc["qos"] = 0;
        String payloadStr;
        serializeJson(doc, payloadStr);
        if (!mqttClient.publish(configTopic.c_str(), payloadStr.c_str(), true)) {
             if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY_ERR] Failed to publish %s config\n", objectId);
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY] %s config published\n", objectId);
        }
    }

    // --- 10. WiFi Signal Strength (Sensor) ---
    {
        String objectId = "wifi_rssi";
        String configTopic = getFullTopic("sensor/" + String(mqttDeviceId) + "/" + objectId + "/config", true);
        ArduinoJson::JsonDocument doc;
        doc["name"] = entityNameBase + " WiFi Signal Strength";
        doc["unique_id"] = entityUniqueIdBase + "_" + objectId;
        doc["availability_topic"] = mqttAvailabilityTopic;
        doc["device"] = deviceObj;
        doc["state_topic"] = mqttStatusTopic;
        doc["value_template"] = "{{ value_json.wifiRSSI }}";
        doc["device_class"] = "signal_strength";
        doc["unit_of_measurement"] = "dBm";
        doc["entity_category"] = "diagnostic";
        doc["qos"] = 0;
        String payloadStr;
        serializeJson(doc, payloadStr);
        if (!mqttClient.publish(configTopic.c_str(), payloadStr.c_str(), true)) {
             if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY_ERR] Failed to publish %s config\n", objectId);
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY] %s config published\n", objectId);
        }
    }

    // --- 11. IP Address (Sensor) ---
    {
        String objectId = "ip_address";
        String configTopic = getFullTopic("sensor/" + String(mqttDeviceId) + "/" + objectId + "/config", true);
        ArduinoJson::JsonDocument doc;
        doc["name"] = entityNameBase + " IP Address";
        doc["unique_id"] = entityUniqueIdBase + "_" + objectId;
        doc["availability_topic"] = mqttAvailabilityTopic;
        doc["device"] = deviceObj;
        doc["state_topic"] = mqttStatusTopic;
        doc["value_template"] = "{{ value_json.ipAddress }}";
        doc["icon"] = "mdi:ip-network-outline";
        doc["entity_category"] = "diagnostic";
        doc["qos"] = 0;
        String payloadStr;
        serializeJson(doc, payloadStr);
        if (!mqttClient.publish(configTopic.c_str(), payloadStr.c_str(), true)) {
             if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY_ERR] Failed to publish %s config\n", objectId);
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY] %s config published\n", objectId);
        }
    }

    // --- 12. MQTT Discovery Enabled (Switch) ---
    {
        String objectId = "mqtt_discovery_enabled_switch";
        String configTopic = getFullTopic("switch/" + String(mqttDeviceId) + "/" + objectId + "/config", true);
        ArduinoJson::JsonDocument doc;
        doc["name"] = entityNameBase + " MQTT Discovery Setting";
        doc["unique_id"] = entityUniqueIdBase + "_" + objectId;
        doc["availability_topic"] = mqttAvailabilityTopic;
        doc["device"] = deviceObj;
        doc["state_topic"] = mqttStatusTopic;
        doc["value_template"] = "{{ 'ON' if value_json.isMqttDiscoveryEnabled else 'OFF' }}";
        doc["command_topic"] = mqttDiscoveryConfigCommandTopic;
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["icon"] = "mdi:magnify-scan";
        doc["optimistic"] = false; // We report state
        doc["entity_category"] = "config";
        doc["qos"] = 0;
        String payloadStr;
        serializeJson(doc, payloadStr);
        if (!mqttClient.publish(configTopic.c_str(), payloadStr.c_str(), true)) {
             if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY_ERR] Failed to publish %s config\n", objectId);
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY] %s config published\n", objectId);
        }
    }

    // --- 13. Reboot Device (Button) ---
    {
        String objectId = "reboot_button";
        String configTopic = getFullTopic("button/" + String(mqttDeviceId) + "/" + objectId + "/config", true);
        ArduinoJson::JsonDocument doc;
        doc["name"] = entityNameBase + " Reboot Device";
        doc["unique_id"] = entityUniqueIdBase + "_" + objectId;
        doc["availability_topic"] = mqttAvailabilityTopic;
        doc["device"] = deviceObj;
        doc["command_topic"] = mqttRebootCommandTopic;
        doc["payload_press"] = "REBOOT"; 
        // doc["device_class"] = "restart"; // Not a standard device class for button, but icon helps
        doc["icon"] = "mdi:restart-alert";
        doc["entity_category"] = "config"; // Could also be diagnostic
        doc["qos"] = 0;
        String payloadStr;
        serializeJson(doc, payloadStr);
        if (!mqttClient.publish(configTopic.c_str(), payloadStr.c_str(), true)) {
             if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY_ERR] Failed to publish %s config\n", objectId);
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY] %s config published\n", objectId);
        }
    }


    if (serialDebugEnabled) Serial.println("[MQTT_DISCOVERY] Finished publishing all discovery messages.");
}


void mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (serialDebugEnabled) {
        Serial.print("[MQTT_RX] Message arrived [");
        Serial.print(topic);
        Serial.print("] ");
    }
    String messageTemp;
    messageTemp.reserve(length + 1); 
    for (unsigned int i = 0; i < length; i++) {
        messageTemp += (char)payload[i];
    }
    if (serialDebugEnabled) Serial.println(messageTemp);

    String topicStr = String(topic);

    if (topicStr.equals(mqttModeCommandTopic)) {
        if (messageTemp.equalsIgnoreCase("AUTO")) {
            isAutoMode = true;
            needsImmediateBroadcast = true; 
            if (serialDebugEnabled) Serial.println("[MQTT_CMD] Mode set to AUTO");
        } else if (messageTemp.equalsIgnoreCase("MANUAL")) {
            isAutoMode = false;
            if (fanSpeedPercentage == 0 && manualFanSpeedPercentage == 0) {
                manualFanSpeedPercentage = 50; 
            }
            needsImmediateBroadcast = true;
            if (serialDebugEnabled) Serial.println("[MQTT_CMD] Mode set to MANUAL");
        } else {
            if (serialDebugEnabled) Serial.println("[MQTT_CMD_ERR] Unknown mode command payload. Use AUTO or MANUAL.");
        }
    } else if (topicStr.equals(mqttSpeedCommandTopic)) {
        int speed = messageTemp.toInt();
        if (speed >= 0 && speed <= 100) {
            isAutoMode = false; 
            manualFanSpeedPercentage = speed;
            needsImmediateBroadcast = true; 
            if (serialDebugEnabled) Serial.printf("[MQTT_CMD] Manual speed target set to %d%% (mode set to MANUAL)\n", speed);
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_CMD_ERR] Invalid speed payload %d. Use 0-100.\n", speed);
        }
    } else if (topicStr.equals(mqttFanCommandTopic)) {
        if (messageTemp.equalsIgnoreCase("ON")) {
            isAutoMode = false; 
            if (manualFanSpeedPercentage == 0) { 
                manualFanSpeedPercentage = 50; 
            }
            setFanSpeed(manualFanSpeedPercentage); 
            if (serialDebugEnabled) Serial.printf("[MQTT_CMD] Fan turned ON (Manual mode, speed %d%%)\n", manualFanSpeedPercentage);
        } else if (messageTemp.equalsIgnoreCase("OFF")) {
            isAutoMode = false; 
            manualFanSpeedPercentage = 0; 
            setFanSpeed(0); 
            if (serialDebugEnabled) Serial.println("[MQTT_CMD] Fan turned OFF (Manual mode, speed 0%)");
        } else {
            if (serialDebugEnabled) Serial.println("[MQTT_CMD_ERR] Unknown fan command payload. Use ON or OFF.");
        }
        needsImmediateBroadcast = true; 
    }
    else if (topicStr.equals(mqttFanCurveGetTopic)) {
        if (serialDebugEnabled) Serial.println("[MQTT_CMD] Received request to publish fan curve.");
        publishFanCurveMQTT(); 
    } else if (topicStr.equals(mqttFanCurveSetTopic)) {
        if (serialDebugEnabled) Serial.println("[MQTT_CMD] Received new fan curve.");
        if (!tempSensorFound) {
            if(serialDebugEnabled) Serial.println("[MQTT_CMD_WARN] Ignored setCurve, temperature sensor not found.");
            return; 
        }
        ArduinoJson::JsonDocument newCurveDoc; 
        DeserializationError error = deserializeJson(newCurveDoc, messageTemp);
        if (error) {
            if(serialDebugEnabled) Serial.printf("[MQTT_CMD_ERR] deserializeJson() for fan curve failed: %s\n", error.c_str());
            return; 
        }
        JsonArray newCurveArray = newCurveDoc.as<JsonArray>();
        if (!newCurveArray || newCurveArray.size() < 2 || newCurveArray.size() > MAX_CURVE_POINTS) {
            if(serialDebugEnabled) Serial.printf("[MQTT_CMD_ERR] Invalid fan curve array. Size: %d (must be 2-%d).\n", newCurveArray.size(), MAX_CURVE_POINTS);
            return; 
        }
        bool curveValid = true;
        int lastTemp = -100; 
        int tempTempPointsValidation[MAX_CURVE_POINTS];
        int tempPwmPercentagePointsValidation[MAX_CURVE_POINTS]; 
        int newNumPoints = newCurveArray.size();
        for(int i=0; i < newNumPoints; ++i) {
            JsonObject point = newCurveArray[i];
            if (!point["temp"].is<int>() || !point["pwmPercent"].is<int>()) {
                curveValid = false;
                if(serialDebugEnabled) Serial.printf("[MQTT_CMD_ERR] Curve point %d missing temp/pwmPercent or wrong type.\n", i);
                break;
            }
            int t = point["temp"].as<int>();
            int p = point["pwmPercent"].as<int>();
            if (t < 0 || t > 120 || p < 0 || p > 100 || (i > 0 && t <= lastTemp) ) {
                curveValid = false;
                if(serialDebugEnabled) Serial.printf("[MQTT_CMD_ERR] Invalid curve point %d values: Temp=%d, PWM=%d. LastTemp=%d\n", i, t, p, lastTemp);
                break;
            }
            tempTempPointsValidation[i] = t;
            tempPwmPercentagePointsValidation[i] = p; 
            lastTemp = t;
        }
        if (curveValid) {
            numCurvePoints = newNumPoints;
            for(int i=0; i < numCurvePoints; ++i) {
                tempPoints[i] = tempTempPointsValidation[i];
                pwmPercentagePoints[i] = tempPwmPercentagePointsValidation[i]; 
            }
            if(serialDebugEnabled) Serial.println("[SYSTEM] Fan curve updated and validated via MQTT.");
            saveFanCurveToNVS(); 
            fanCurveChanged = true; 
            needsImmediateBroadcast = true; 
        } else {
            if(serialDebugEnabled) Serial.println("[SYSTEM_ERR] New fan curve from MQTT rejected due to invalid data.");
        }
    } 
    // ADDED: Handle new command topics
    else if (topicStr.equals(mqttDiscoveryConfigCommandTopic)) {
        bool newSetting;
        if (messageTemp.equalsIgnoreCase("ON")) {
            newSetting = true;
        } else if (messageTemp.equalsIgnoreCase("OFF")) {
            newSetting = false;
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_CMD_ERR] Invalid payload for discovery config: %s. Use ON or OFF.\n", messageTemp.c_str());
            return;
        }

        if (isMqttDiscoveryEnabled != newSetting) {
            isMqttDiscoveryEnabled = newSetting;
            saveMqttDiscoveryConfig();
            rebootNeeded = true; // Discovery changes usually require a reboot to republish all configs or clear them.
            needsImmediateBroadcast = true;
            if (serialDebugEnabled) Serial.printf("[MQTT_CMD] MQTT Discovery set to %s. Reboot needed.\n", isMqttDiscoveryEnabled ? "Enabled" : "Disabled");
        }
    } else if (topicStr.equals(mqttRebootCommandTopic)) {
        if (messageTemp.equalsIgnoreCase("REBOOT")) {
            if (serialDebugEnabled) Serial.println("[MQTT_CMD] Reboot command received. Rebooting now...");
            delay(500); // Give a moment for MQTT ack if any, and serial print
            ESP.restart();
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_CMD_ERR] Invalid payload for reboot: %s. Use REBOOT.\n", messageTemp.c_str());
        }
    }
    else {
        if (serialDebugEnabled) Serial.println("[MQTT_RX] Unknown topic.");
    }
}
