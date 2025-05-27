#include "mqtt_handler.h"
#include "config.h" 
#include "fan_control.h" 
#include "nvs_handler.h" // For saveFanCurveToNVS
#include <ArduinoJson.h> 

// Define MQTT Topics
String mqttStatusTopic = "";
String mqttModeCommandTopic = "";   
String mqttSpeedCommandTopic = "";  
String mqttAvailabilityTopic = ""; 
String mqttFanCurveGetTopic = "";    
String mqttFanCurveStatusTopic = ""; 
String mqttFanCurveSetTopic = "";    
String mqttFanCommandTopic = ""; // For HA Fan component on/off

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
    mqttFanCommandTopic = getFullTopic("fan/set"); // New topic for HA fan on/off

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
        Serial.printf("[MQTT] Discovery Enabled: %s, Prefix: %s\n", isMqttDiscoveryEnabled ? "Yes" : "No", mqttDiscoveryPrefix);
    }

    mqttClient.setServer(mqttServer, mqttPort);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(768); // Increased buffer size for discovery payloads + fan curve
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
    clientId += String(mqttDeviceId); // Use the unique device ID for MQTT client ID

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
        publishMqttAvailability(true); // Publish online status

        // Subscribe to command topics
        if (mqttClient.subscribe(mqttModeCommandTopic.c_str())) {
            if (serialDebugEnabled) Serial.printf("[MQTT] Subscribed to %s\n", mqttModeCommandTopic.c_str());
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_ERR] Failed to subscribe to %s\n", mqttModeCommandTopic.c_str());
        }
        if (mqttClient.subscribe(mqttSpeedCommandTopic.c_str())) {
            if (serialDebugEnabled) Serial.printf("[MQTT] Subscribed to %s\n", mqttSpeedCommandTopic.c_str());
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_ERR] Failed to subscribe to %s\n", mqttSpeedCommandTopic.c_str());
        }
        if (mqttClient.subscribe(mqttFanCurveGetTopic.c_str())) {
            if (serialDebugEnabled) Serial.printf("[MQTT] Subscribed to %s\n", mqttFanCurveGetTopic.c_str());
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_ERR] Failed to subscribe to %s\n", mqttFanCurveGetTopic.c_str());
        }
        if (mqttClient.subscribe(mqttFanCurveSetTopic.c_str())) {
            if (serialDebugEnabled) Serial.printf("[MQTT] Subscribed to %s\n", mqttFanCurveSetTopic.c_str());
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_ERR] Failed to subscribe to %s\n", mqttFanCurveSetTopic.c_str());
        }
        // Subscribe to new fan command topic for HA
        if (mqttClient.subscribe(mqttFanCommandTopic.c_str())) {
            if (serialDebugEnabled) Serial.printf("[MQTT] Subscribed to %s\n", mqttFanCommandTopic.c_str());
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_ERR] Failed to subscribe to %s\n", mqttFanCommandTopic.c_str());
        }
        
        publishStatusMQTT(); 
        publishFanCurveMQTT(); 

        if (isMqttDiscoveryEnabled) {
            publishMqttDiscovery(); // Publish HA discovery messages
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
    // Corrected: Use if/else for assigning temperature or null to avoid type conflict in ternary operator
    if (tempSensorFound) {
        doc["temperature"] = currentTemperature;
    } else {
        doc["temperature"] = nullptr; // Assign JSON null
    }
    doc["tempSensorFound"] = tempSensorFound;
    doc["fanSpeedPercent"] = fanSpeedPercentage;
    doc["fanRpm"] = fanRpm;
    doc["mode"] = isAutoMode ? "AUTO" : "MANUAL";
    doc["manualSetSpeed"] = manualFanSpeedPercentage; 
    doc["ipAddress"] = WiFi.localIP().toString();
    doc["wifiRSSI"] = WiFi.RSSI();
    doc["fan_state"] = (fanSpeedPercentage > 0) ? "ON" : "OFF"; // For HA fan component state

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
    if (!isMqttEnabled) return; // Only publish if MQTT is generally enabled
    // If trying to publish "online", ensure client is actually connected.
    // For "offline" (LWT), this check is not needed as LWT is set by broker.
    if (available && !mqttClient.connected()) {
        if (serialDebugEnabled) Serial.println("[MQTT] Cannot publish 'online' availability, client not connected to broker.");
        return;
    }

    const char* message = available ? "online" : "offline";
    // Availability topic should always be published, even if base topic is empty
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

    // --- Device Information ---
    // This will be included in each entity's config
    ArduinoJson::JsonDocument deviceDoc;
    JsonObject deviceObj = deviceDoc.to<JsonObject>(); // Changed to JsonObject
    deviceObj["identifiers"] = mqttDeviceId; // Use the unique device ID (e.g., esp32fanctrl_AABBCC)
    deviceObj["name"] = mqttDeviceName;
    deviceObj["manufacturer"] = "Daniele Viti (ESP32 Project)";
    deviceObj["model"] = "SmartWifiFanController";
    deviceObj["sw_version"] = FIRMWARE_VERSION;
    // deviceObj["configuration_url"] = "http://" + WiFi.localIP().toString() + "/"; // Optional: Link to web UI

    String discoveryTopicBase;
    String entityUniqueIdBase = String(mqttDeviceId); // e.g. "esp32fanctrl_AABBCC"
    String entityNameBase = String(mqttDeviceName); // e.g. "ESP32 Fan Controller"

    // --- 1. Fan Entity ---
    {
        String fanObjectId = "fan"; // Object ID for the fan entity
        String fanConfigTopic = getFullTopic("fan/" + String(mqttDeviceId) + "/" + fanObjectId + "/config", true);
        
        ArduinoJson::JsonDocument fanDoc;
        fanDoc["name"] = entityNameBase; // Or more specific like entityNameBase + " Fan"
        fanDoc["unique_id"] = entityUniqueIdBase + "_" + fanObjectId;
        fanDoc["availability_topic"] = mqttAvailabilityTopic;
        fanDoc["device"] = deviceObj; // Embed device info

        fanDoc["state_topic"] = mqttStatusTopic;
        fanDoc["state_value_template"] = "{{ value_json.fan_state }}"; // "ON" or "OFF"
        fanDoc["command_topic"] = mqttFanCommandTopic; // Topic to send "ON" or "OFF"

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
        fanDoc["optimistic"] = false; // We report state
        fanDoc["speed_range_min"] = 0; // Assuming 0% is a valid speed (can mean off or lowest speed)
        fanDoc["speed_range_max"] = 100;


        String fanPayload;
        serializeJson(fanDoc, fanPayload);
        if (mqttClient.publish(fanConfigTopic.c_str(), fanPayload.c_str(), true)) {
            if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY] Fan config published to %s\n", fanConfigTopic.c_str());
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY_ERR] Failed to publish fan config to %s\n", fanConfigTopic.c_str());
        }
    }

    // --- 2. Temperature Sensor ---
    if (tempSensorFound) { // Only publish if sensor is detected
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
        // tempDoc["force_update"] = true; // Optional: if you want HA to update even if value is same

        String tempPayload;
        serializeJson(tempDoc, tempPayload);
        if (mqttClient.publish(tempConfigTopic.c_str(), tempPayload.c_str(), true)) {
            if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY] Temperature sensor config published to %s\n", tempConfigTopic.c_str());
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY_ERR] Failed to publish temperature sensor config to %s\n", tempConfigTopic.c_str());
        }
    } else {
         // Optionally, publish an "unavailable" sensor or clear a previously published one
        String tempObjectId = "temperature";
        String tempConfigTopic = getFullTopic("sensor/" + String(mqttDeviceId) + "/" + tempObjectId + "/config", true);
        if (mqttClient.publish(tempConfigTopic.c_str(), "", true)) { // Empty payload clears retained message
            if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY] Temperature sensor NOT FOUND, cleared config at %s\n", tempConfigTopic.c_str());
        }
    }


    // --- 3. Fan RPM Sensor ---
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
        if (mqttClient.publish(rpmConfigTopic.c_str(), rpmPayload.c_str(), true)) {
            if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY] RPM sensor config published to %s\n", rpmConfigTopic.c_str());
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_DISCOVERY_ERR] Failed to publish RPM sensor config to %s\n", rpmConfigTopic.c_str());
        }
    }
    
    // --- Add other entities as needed (e.g., a select for mode, a number for manual speed if not covered by fan) ---
    // For simplicity, the fan entity already covers mode and percentage speed.

    if (serialDebugEnabled) Serial.println("[MQTT_DISCOVERY] Finished publishing discovery messages.");
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
            // When switching to MANUAL, if fan was off, set to a default speed or last manual speed
            if (fanSpeedPercentage == 0 && manualFanSpeedPercentage == 0) {
                manualFanSpeedPercentage = 50; // Default to 50% if was off
            } else if (fanSpeedPercentage == 0 && manualFanSpeedPercentage > 0) {
                // Keep last manualFanSpeedPercentage
            }
            // If fan was on, manualFanSpeedPercentage should already hold the target
            needsImmediateBroadcast = true;
            if (serialDebugEnabled) Serial.println("[MQTT_CMD] Mode set to MANUAL");
        } else {
            if (serialDebugEnabled) Serial.println("[MQTT_CMD_ERR] Unknown mode command payload. Use AUTO or MANUAL.");
        }
    } else if (topicStr.equals(mqttSpeedCommandTopic)) {
        // This topic is used by HA fan's percentage_command_topic
        // It should also imply MANUAL mode if a speed is set.
        int speed = messageTemp.toInt();
        if (speed >= 0 && speed <= 100) {
            isAutoMode = false; // Setting speed implies manual mode
            manualFanSpeedPercentage = speed;
            needsImmediateBroadcast = true; 
            if (serialDebugEnabled) Serial.printf("[MQTT_CMD] Manual speed target set to %d%% (mode set to MANUAL)\n", speed);
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_CMD_ERR] Invalid speed payload %d. Use 0-100.\n", speed);
        }
    } else if (topicStr.equals(mqttFanCommandTopic)) {
        // Handles "ON" / "OFF" from HA fan component
        if (messageTemp.equalsIgnoreCase("ON")) {
            isAutoMode = false; // "ON" implies manual control
            if (manualFanSpeedPercentage == 0) { // If it was set to 0% (off)
                manualFanSpeedPercentage = 50; // Default to 50% or last known speed > 0
            }
            // If manualFanSpeedPercentage is already > 0, it will use that value.
            setFanSpeed(manualFanSpeedPercentage); // This sets fanSpeedPercentage and needsImmediateBroadcast
            if (serialDebugEnabled) Serial.printf("[MQTT_CMD] Fan turned ON (Manual mode, speed %d%%)\n", manualFanSpeedPercentage);
        } else if (messageTemp.equalsIgnoreCase("OFF")) {
            isAutoMode = false; // "OFF" implies manual control
            manualFanSpeedPercentage = 0; // Store 0 as the target for manual mode
            setFanSpeed(0); // This sets fanSpeedPercentage to 0 and needsImmediateBroadcast
            if (serialDebugEnabled) Serial.println("[MQTT_CMD] Fan turned OFF (Manual mode, speed 0%)");
        } else {
            if (serialDebugEnabled) Serial.println("[MQTT_CMD_ERR] Unknown fan command payload. Use ON or OFF.");
        }
        needsImmediateBroadcast = true; // Ensure state is updated
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

    } else {
        if (serialDebugEnabled) Serial.println("[MQTT_RX] Unknown topic.");
    }
}
