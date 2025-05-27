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
String mqttFanCurveGetTopic = "";    // New: For requesting the current curve
String mqttFanCurveStatusTopic = ""; // New: For publishing the current curve
String mqttFanCurveSetTopic = "";    // New: For setting a new curve

unsigned long lastMqttConnectAttempt = 0;
const long mqttConnectRetryInterval = 5000; // Try to reconnect every 5 seconds

// Helper function to construct full topic paths
String getFullTopic(const String& suffix) {
    String topic = String(mqttBaseTopic);
    if (strlen(mqttBaseTopic) > 0 && !topic.endsWith("/")) { // Add slash if base topic is not empty and doesn't have one
        topic += "/";
    } else if (strlen(mqttBaseTopic) == 0 && suffix.startsWith("/")){ // If base is empty, remove leading slash from suffix
        String tempSuffix = suffix;
        return tempSuffix.substring(1);
    }


    String cleanSuffix = suffix;
    if (cleanSuffix.startsWith("/")) { // Avoid double slashes if suffix also starts with one
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

    if (strlen(mqttBaseTopic) == 0) {
        if (serialDebugEnabled) Serial.println("[MQTT_WARN] Base topic is empty. Topics will be relative (e.g., 'status_json').");
        // Topics will be constructed without a leading base, e.g. "status_json" instead of "fancontroller/status_json"
    }
    
    mqttStatusTopic = getFullTopic("status_json"); 
    mqttModeCommandTopic = getFullTopic("mode/set");
    mqttSpeedCommandTopic = getFullTopic("speed/set");
    mqttAvailabilityTopic = getFullTopic("online_status");
    // ADDED: Initialize new fan curve topics
    mqttFanCurveGetTopic = getFullTopic("fancurve/get");
    mqttFanCurveStatusTopic = getFullTopic("fancurve/status");
    mqttFanCurveSetTopic = getFullTopic("fancurve/set");


    if (serialDebugEnabled) {
        Serial.printf("[MQTT] Server: %s:%d\n", mqttServer, mqttPort);
        Serial.printf("[MQTT] Base Topic (effective): '%s'\n", mqttBaseTopic); // Show the actual base topic used
        Serial.printf("[MQTT] Status JSON Topic: %s\n", mqttStatusTopic.c_str());
        Serial.printf("[MQTT] Mode Command Topic: %s\n", mqttModeCommandTopic.c_str());
        Serial.printf("[MQTT] Speed Command Topic: %s\n", mqttSpeedCommandTopic.c_str());
        Serial.printf("[MQTT] Availability Topic: %s\n", mqttAvailabilityTopic.c_str());
        Serial.printf("[MQTT] Fan Curve Get Topic: %s\n", mqttFanCurveGetTopic.c_str());    // ADDED
        Serial.printf("[MQTT] Fan Curve Status Topic: %s\n", mqttFanCurveStatusTopic.c_str()); // ADDED
        Serial.printf("[MQTT] Fan Curve Set Topic: %s\n", mqttFanCurveSetTopic.c_str());    // ADDED
    }

    mqttClient.setServer(mqttServer, mqttPort);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(512); // Increased buffer size for potentially larger fan curve JSON
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
        // ADDED: Subscribe to new fan curve topics
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
        
        publishStatusMQTT(); // Publish initial status
        publishFanCurveMQTT(); // ADDED: Publish initial fan curve

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

    ArduinoJson::JsonDocument doc; // Using dynamic allocation, ensure sufficient heap
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
        // Reduce serial spam for status, log only if significantly changed or periodically
        // if (serialDebugEnabled) Serial.printf("[MQTT] Status Published to %s: %s\n", mqttStatusTopic.c_str(), output.c_str());
    } else {
        if (serialDebugEnabled) Serial.printf("[MQTT_ERR] Failed to publish status to %s\n", mqttStatusTopic.c_str());
    }
}

