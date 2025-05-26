#include "nvs_handler.h"
#include "config.h" 
#include "fan_control.h" // Added for setDefaultFanCurve

// NVS Helper Functions
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
    if (preferences.begin("wifi-cfg", true)) {
        String stored_ssid = preferences.getString("ssid", current_ssid); 
        String stored_password = preferences.getString("password", current_password); 
        
        if (preferences.isKey("wifiEn")) {
            isWiFiEnabled = preferences.getBool("wifiEn");
            if(serialDebugEnabled) Serial.printf("[NVS_LOAD] 'wifiEn' key found. Loaded value: %s\n", isWiFiEnabled ? "true" : "false");
        } else {
            isWiFiEnabled = false; 
            if(serialDebugEnabled) Serial.println("[NVS_LOAD] 'wifiEn' key NOT found. Defaulting isWiFiEnabled to false.");
        }
        preferences.end();

        if (stored_ssid.length() > 0 && stored_ssid.length() < sizeof(current_ssid)) {
             if (strcmp(current_ssid, stored_ssid.c_str()) != 0) { 
                strcpy(current_ssid, stored_ssid.c_str());
             }
        }
        if (stored_password.length() < sizeof(current_password)) { 
            if (strcmp(current_password, stored_password.c_str()) != 0) {
                strcpy(current_password, stored_password.c_str());
            }
        }
        if(serialDebugEnabled) Serial.printf("[NVS] Effective WiFi Config after load: SSID='%s', Enabled=%s\n", current_ssid, isWiFiEnabled ? "Yes" : "No");

    } else {
        if(serialDebugEnabled) Serial.println("[NVS_LOAD_ERR] Failed to open 'wifi-cfg' for reading. isWiFiEnabled defaults to false.");
        isWiFiEnabled = false; 
    }
}

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
  if(!preferences.begin("fan-curve", true)) {
    if(serialDebugEnabled) Serial.println("[NVS_LOAD_ERR] Failed to open 'fan-curve' for reading. Using default curve.");
    setDefaultFanCurve(); 
    return;
  }
  int savedNumPoints = preferences.getInt("numPoints", 0);
  if(serialDebugEnabled) Serial.printf("[NVS] Attempting to load fan curve. Found %d points in NVS.\n", savedNumPoints);

  if (savedNumPoints >= 2 && savedNumPoints <= MAX_CURVE_POINTS) {
    bool success = true;
    int tempTempPoints[MAX_CURVE_POINTS];
    int tempPwmPercentagePoints[MAX_CURVE_POINTS];

    for (int i = 0; i < savedNumPoints; i++) {
      String tempKey = "tP" + String(i);
      String pwmKey = "pP" + String(i);
      tempTempPoints[i] = preferences.getInt(tempKey.c_str(), -1000);
      tempPwmPercentagePoints[i] = preferences.getInt(pwmKey.c_str(), -1000);
      if(tempTempPoints[i] == -1000 || tempPwmPercentagePoints[i] == -1000 || 
         tempTempPoints[i] < 0 || tempTempPoints[i] > 120 || 
         tempPwmPercentagePoints[i] < 0 || tempPwmPercentagePoints[i] > 100 ||
         (i > 0 && tempTempPoints[i] <= tempTempPoints[i-1])) {
        if(serialDebugEnabled) Serial.printf("[NVS] Invalid data for point %d. Temp: %d, PWM: %d\n", i, tempTempPoints[i], tempPwmPercentagePoints[i]);
        success = false;
        break;
      }
    }
    if (success) {
        numCurvePoints = savedNumPoints;
        for(int i=0; i < numCurvePoints; ++i) {
            tempPoints[i] = tempTempPoints[i];
            pwmPercentagePoints[i] = tempPwmPercentagePoints[i];
            if(serialDebugEnabled) Serial.printf("  Loaded Point %d: Temp=%d, PWM=%d\n", i, tempPoints[i], pwmPercentagePoints[i]);
        }
        if(serialDebugEnabled) Serial.println("[NVS] Fan curve successfully loaded from NVS.");
    } else {
        if(serialDebugEnabled) Serial.println("[NVS] Error/Invalid data in NVS fan curve, using default curve.");
        setDefaultFanCurve(); 
    }
  } else {
    if(serialDebugEnabled) Serial.println("[NVS] No valid fan curve in NVS or invalid point count, using default curve.");
    setDefaultFanCurve(); 
  }
  preferences.end();
}
