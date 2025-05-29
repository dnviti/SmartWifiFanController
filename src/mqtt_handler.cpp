#include "mqtt_handler.h"
#include "config.h" 
#include "fan_control.h" 
#include "nvs_handler.h" // For saving all configs
#include "input_handler.h" // For attemptWiFiConnection, disconnectWiFi (though MQTT control removed)
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
String mqttDiscoveryConfigCommandTopic = ""; // For enabling/disabling discovery (the boolean setting)
String mqttRebootCommandTopic = "";
String mqttDiscoveryPrefixSetCommandTopic = ""; // To set the discovery prefix string


// REMOVED: Definitions for problematic configuration command topics
// String mqttWifiEnableCommandTopic = "";
// String mqttWifiSsidCommandTopic = "";
// String mqttWifiConnectCommandTopic = "";
// String mqttWifiDisconnectCommandTopic = "";
// String mqttMqttEnableCommandTopic = "";
// String mqttBrokerServerCommandTopic = "";
// String mqttBrokerPortCommandTopic = "";
// String mqttBrokerUserCommandTopic = "";
// String mqttBaseTopicCommandTopic = "";


unsigned long lastMqttConnectAttempt = 0;
const long mqttConnectRetryInterval = 5000; 

// Helper function to construct full topic paths
String getFullTopic(const String& suffix, bool isDiscoveryTopic = false) {
    String topic = "";
    if (isDiscoveryTopic) {
        topic = String(mqttDiscoveryPrefix); 
    } else {
        topic = String(mqttBaseTopic); 
    }

    if (topic.length() > 0 && !topic.endsWith("/")) {
        topic += "/";
    } else if (topic.length() == 0 && suffix.startsWith("/")) {
        return suffix.substring(1);
    } else if (topic.length() == 0 && !suffix.startsWith("/")) {
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
    // The initial check for isMqttEnabled is fine here.
    // If MQTT is off, most of this won't run, which is intended.
    // The HA switch for isMqttEnabled (if we decide to keep it for *status viewing*) 
    // would reflect the NVS value. Controlling it via HA is now removed.

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
    mqttDiscoveryConfigCommandTopic = getFullTopic("discovery_enabled/set"); 
    mqttRebootCommandTopic = getFullTopic("reboot/set");
    mqttDiscoveryPrefixSetCommandTopic = getFullTopic("discovery_prefix/set");


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
        Serial.printf("[MQTT] Discovery Enabled Command Topic: %s\n", mqttDiscoveryConfigCommandTopic.c_str()); 
        Serial.printf("[MQTT] Reboot Command Topic: %s\n", mqttRebootCommandTopic.c_str()); 
        Serial.printf("[MQTT] Discovery Prefix Set Command Topic: %s\n", mqttDiscoveryPrefixSetCommandTopic.c_str());
        Serial.printf("[MQTT] Discovery Enabled Setting: %s, Prefix: %s\n", isMqttDiscoveryEnabled ? "Yes" : "No", mqttDiscoveryPrefix);
    }

    mqttClient.setServer(mqttServer, mqttPort);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(1536); 
}

