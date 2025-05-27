#include "mqtt_handler.h"
#include "config.h" // For global variables like mqttClient, mqttServer, etc.
#include "fan_control.h" // For setFanSpeed
#include <ArduinoJson.h> // For parsing command payloads

// Define MQTT Topics (initialized in setupMQTT based on base topic from NVS)
String mqttStatusTopic = "";
String mqttCommandTopicSuffix = "/set"; // Suffix for commands related to this device
String mqttModeCommandTopic = "";   // Full topic: base + /mode/set
String mqttSpeedCommandTopic = "";  // Full topic: base + /speed/set
String mqttAvailabilityTopic = ""; // Full topic: base + /status or /online

unsigned long lastMqttConnectAttempt = 0;
const long mqttConnectRetryInterval = 5000; // Try to reconnect every 5 seconds

// Helper function to construct full topic paths
String getFullTopic(const String& suffix) {
    String topic = String(mqttBaseTopic);
    if (!topic.endsWith("/")) {
        topic += "/";
    }
    // Remove leading slash from suffix if present, as baseTopic should end with / or be empty
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

    // Construct dynamic topic names based on the base topic
    // Ensure mqttBaseTopic is loaded from NVS before this is called
    if (strlen(mqttBaseTopic) == 0) {
        if (serialDebugEnabled) Serial.println("[MQTT_ERR] Base topic is empty. Using default 'fancontroller'.");
        strcpy(mqttBaseTopic, "fancontroller"); // Fallback default if not set
    }
    
    mqttStatusTopic = getFullTopic("status_json"); // Publishes all data as a single JSON
    mqttModeCommandTopic = getFullTopic("mode" + mqttCommandTopicSuffix);
    mqttSpeedCommandTopic = getFullTopic("speed" + mqttCommandTopicSuffix);
    mqttAvailabilityTopic = getFullTopic("online_status");


    if (serialDebugEnabled) {
        Serial.printf("[MQTT] Server: %s:%d\n", mqttServer, mqttPort);
        Serial.printf("[MQTT] Base Topic: %s\n", mqttBaseTopic);
        Serial.printf("[MQTT] Status JSON Topic: %s\n", mqttStatusTopic.c_str());
        Serial.printf("[MQTT] Mode Command Topic: %s\n", mqttModeCommandTopic.c_str());
        Serial.printf("[MQTT] Speed Command Topic: %s\n", mqttSpeedCommandTopic.c_str());
        Serial.printf("[MQTT] Availability Topic: %s\n", mqttAvailabilityTopic.c_str());
    }

    mqttClient.setServer(mqttServer, mqttPort);
    mqttClient.setCallback(mqttCallback);
    // Set buffer size if you expect large messages, default is 256.
    // mqttClient.setBufferSize(512); 
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

    // Create a client ID
    String clientId = "ESP32FanController-";
    clientId += String(random(0xffff), HEX);

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
        // Publish initial status
        publishStatusMQTT();

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
        if (mqttClient.connected()) { // If WiFi drops, publish offline and disconnect
            publishMqttAvailability(false);
            mqttClient.disconnect();
             if (serialDebugEnabled) Serial.println("[MQTT] WiFi disconnected, MQTT client disconnected.");
        }
        return;
    }

    if (!mqttClient.connected()) {
        connectMQTT();
    }
    mqttClient.loop(); // Handles incoming messages and keeps connection alive
}

void publishStatusMQTT() {
    if (!isMqttEnabled || !mqttClient.connected()) {
        return;
    }

    ArduinoJson::JsonDocument doc;
    doc["temperature"] = tempSensorFound ? currentTemperature : -999.0;
    doc["tempSensorFound"] = tempSensorFound;
    doc["fanSpeedPercent"] = fanSpeedPercentage;
    doc["fanRpm"] = fanRpm;
    doc["mode"] = isAutoMode ? "AUTO" : "MANUAL";
    doc["manualSetSpeed"] = manualFanSpeedPercentage; // Current target for manual mode
    doc["ipAddress"] = WiFi.localIP().toString();
    doc["wifiRSSI"] = WiFi.RSSI();

    String output;
    serializeJson(doc, output);

    if (mqttClient.publish(mqttStatusTopic.c_str(), output.c_str(), true)) { // Retain status
        if (serialDebugEnabled) Serial.printf("[MQTT] Status Published to %s: %s\n", mqttStatusTopic.c_str(), output.c_str());
    } else {
        if (serialDebugEnabled) Serial.printf("[MQTT_ERR] Failed to publish status to %s\n", mqttStatusTopic.c_str());
    }
}

void publishMqttAvailability(bool available) {
    if (!isMqttEnabled || !mqttClient.connected()) { // Need to be connected to publish LWT's "online"
        if (serialDebugEnabled && !mqttClient.connected() && available) Serial.println("[MQTT] Cannot publish availability, not connected.");
        // If trying to publish "offline" and already disconnected, that's fine.
    }
    const char* message = available ? "online" : "offline";
    if (mqttClient.publish(mqttAvailabilityTopic.c_str(), message, true)) { // Retain availability
         if (serialDebugEnabled) Serial.printf("[MQTT] Availability '%s' published to %s\n", message, mqttAvailabilityTopic.c_str());
    } else {
         if (serialDebugEnabled) Serial.printf("[MQTT_ERR] Failed to publish availability to %s\n", mqttAvailabilityTopic.c_str());
    }
}


void mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (serialDebugEnabled) {
        Serial.print("[MQTT_RX] Message arrived [");
        Serial.print(topic);
        Serial.print("] ");
    }
    String messageTemp;
    for (int i = 0; i < length; i++) {
        messageTemp += (char)payload[i];
    }
    if (serialDebugEnabled) Serial.println(messageTemp);

    String topicStr = String(topic);

    if (topicStr.equals(mqttModeCommandTopic)) {
        if (messageTemp.equalsIgnoreCase("AUTO")) {
            isAutoMode = true;
            needsImmediateBroadcast = true; // For WebSocket and MQTT status update
            if (serialDebugEnabled) Serial.println("[MQTT_CMD] Mode set to AUTO");
        } else if (messageTemp.equalsIgnoreCase("MANUAL")) {
            isAutoMode = false;
            needsImmediateBroadcast = true;
            if (serialDebugEnabled) Serial.println("[MQTT_CMD] Mode set to MANUAL");
        } else {
            if (serialDebugEnabled) Serial.println("[MQTT_CMD_ERR] Unknown mode command payload. Use AUTO or MANUAL.");
        }
    } else if (topicStr.equals(mqttSpeedCommandTopic)) {
        if (isAutoMode) {
            if (serialDebugEnabled) Serial.println("[MQTT_CMD_WARN] Manual speed command received, but in AUTO mode. Ignoring.");
            return;
        }
        int speed = messageTemp.toInt();
        if (speed >= 0 && speed <= 100) {
            manualFanSpeedPercentage = speed;
            // The mainAppTask will pick this up and call setFanSpeed
            needsImmediateBroadcast = true; 
            if (serialDebugEnabled) Serial.printf("[MQTT_CMD] Manual speed target set to %d%%\n", speed);
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_CMD_ERR] Invalid speed payload %d. Use 0-100.\n", speed);
        }
    } else {
        if (serialDebugEnabled) Serial.println("[MQTT_RX] Unknown topic.");
    }
    // After processing, publish an updated status
    publishStatusMQTT();
}