// ADDED: Function to publish the fan curve
void publishFanCurveMQTT() {
    if (!isMqttEnabled || !mqttClient.connected()) {
        return;
    }

    ArduinoJson::JsonDocument curveDoc; // Dynamic allocation
    JsonArray curveArray = curveDoc.to<JsonArray>();
    for (int i = 0; i < numCurvePoints; i++) {
        JsonObject point = curveArray.add<JsonObject>();
        point["temp"] = tempPoints[i];
        point["pwmPercent"] = pwmPercentagePoints[i];
    }

    String curveString;
    serializeJson(curveDoc, curveString);

    if (mqttClient.publish(mqttFanCurveStatusTopic.c_str(), curveString.c_str(), true)) { // Retain fan curve
        if (serialDebugEnabled) Serial.printf("[MQTT] Fan Curve Published to %s: %s\n", mqttFanCurveStatusTopic.c_str(), curveString.c_str());
    } else {
        if (serialDebugEnabled) Serial.printf("[MQTT_ERR] Failed to publish fan curve to %s\n", mqttFanCurveStatusTopic.c_str());
    }
}


void publishMqttAvailability(bool available) {
    if (!isMqttEnabled) return;
    if (available && !mqttClient.connected()) {
        if (serialDebugEnabled) Serial.println("[MQTT] Cannot publish 'online' availability, not connected.");
        return;
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
            needsImmediateBroadcast = true;
            if (serialDebugEnabled) Serial.println("[MQTT_CMD] Mode set to MANUAL");
        } else {
            if (serialDebugEnabled) Serial.println("[MQTT_CMD_ERR] Unknown mode command payload. Use AUTO or MANUAL.");
        }
    } else if (topicStr.equals(mqttSpeedCommandTopic)) {
        if (isAutoMode) {
            if (serialDebugEnabled) Serial.println("[MQTT_CMD_WARN] Manual speed command received, but in AUTO mode. Ignoring.");
            return; // Important: return to avoid further processing for this message
        }
        int speed = messageTemp.toInt();
        if (speed >= 0 && speed <= 100) {
            manualFanSpeedPercentage = speed;
            needsImmediateBroadcast = true; 
            if (serialDebugEnabled) Serial.printf("[MQTT_CMD] Manual speed target set to %d%%\n", speed);
        } else {
            if (serialDebugEnabled) Serial.printf("[MQTT_CMD_ERR] Invalid speed payload %d. Use 0-100.\n", speed);
        }
    } 
    // ADDED: Handle fan curve topics
    else if (topicStr.equals(mqttFanCurveGetTopic)) {
        if (serialDebugEnabled) Serial.println("[MQTT_CMD] Received request to publish fan curve.");
        publishFanCurveMQTT(); // Publish the current curve
    } else if (topicStr.equals(mqttFanCurveSetTopic)) {
        if (serialDebugEnabled) Serial.println("[MQTT_CMD] Received new fan curve.");
        if (!tempSensorFound) {
            if(serialDebugEnabled) Serial.println("[MQTT_CMD_WARN] Ignored setCurve, temperature sensor not found.");
            return; // Important: return
        }
        
        ArduinoJson::JsonDocument newCurveDoc; // Use dynamic allocation
        DeserializationError error = deserializeJson(newCurveDoc, messageTemp);

        if (error) {
            if(serialDebugEnabled) Serial.printf("[MQTT_CMD_ERR] deserializeJson() for fan curve failed: %s\n", error.c_str());
            return; // Important: return
        }

        JsonArray newCurveArray = newCurveDoc.as<JsonArray>();
        if (!newCurveArray || newCurveArray.size() < 2 || newCurveArray.size() > MAX_CURVE_POINTS) {
            if(serialDebugEnabled) Serial.printf("[MQTT_CMD_ERR] Invalid fan curve array. Size: %d (must be 2-%d).\n", newCurveArray.size(), MAX_CURVE_POINTS);
            return; // Important: return
        }

        bool curveValid = true;
        int lastTemp = -100; // Allows 0 as a valid first temperature
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
            fanCurveChanged = true; // Signal that the curve was updated
            needsImmediateBroadcast = true; // This will trigger status & curve publish in networkTask
        } else {
            if(serialDebugEnabled) Serial.println("[SYSTEM_ERR] New fan curve from MQTT rejected due to invalid data.");
        }

    } else {
        if (serialDebugEnabled) Serial.println("[MQTT_RX] Unknown topic.");
    }
    // After processing some commands, an updated status might be useful.
    // needsImmediateBroadcast will handle this for most cases.
    // If it was just a 'get' request, the specific data was already published.
}