void connectMQTT() {
    if (!isMqttEnabled || WiFi.status() != WL_CONNECTED) { 
        if (serialDebugEnabled && !isMqttEnabled) Serial.println("[MQTT] MQTT client is disabled by setting, connection skipped.");
        if (serialDebugEnabled && WiFi.status() != WL_CONNECTED) Serial.println("[MQTT] WiFi not connected, MQTT connection skipped.");
        return;
    }
    
    if (mqttClient.connected()) {
        return;
    }

    unsigned long now = millis();
    if (now - lastMqttConnectAttempt < mqttConnectRetryInterval && lastMqttConnectAttempt != 0) {
        return; 
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
        mqttClient.subscribe(mqttDiscoveryConfigCommandTopic.c_str()); 
        mqttClient.subscribe(mqttRebootCommandTopic.c_str());
        mqttClient.subscribe(mqttDiscoveryPrefixSetCommandTopic.c_str());
        
        // REMOVED: Subscriptions to problematic config topics
        
        if(serialDebugEnabled) {
            Serial.println("[MQTT] Subscribed to relevant command topics.");
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
    if (!isMqttEnabled && WiFi.status() != WL_CONNECTED) { 
        if (mqttClient.connected()) { 
            publishMqttAvailability(false); 
            mqttClient.disconnect();
            if (serialDebugEnabled) Serial.println("[MQTT] MQTT client disabled or WiFi disconnected, MQTT client explicitly disconnected.");
        }
        return;
    }
     if (!isMqttEnabled && mqttClient.connected()){ 
        publishMqttAvailability(false);
        mqttClient.disconnect();
        if(serialDebugEnabled) Serial.println("[MQTT] MQTT setting disabled by user, client disconnected.");
        return;
    }

    if (!mqttClient.connected() && isMqttEnabled && WiFi.status() == WL_CONNECTED) { 
        connectMQTT();
    }
    if(mqttClient.connected()){ 
        mqttClient.loop(); 
    }
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
    doc["ipAddress"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "0.0.0.0";
    doc["wifiRSSI"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
    doc["fan_state"] = (fanSpeedPercentage > 0) ? "ON" : "OFF"; 
    
    doc["isWiFiEnabled"] = isWiFiEnabled; // State of the setting
    doc["wifiConnected"] = (WiFi.status() == WL_CONNECTED); // Actual connection status
    doc["serialDebugEnabled"] = serialDebugEnabled;
    doc["rebootNeeded"] = rebootNeeded;
    doc["isMqttEnabled"] = isMqttEnabled; // State of the MQTT client setting
    doc["isMqttDiscoveryEnabled"] = isMqttDiscoveryEnabled; // State of the HA discovery setting

    // Current config values for HA *sensor* entities state
    doc["currentSsid"] = current_ssid;
    doc["mqttBrokerServer"] = mqttServer;
    doc["mqttBrokerPort"] = mqttPort;
    doc["mqttBrokerUser"] = mqttUser; // Displaying user is okay, not password
    doc["mqttBaseTopic"] = mqttBaseTopic;
    doc["mqttDiscoveryPrefix"] = mqttDiscoveryPrefix; // Display current discovery prefix

    String output;
    serializeJson(doc, output);

    if (!mqttClient.publish(mqttStatusTopic.c_str(), output.c_str(), true)) { 
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
    if (!mqttClient.publish(mqttFanCurveStatusTopic.c_str(), curveString.c_str(), true)) { 
        if (serialDebugEnabled) Serial.printf("[MQTT_ERR] Failed to publish fan curve to %s\n", mqttFanCurveStatusTopic.c_str());
    } else {
        if (serialDebugEnabled) Serial.printf("[MQTT] Fan Curve Published to %s\n", mqttFanCurveStatusTopic.c_str());
    }
}


void publishMqttAvailability(bool available) {
    if (available && (!isMqttEnabled || !mqttClient.connected())) {
        if (serialDebugEnabled) Serial.println("[MQTT] Cannot publish 'online' availability, MQTT client not enabled or not connected to broker.");
        return;
    }
    const char* message = available ? "online" : "offline";
    String effectiveAvailabilityTopic = mqttAvailabilityTopic;
    if (strlen(mqttBaseTopic) == 0 && effectiveAvailabilityTopic.startsWith("/")) {
        effectiveAvailabilityTopic = effectiveAvailabilityTopic.substring(1);
    }
    if (!mqttClient.publish(effectiveAvailabilityTopic.c_str(), message, true)) { 
         if (serialDebugEnabled) Serial.printf("[MQTT_ERR] Failed to publish availability to %s\n", effectiveAvailabilityTopic.c_str());
    } else {
         if (serialDebugEnabled) Serial.printf("[MQTT] Availability '%s' published to %s\n", message, effectiveAvailabilityTopic.c_str());
    }
}

void publishMqttDiscovery() {
    if (!isMqttEnabled || !mqttClient.connected() || !isMqttDiscoveryEnabled || strlen(mqttDiscoveryPrefix) == 0) {
        if (serialDebugEnabled && isMqttEnabled && isMqttDiscoveryEnabled && strlen(mqttDiscoveryPrefix) == 0) {
            Serial.println("[MQTT_DISCOVERY_ERR] Discovery prefix is empty. Cannot publish discovery messages.");
        }
        if (isMqttEnabled && mqttClient.connected() && !isMqttDiscoveryEnabled && strlen(mqttDiscoveryPrefix) > 0) {
            if (serialDebugEnabled) Serial.println("[MQTT_DISCOVERY] Discovery disabled. Clearing previous discovery messages...");
            // List of object IDs to clear (component_type/object_id_suffix)
            // Keep only those that are sensible and were previously defined.
            const char* objectIdsToClear[] = {
                "fan/fan", "sensor/temperature", "sensor/rpm", 
                "binary_sensor/temp_sensor_status", "binary_sensor/wifi_connection_status", 
                "binary_sensor/serial_debug_status", "binary_sensor/reboot_needed_status",
                "sensor/manual_target_speed", "sensor/wifi_rssi", "sensor/ip_address", 
                "switch/mqtt_discovery_enabled_switch", "button/reboot_button", 
                "text/discovery_prefix_text", "text/fan_curve_text",
                // Sensors for displaying config values (these are fine to clear if discovery is off)
                "sensor/current_ssid_sensor", "sensor/mqtt_broker_server_sensor", 
                "sensor/mqtt_broker_port_sensor", "sensor/mqtt_broker_user_sensor",
                "sensor/mqtt_base_topic_sensor",
                "binary_sensor/wifi_enabled_setting_status", // Status of the WiFi setting
                "binary_sensor/mqtt_client_setting_status" // Status of the MQTT client setting
            };
            for (const char* fullObjectId : objectIdsToClear) {
                String fullIdStr = String(fullObjectId);
                int slashPos = fullIdStr.indexOf('/');
                if (slashPos != -1) {
                    String componentType = fullIdStr.substring(0, slashPos);
                    String objectIdSuffix = fullIdStr.substring(slashPos + 1);
                    String configTopic = getFullTopic(componentType + "/" + String(mqttDeviceId) + "/" + objectIdSuffix + "/config", true);
                    mqttClient.publish(configTopic.c_str(), "", true); 
                }
            }
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
    if (WiFi.status() == WL_CONNECTED) { 
        deviceObj["configuration_url"] = "http://" + WiFi.localIP().toString() + "/";
    }

    String entityUniqueIdBase = String(mqttDeviceId); 
    String entityNameBase = String(mqttDeviceName); 

    auto publishDiscoveryEntity = [&](const String& component, const String& objectId, JsonDocument& doc) {
        String configTopic = getFullTopic(component + "/" + String(mqttDeviceId) + "/" + objectId + "/config", true);
        String payloadStr;
        serializeJson(doc, payloadStr);
        if (!mqttClient.publish(configTopic.c_str(), payloadStr.c_str(), true)) {
            if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY_ERR] Failed to publish %s/%s config\n", component.c_str(), objectId.c_str());
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY] %s/%s config published to %s\n", component.c_str(), objectId.c_str(), configTopic.c_str());
        }
    };

    // --- Core Fan Control Entities ---
    // 1. Fan Entity
    {
        String objectId = "fan"; 
        ArduinoJson::JsonDocument doc;
        doc["name"] = entityNameBase; 
        doc["unique_id"] = entityUniqueIdBase + "_" + objectId;
        doc["availability_topic"] = mqttAvailabilityTopic;
        doc["device"] = deviceObj; 
        doc["state_topic"] = mqttStatusTopic;
        doc["state_value_template"] = "{{ value_json.fan_state }}"; 
        doc["command_topic"] = mqttFanCommandTopic; 
        doc["percentage_state_topic"] = mqttStatusTopic;
        doc["percentage_value_template"] = "{{ value_json.fanSpeedPercent }}";
        doc["percentage_command_topic"] = mqttSpeedCommandTopic;
        doc["preset_mode_state_topic"] = mqttStatusTopic;
        doc["preset_mode_value_template"] = "{{ value_json.mode }}";
        doc["preset_mode_command_topic"] = mqttModeCommandTopic;
        JsonArray presetModes = doc["preset_modes"].to<JsonArray>();
        presetModes.add("AUTO");
        presetModes.add("MANUAL");
        doc["qos"] = 0;
        doc["optimistic"] = false; 
        doc["speed_range_min"] = 0; 
        doc["speed_range_max"] = 100;
        publishDiscoveryEntity("fan", objectId, doc);
    }
    // 2. Fan Curve (Text Input for setting, Sensor for current value)
    {
        String objectId = "fan_curve_text";
        ArduinoJson::JsonDocument doc;
        doc["name"] = entityNameBase + " Fan Curve (JSON)";
        doc["unique_id"] = entityUniqueIdBase + "_" + objectId;
        doc["availability_topic"] = mqttAvailabilityTopic;
        doc["device"] = deviceObj;
        doc["state_topic"] = mqttFanCurveStatusTopic; 
        doc["command_topic"] = mqttFanCurveSetTopic;
        doc["icon"] = "mdi:chart-bell-curve-cumulative";
        doc["entity_category"] = "config";
        doc["qos"] = 0;
        publishDiscoveryEntity("text", objectId, doc);
    }


    // --- Sensor Readings ---
    // 3. Temperature Sensor
    if (tempSensorFound) { 
        String objectId = "temperature";
        ArduinoJson::JsonDocument doc;
        // ... (same as before)
        doc["name"] = entityNameBase + " Temperature";
        doc["unique_id"] = entityUniqueIdBase + "_" + objectId;
        doc["availability_topic"] = mqttAvailabilityTopic;
        doc["device"] = deviceObj;
        doc["state_topic"] = mqttStatusTopic;
        doc["value_template"] = "{{ value_json.temperature }}";
        doc["device_class"] = "temperature";
        doc["unit_of_measurement"] = "°C";
        doc["qos"] = 0;
        publishDiscoveryEntity("sensor", objectId, doc);
    } else { 
        String configTopic = getFullTopic("sensor/" + String(mqttDeviceId) + "/temperature/config", true);
        mqttClient.publish(configTopic.c_str(), "", true); 
    }
    // 4. Fan RPM Sensor
    {
        String objectId = "rpm";
        ArduinoJson::JsonDocument doc;
        // ... (same as before)
        doc["name"] = entityNameBase + " Fan RPM";
        doc["unique_id"] = entityUniqueIdBase + "_" + objectId;
        doc["availability_topic"] = mqttAvailabilityTopic;
        doc["device"] = deviceObj;
        doc["state_topic"] = mqttStatusTopic;
        doc["value_template"] = "{{ value_json.fanRpm }}";
        doc["unit_of_measurement"] = "RPM";
        doc["icon"] = "mdi:fan"; 
        doc["qos"] = 0;
        publishDiscoveryEntity("sensor", objectId, doc);
    }
    // 5. Manual Mode Target Speed (Sensor)
    {
        String objectId = "manual_target_speed";
        ArduinoJson::JsonDocument doc;
        // ... (same as before)
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
        publishDiscoveryEntity("sensor", objectId, doc);
    }

    // --- Diagnostic Binary Sensors ---
    // 6. Temperature Sensor Found Status
    {
        String objectId = "temp_sensor_status";
        ArduinoJson::JsonDocument doc;
        // ... (same as before)
        doc["name"] = entityNameBase + " Temperature Sensor Status";
        doc["unique_id"] = entityUniqueIdBase + "_" + objectId;
        doc["availability_topic"] = mqttAvailabilityTopic;
        doc["device"] = deviceObj;
        doc["state_topic"] = mqttStatusTopic;
        doc["value_template"] = "{{ 'ON' if value_json.tempSensorFound else 'OFF' }}";
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["device_class"] = "connectivity"; 
        doc["entity_category"] = "diagnostic";
        doc["qos"] = 0;
        publishDiscoveryEntity("binary_sensor", objectId, doc);
    }
    // 7. WiFi Connection Status
    {
        String objectId = "wifi_connection_status";
        ArduinoJson::JsonDocument doc;
        // ... (same as before)
        doc["name"] = entityNameBase + " WiFi Connection";
        doc["unique_id"] = entityUniqueIdBase + "_" + objectId;
        doc["availability_topic"] = mqttAvailabilityTopic; 
        doc["device"] = deviceObj;
        doc["state_topic"] = mqttStatusTopic;
        doc["value_template"] = "{{ 'ON' if value_json.wifiConnected else 'OFF' }}";
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["device_class"] = "connectivity";
        doc["icon"] = "mdi:wifi";
        doc["entity_category"] = "diagnostic";
        doc["qos"] = 0;
        publishDiscoveryEntity("binary_sensor", objectId, doc);
    }
    // 8. Serial Debug Status
    {
        String objectId = "serial_debug_status";
        ArduinoJson::JsonDocument doc;
        // ... (same as before)
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
        publishDiscoveryEntity("binary_sensor", objectId, doc);
    }
    // 9. Reboot Needed Status
    {
        String objectId = "reboot_needed_status";
        ArduinoJson::JsonDocument doc;
        // ... (same as before)
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
        publishDiscoveryEntity("binary_sensor", objectId, doc);
    }

    // --- Diagnostic Sensors for Config Values (Read-Only from HA perspective) ---
    // 10. Current WiFi SSID (Sensor)
    {
        String objectId = "current_ssid_sensor";
        ArduinoJson::JsonDocument doc;
        doc["name"] = entityNameBase + " Current WiFi SSID";
        doc["unique_id"] = entityUniqueIdBase + "_" + objectId;
        doc["availability_topic"] = mqttAvailabilityTopic;
        doc["device"] = deviceObj;
        doc["state_topic"] = mqttStatusTopic;
        doc["value_template"] = "{{ value_json.currentSsid }}";
        doc["icon"] = "mdi:wifi-settings";
        doc["entity_category"] = "diagnostic";
        publishDiscoveryEntity("sensor", objectId, doc);
    }
    // 11. MQTT Broker Server (Sensor)
    {
        String objectId = "mqtt_broker_server_sensor";
        ArduinoJson::JsonDocument doc;
        doc["name"] = entityNameBase + " MQTT Broker Server";
        doc["unique_id"] = entityUniqueIdBase + "_" + objectId;
        doc["availability_topic"] = mqttAvailabilityTopic;
        doc["device"] = deviceObj;
        doc["state_topic"] = mqttStatusTopic;
        doc["value_template"] = "{{ value_json.mqttBrokerServer }}";
        doc["icon"] = "mdi:server-network";
        doc["entity_category"] = "diagnostic";
        publishDiscoveryEntity("sensor", objectId, doc);
    }
    // 12. MQTT Broker Port (Sensor)
    {
        String objectId = "mqtt_broker_port_sensor";
        ArduinoJson::JsonDocument doc;
        doc["name"] = entityNameBase + " MQTT Broker Port";
        doc["unique_id"] = entityUniqueIdBase + "_" + objectId;
        doc["availability_topic"] = mqttAvailabilityTopic;
        doc["device"] = deviceObj;
        doc["state_topic"] = mqttStatusTopic;
        doc["value_template"] = "{{ value_json.mqttBrokerPort }}";
        doc["icon"] = "mdi:numeric";
        doc["entity_category"] = "diagnostic";
        publishDiscoveryEntity("sensor", objectId, doc);
    }
    // 13. MQTT Broker User (Sensor)
    {
        String objectId = "mqtt_broker_user_sensor";
        ArduinoJson::JsonDocument doc;
        doc["name"] = entityNameBase + " MQTT Broker User";
        doc["unique_id"] = entityUniqueIdBase + "_" + objectId;
        doc["availability_topic"] = mqttAvailabilityTopic;
        doc["device"] = deviceObj;
        doc["state_topic"] = mqttStatusTopic;
        doc["value_template"] = "{{ value_json.mqttBrokerUser }}";
        doc["icon"] = "mdi:account-key-outline";
        doc["entity_category"] = "diagnostic";
        publishDiscoveryEntity("sensor", objectId, doc);
    }
    // 14. MQTT Base Topic (Sensor)
    {
        String objectId = "mqtt_base_topic_sensor";
        ArduinoJson::JsonDocument doc;
        doc["name"] = entityNameBase + " MQTT Base Topic";
        doc["unique_id"] = entityUniqueIdBase + "_" + objectId;
        doc["availability_topic"] = mqttAvailabilityTopic;
        doc["device"] = deviceObj;
        doc["state_topic"] = mqttStatusTopic;
        doc["value_template"] = "{{ value_json.mqttBaseTopic }}";
        doc["icon"] = "mdi:folder-key-network-outline";
        doc["entity_category"] = "diagnostic";
        publishDiscoveryEntity("sensor", objectId, doc);
    }
     // 15. WiFi Enabled Setting Status (Binary Sensor)
    {
        String objectId = "wifi_enabled_setting_status";
        ArduinoJson::JsonDocument doc;
        doc["name"] = entityNameBase + " WiFi Enabled Setting";
        doc["unique_id"] = entityUniqueIdBase + "_" + objectId;
        doc["availability_topic"] = mqttAvailabilityTopic;
        doc["device"] = deviceObj;
        doc["state_topic"] = mqttStatusTopic;
        doc["value_template"] = "{{ 'ON' if value_json.isWiFiEnabled else 'OFF' }}";
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["icon"] = "mdi:wifi-cog";
        doc["entity_category"] = "diagnostic"; // Diagnostic, as control is removed
        publishDiscoveryEntity("binary_sensor", objectId, doc);
    }
    // 16. MQTT Client Enabled Setting Status (Binary Sensor)
    {
        String objectId = "mqtt_client_setting_status";
        ArduinoJson::JsonDocument doc;
        doc["name"] = entityNameBase + " MQTT Client Setting";
        doc["unique_id"] = entityUniqueIdBase + "_" + objectId;
        doc["availability_topic"] = mqttAvailabilityTopic;
        doc["device"] = deviceObj;
        doc["state_topic"] = mqttStatusTopic;
        doc["value_template"] = "{{ 'ON' if value_json.isMqttEnabled else 'OFF' }}";
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["icon"] = "mdi:mqtt";
        doc["entity_category"] = "diagnostic"; // Diagnostic, as control is removed
        publishDiscoveryEntity("binary_sensor", objectId, doc);
    }


    // --- Sensible Configuration Entities ---
    // 17. MQTT Discovery Enabled Setting (Switch)
    {
        String objectId = "mqtt_discovery_enabled_switch";
        ArduinoJson::JsonDocument doc;
        // ... (same as before)
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
        doc["optimistic"] = false; 
        doc["entity_category"] = "config";
        doc["qos"] = 0;
        publishDiscoveryEntity("switch", objectId, doc);
    }
    // 18. MQTT Discovery Prefix (Text - for setting it)
    {
        String objectId = "discovery_prefix_text"; // Renamed for clarity if also a sensor
        ArduinoJson::JsonDocument doc;
        doc["name"] = entityNameBase + " MQTT Discovery Prefix";
        doc["unique_id"] = entityUniqueIdBase + "_" + objectId;
        doc["availability_topic"] = mqttAvailabilityTopic;
        doc["device"] = deviceObj;
        doc["state_topic"] = mqttStatusTopic; // Read current value from status
        doc["value_template"] = "{{ value_json.mqttDiscoveryPrefix }}";
        doc["command_topic"] = mqttDiscoveryPrefixSetCommandTopic; // Topic to send new prefix
        doc["icon"] = "mdi:home-assistant";
        doc["entity_category"] = "config";
        doc["qos"] = 0;
        publishDiscoveryEntity("text", objectId, doc);
    }
    // 19. Reboot Device (Button)
    {
        String objectId = "reboot_button";
        ArduinoJson::JsonDocument doc;
        // ... (same as before)
        doc["name"] = entityNameBase + " Reboot Device";
        doc["unique_id"] = entityUniqueIdBase + "_" + objectId;
        doc["availability_topic"] = mqttAvailabilityTopic;
        doc["device"] = deviceObj;
        doc["command_topic"] = mqttRebootCommandTopic;
        doc["payload_press"] = "REBOOT"; 
        doc["icon"] = "mdi:restart-alert";
        doc["entity_category"] = "config"; 
        doc["qos"] = 0;
        publishDiscoveryEntity("button", objectId, doc);
    }


    if (serialDebugEnabled) Serial.println("[MQTT_DISCOVERY] Finished publishing all sensible discovery messages.");
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

    // --- Fan Control Commands ---
    if (topicStr.equals(mqttModeCommandTopic)) {
        if (messageTemp.equalsIgnoreCase("AUTO")) {
            isAutoMode = true;
        } else if (messageTemp.equalsIgnoreCase("MANUAL")) {
            isAutoMode = false;
            if (fanSpeedPercentage == 0 && manualFanSpeedPercentage == 0) manualFanSpeedPercentage = 50; 
        } else { if (serialDebugEnabled) Serial.println("[MQTT_CMD_ERR] Unknown mode payload.");}
        needsImmediateBroadcast = true; 
    } else if (topicStr.equals(mqttSpeedCommandTopic)) {
        int speed = messageTemp.toInt();
        if (speed >= 0 && speed <= 100) {
            isAutoMode = false; 
            manualFanSpeedPercentage = speed;
            needsImmediateBroadcast = true; 
        } else { if (serialDebugEnabled) Serial.printf("[MQTT_CMD_ERR] Invalid speed payload %d.\n", speed);}
    } else if (topicStr.equals(mqttFanCommandTopic)) {
        if (messageTemp.equalsIgnoreCase("ON")) {
            isAutoMode = false; 
            if (manualFanSpeedPercentage == 0) manualFanSpeedPercentage = 50; 
            setFanSpeed(manualFanSpeedPercentage); 
        } else if (messageTemp.equalsIgnoreCase("OFF")) {
            isAutoMode = false; 
            manualFanSpeedPercentage = 0; 
            setFanSpeed(0); 
        } else { if (serialDebugEnabled) Serial.println("[MQTT_CMD_ERR] Unknown fan command payload.");}
        needsImmediateBroadcast = true; 
    }
    // --- Fan Curve Commands ---
    else if (topicStr.equals(mqttFanCurveGetTopic)) {
        publishFanCurveMQTT(); 
    } else if (topicStr.equals(mqttFanCurveSetTopic)) {
        if (!tempSensorFound) { if(serialDebugEnabled) Serial.println("[MQTT_CMD_WARN] Ignored setCurve, temp sensor not found."); return; }
        ArduinoJson::JsonDocument newCurveDoc; 
        DeserializationError error = deserializeJson(newCurveDoc, messageTemp);
        if (error) { if(serialDebugEnabled) Serial.printf("[MQTT_CMD_ERR] deserializeJson() for fan curve failed: %s\n", error.c_str()); return; }
        JsonArray newCurveArray = newCurveDoc.as<JsonArray>();
        if (!newCurveArray || newCurveArray.size() < 2 || newCurveArray.size() > MAX_CURVE_POINTS) { if(serialDebugEnabled) Serial.printf("[MQTT_CMD_ERR] Invalid fan curve array. Size: %d (must be 2-%d).\n", newCurveArray.size(), MAX_CURVE_POINTS); return; }
        bool curveValid = true; int lastTemp = -100; 
        int tempTempPointsValidation[MAX_CURVE_POINTS]; int tempPwmPercentagePointsValidation[MAX_CURVE_POINTS]; 
        int newNumPoints = newCurveArray.size();
        for(int i=0; i < newNumPoints; ++i) {
            JsonObject point = newCurveArray[i];
            if (!point["temp"].is<int>() || !point["pwmPercent"].is<int>()) { curveValid = false; break; }
            int t = point["temp"].as<int>(); int p = point["pwmPercent"].as<int>();
            if (t < 0 || t > 120 || p < 0 || p > 100 || (i > 0 && t <= lastTemp) ) { curveValid = false; break; }
            tempTempPointsValidation[i] = t; tempPwmPercentagePointsValidation[i] = p; lastTemp = t;
        }
        if (curveValid) {
            numCurvePoints = newNumPoints;
            for(int i=0; i < numCurvePoints; ++i) { tempPoints[i] = tempTempPointsValidation[i]; pwmPercentagePoints[i] = tempPwmPercentagePointsValidation[i]; }
            if(serialDebugEnabled) Serial.println("[SYSTEM] Fan curve updated and validated via MQTT.");
            saveFanCurveToNVS(); fanCurveChanged = true; needsImmediateBroadcast = true; 
        } else { if(serialDebugEnabled) Serial.println("[SYSTEM_ERR] New fan curve from MQTT rejected."); }
    } 
    // --- System & Sensible Config Commands ---
    else if (topicStr.equals(mqttDiscoveryConfigCommandTopic)) { // For isMqttDiscoveryEnabled
        bool newSetting = messageTemp.equalsIgnoreCase("ON");
        if (isMqttDiscoveryEnabled != newSetting) {
            isMqttDiscoveryEnabled = newSetting; saveMqttDiscoveryConfig(); rebootNeeded = true; needsImmediateBroadcast = true;
            if (serialDebugEnabled) Serial.printf("[MQTT_CMD] MQTT Discovery setting set to %s. Reboot needed.\n", isMqttDiscoveryEnabled ? "Enabled" : "Disabled");
        }
    } else if (topicStr.equals(mqttRebootCommandTopic)) {
        if (messageTemp.equalsIgnoreCase("REBOOT")) { if (serialDebugEnabled) Serial.println("[MQTT_CMD] Reboot command received."); delay(500); ESP.restart(); }
    } else if (topicStr.equals(mqttDiscoveryPrefixSetCommandTopic)) { 
        if (messageTemp.length() < sizeof(mqttDiscoveryPrefix)) {
            // Basic validation for prefix (e.g., no spaces, valid MQTT topic characters) could be added here.
            // For now, just check length.
            if (strcmp(mqttDiscoveryPrefix, messageTemp.c_str()) != 0) {
                strcpy(mqttDiscoveryPrefix, messageTemp.c_str()); 
                saveMqttDiscoveryConfig(); 
                rebootNeeded = true; 
                needsImmediateBroadcast = true;
                if (serialDebugEnabled) Serial.printf("[MQTT_CMD] MQTT Discovery Prefix set to '%s'. Reboot needed.\n", mqttDiscoveryPrefix);
            }
        } else { if (serialDebugEnabled) Serial.println("[MQTT_CMD_ERR] MQTT Discovery Prefix too long.");}
    }
    // REMOVED: Callbacks for WiFi and core MQTT client config topics
    else {
        if (serialDebugEnabled) Serial.println("[MQTT_RX] Unknown topic or unhandled message.");
    }
}
