#include "nvs_handler.h"
#include "config.h" 
#include "fan_control.h" 

// NVS Helper Functions for WiFi
void saveWiFiConfig() {
    if (preferences.begin("wifi-cfg", false)) {
        preferences.putString("ssid", current_ssid);
        preferences.putString("password", current_password);
        if(serialDebugEnabled) Serial.printf("[NVS_SAVE] Saving 'wifiEn' as: %s\n", isWiFiEnabled ? "true" : "false");
        preferences.putBool("wifiEn", isWiFiEnabled);
        preferences.end();
        if(serialDebugEnabled) Serial.println("[NVS] WiFi configuration saved.");
    } else {
        if(serialDebugEnabled) Serial.println("[NVS_SAVE_ERR] Failed to open 'wifi-cfg' for writing.");
    }
}

void loadWiFiConfig() {
    if (preferences.begin("wifi-cfg", true)) { // Open read-only
        String stored_ssid = preferences.getString("ssid", "YOUR_WIFI_SSID"); // Provide default if not found
        String stored_password = preferences.getString("password", "YOUR_WIFI_PASSWORD"); // Provide default
        
        if (preferences.isKey("wifiEn")) {
            isWiFiEnabled = preferences.getBool("wifiEn");
            if(serialDebugEnabled) Serial.printf("[NVS_LOAD] 'wifiEn' key found. Loaded value: %s\n", isWiFiEnabled ? "true" : "false");
        } else {
            isWiFiEnabled = false; // Default to false if key doesn't exist
            if(serialDebugEnabled) Serial.println("[NVS_LOAD] 'wifiEn' key NOT found. Defaulting isWiFiEnabled to false.");
        }
        
        // Only copy if the stored value is different and valid
        if (stored_ssid.length() > 0 && stored_ssid.length() < sizeof(current_ssid)) {
             if (strcmp(current_ssid, stored_ssid.c_str()) != 0) { 
                strcpy(current_ssid, stored_ssid.c_str());
             }
        }
        if (stored_password.length() < sizeof(current_password)) { // Allow empty password
            if (strcmp(current_password, stored_password.c_str()) != 0) {
                strcpy(current_password, stored_password.c_str());
            }
        }
        preferences.end();
        if(serialDebugEnabled) Serial.printf("[NVS] Effective WiFi Config after load: SSID='%s', Enabled=%s\n", current_ssid, isWiFiEnabled ? "Yes" : "No");

    } else {
        if(serialDebugEnabled) Serial.println("[NVS_LOAD_ERR] Failed to open 'wifi-cfg' for reading. isWiFiEnabled defaults to false.");
        isWiFiEnabled = false; 
        // Set default SSID/Pass if loading failed entirely, though getString defaults should handle it
        strcpy(current_ssid, "YOUR_WIFI_SSID");
        strcpy(current_password, "YOUR_WIFI_PASSWORD");
    }
}

// NVS Helper Functions for Fan Curve
void saveFanCurveToNVS() {
  if (preferences.begin("fan-curve", false)) {
    preferences.putInt("numPoints", numCurvePoints);
    if(serialDebugEnabled) Serial.printf("[NVS] Saving fan curve with %d points:\n", numCurvePoints);
    for (int i = 0; i < numCurvePoints; i++) {
        String tempKey = "tP" + String(i);
        String pwmKey = "pP" + String(i);
        preferences.putInt(tempKey.c_str(), tempPoints[i]);
        preferences.putInt(pwmKey.c_str(), pwmPercentagePoints[i]);
        if(serialDebugEnabled) Serial.printf("  Point %d: Temp=%d, PWM=%d\n", i, tempPoints[i], pwmPercentagePoints[i]);
    }
    preferences.end();
    if(serialDebugEnabled) Serial.println("[NVS] Fan curve saved.");
  } else {
    if(serialDebugEnabled) Serial.println("[NVS_SAVE_ERR] Failed to open 'fan-curve' for writing.");
  }
}

void loadFanCurveFromNVS() {
  if(!preferences.begin("fan-curve", true)) { // Open read-only
    if(serialDebugEnabled) Serial.println("[NVS_LOAD_ERR] Failed to open 'fan-curve' for reading. Using default curve.");
    setDefaultFanCurve(); 
    return;
  }
  int savedNumPoints = preferences.getInt("numPoints", 0); // Default to 0 if not found
  if(serialDebugEnabled) Serial.printf("[NVS] Attempting to load fan curve. Found %d points in NVS.\n", savedNumPoints);

  if (savedNumPoints >= 2 && savedNumPoints <= MAX_CURVE_POINTS) {
    bool success = true;
    int tempTempPointsValidation[MAX_CURVE_POINTS]; // Temporary arrays for validation
    int tempPwmPercentagePointsValidation[MAX_CURVE_POINTS];

    for (int i = 0; i < savedNumPoints; i++) {
      String tempKey = "tP" + String(i);
      String pwmKey = "pP" + String(i);
      tempTempPointsValidation[i] = preferences.getInt(tempKey.c_str(), -1000); // Use a sentinel for not found/error
      tempPwmPercentagePointsValidation[i] = preferences.getInt(pwmKey.c_str(), -1000);
      
      // Validation logic
      if(tempTempPointsValidation[i] == -1000 || tempPwmPercentagePointsValidation[i] == -1000 || 
         tempTempPointsValidation[i] < 0 || tempTempPointsValidation[i] > 120 || 
         tempPwmPercentagePointsValidation[i] < 0 || tempPwmPercentagePointsValidation[i] > 100 ||
         (i > 0 && tempTempPointsValidation[i] <= tempTempPointsValidation[i-1])) { // Temp must be increasing
        if(serialDebugEnabled) Serial.printf("[NVS_VALIDATE_ERR] Invalid data for point %d. Temp: %d, PWM: %d\n", i, tempTempPointsValidation[i], tempPwmPercentagePointsValidation[i]);
        success = false;
        break;
      }
    }
    
    if (success) {
        numCurvePoints = savedNumPoints;
        for(int i=0; i < numCurvePoints; ++i) {
            tempPoints[i] = tempTempPointsValidation[i];
            pwmPercentagePoints[i] = tempPwmPercentagePointsValidation[i];
            if(serialDebugEnabled) Serial.printf("  Loaded Point %d: Temp=%d, PWM=%d\n", i, tempPoints[i], pwmPercentagePoints[i]);
        }
        if(serialDebugEnabled) Serial.println("[NVS] Fan curve successfully loaded from NVS.");
    } else {
        if(serialDebugEnabled) Serial.println("[NVS_LOAD_ERR] Error/Invalid data in NVS fan curve, using default curve.");
        setDefaultFanCurve(); 
    }
  } else {
    if(serialDebugEnabled) Serial.printf("[NVS] No valid fan curve in NVS (points: %d), using default curve.\n", savedNumPoints);
    setDefaultFanCurve(); 
  }
  preferences.end();
}


// --- NVS Helper Functions for MQTT ---
void saveMqttConfig() {
    if (preferences.begin("mqtt-cfg", false)) { // Open for writing
        preferences.putBool("mqttEn", isMqttEnabled);
        preferences.putString("mqttSrv", mqttServer);
        preferences.putInt("mqttPrt", mqttPort);
        preferences.putString("mqttUsr", mqttUser);
        preferences.putString("mqttPwd", mqttPassword);
        preferences.putString("mqttTop", mqttBaseTopic);
        preferences.end();
        if (serialDebugEnabled) Serial.println("[NVS] MQTT configuration saved.");
    } else {
        if (serialDebugEnabled) Serial.println("[NVS_SAVE_ERR] Failed to open 'mqtt-cfg' for writing.");
    }
}

void loadMqttConfig() {
    if (preferences.begin("mqtt-cfg", true)) { // Open read-only
        isMqttEnabled = preferences.getBool("mqttEn", false); // Default to false

        String tempServer = preferences.getString("mqttSrv", "your_broker_ip");
        strncpy(mqttServer, tempServer.c_str(), sizeof(mqttServer) - 1);
        mqttServer[sizeof(mqttServer) - 1] = '\0';

        mqttPort = preferences.getInt("mqttPrt", 1883);

        String tempUser = preferences.getString("mqttUsr", "");
        strncpy(mqttUser, tempUser.c_str(), sizeof(mqttUser) - 1);
        mqttUser[sizeof(mqttUser) - 1] = '\0';
        
        String tempPass = preferences.getString("mqttPwd", "");
        strncpy(mqttPassword, tempPass.c_str(), sizeof(mqttPassword) - 1);
        mqttPassword[sizeof(mqttPassword) - 1] = '\0';

        String tempTopic = preferences.getString("mqttTop", "fancontroller");
        strncpy(mqttBaseTopic, tempTopic.c_str(), sizeof(mqttBaseTopic) - 1);
        mqttBaseTopic[sizeof(mqttBaseTopic) - 1] = '\0';
        
        preferences.end();
        if (serialDebugEnabled) {
            Serial.println("[NVS] MQTT configuration loaded:");
            Serial.printf("  Enabled: %s\n", isMqttEnabled ? "Yes" : "No");
            Serial.printf("  Server: %s\n", mqttServer);
            Serial.printf("  Port: %d\n", mqttPort);
            Serial.printf("  User: %s\n", strlen(mqttUser) > 0 ? mqttUser : "N/A");
            Serial.printf("  Base Topic: %s\n", mqttBaseTopic);
        }
    } else {
        if (serialDebugEnabled) Serial.println("[NVS_LOAD_ERR] Failed to open 'mqtt-cfg' for reading. Using default MQTT values.");
        // Defaults are already set in main.cpp, ensure isMqttEnabled is false if load fails
        isMqttEnabled = false;
    }
}
