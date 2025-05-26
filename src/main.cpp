#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h> 
#include <Preferences.h>

// --- Configuration ---
char current_ssid[64] = "YOUR_WIFI_SSID"; 
char current_password[64] = "YOUR_WIFI_PASSWORD"; 

Preferences preferences; // Global preferences object

// Pin Definitions
const int FAN_PWM_PIN = 18;
const int BTN_MENU_PIN = 19;    
const int BTN_UP_PIN = 23;      
const int BTN_DOWN_PIN = 22;    
const int BTN_SELECT_PIN = 21;  
const int BTN_BACK_PIN = 5;     
const int DEBUG_ENABLE_PIN = 4; // GPIO to enable Serial Command Interface (HIGH = enabled)
const int LED_DEBUG_PIN = 2;    // GPIO for Debug LED (usually on-board D2 for many ESP32 dev boards)


const int FAN_TACH_PIN = 2; // NOTE: This is the same as LED_DEBUG_PIN. This will cause a conflict.
                            // Assuming FAN_TACH_PIN should be a different, dedicated pin.
                            // For this example, I will change FAN_TACH_PIN to GPIO 15. Please adjust as needed.
const int FAN_TACH_PIN_ACTUAL = 15; // Changed from 2 to avoid conflict with LED_DEBUG_PIN


// Fan Control
const int PWM_CHANNEL = 0;
const int PWM_FREQ = 25000;
const int PWM_RESOLUTION_BITS = 8;
volatile int fanSpeedPWM_Raw = 0;
volatile int fanSpeedPercentage = 0;
const int AUTO_MODE_NO_SENSOR_FAN_PERCENTAGE = 60;

// Fan RPM Reading
volatile unsigned long pulseCount = 0;
unsigned long lastRpmReadTime_Task = 0;
volatile int fanRpm = 0;
const int PULSES_PER_REVOLUTION = 2;

// Modes & States
volatile bool isAutoMode = true;
volatile int manualFanSpeedPercentage = 50;
volatile bool isInMenuMode = false;
volatile bool isWiFiEnabled = false; 
volatile bool serialDebugEnabled = false; // Flag to indicate if serial commands are active

// Temperature Sensor
Adafruit_BMP280 bmp;
volatile float currentTemperature = -999.0; 
volatile bool tempSensorFound = false;      

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// Web Server and WebSockets
AsyncWebServer server(80);
WebSocketsServer webSocket(81);

// Button Debouncing & Long Press
unsigned long buttonPressTime[5]; 
bool buttonPressedState[5];
const long debounceDelay = 50; 
const long longPressDelay = 1000; 

// Menu System Variables
enum MenuScreen { MAIN_MENU, WIFI_SETTINGS, WIFI_SCAN, WIFI_PASSWORD_ENTRY, WIFI_STATUS, VIEW_STATUS, CONFIRM_REBOOT };
volatile MenuScreen currentMenuScreen = MAIN_MENU;
volatile int selectedMenuItem = 0;
volatile int scanResultCount = 0;
String scannedSSIDs[10]; 
char passwordInputBuffer[64] = "";
volatile int passwordCharIndex = 0;
volatile char currentPasswordEditChar = 'a';

// Fan Curve
const int MAX_CURVE_POINTS = 8;
int tempPoints[MAX_CURVE_POINTS];
int pwmPercentagePoints[MAX_CURVE_POINTS];
int numCurvePoints = 0;

// Staging Fan Curve for Serial Commands
int stagingTempPoints[MAX_CURVE_POINTS];
int stagingPwmPercentagePoints[MAX_CURVE_POINTS];
int stagingNumCurvePoints = 0;


// Task Handles
TaskHandle_t networkTaskHandle = NULL;
TaskHandle_t mainAppTaskHandle = NULL;

volatile bool needsImmediateBroadcast = false;
volatile bool rebootNeeded = false; 

// --- Forward Declarations ---
void setDefaultFanCurve();
void updateLCD_NormalMode(); 
void broadcastWebSocketData(); 
void setFanSpeed(int percentage); 
void handleMenuInput();
void displayMenu();
void displayMainMenu();
void displayWiFiSettingsMenu();
void displayWiFiScanMenu();
void displayPasswordEntryMenu();
void displayWiFiStatusMenu();
void displayConfirmRebootMenu();
void performWiFiScan();
void attemptWiFiConnection();
void disconnectWiFi();
void handleSerialCommands();


// --- Interrupt Service Routine for RPM ---
void IRAM_ATTR countPulse() {
  pulseCount++;
}

// --- NVS Helper Functions ---
void saveWiFiConfig() {
    if (preferences.begin("wifi-cfg", false)) {
        preferences.putString("ssid", current_ssid);
        preferences.putString("password", current_password);
        Serial.printf("[NVS_SAVE] Saving 'wifiEn' as: %s\n", isWiFiEnabled ? "true" : "false");
        preferences.putBool("wifiEn", isWiFiEnabled);
        preferences.end();
        Serial.println("[NVS] WiFi configuration saved.");
    } else {
        Serial.println("[NVS_SAVE_ERR] Failed to open 'wifi-cfg' for writing.");
    }
}

void loadWiFiConfig() {
    if (preferences.begin("wifi-cfg", true)) {
        String stored_ssid = preferences.getString("ssid", current_ssid); 
        String stored_password = preferences.getString("password", current_password); 
        
        if (preferences.isKey("wifiEn")) {
            isWiFiEnabled = preferences.getBool("wifiEn");
            Serial.printf("[NVS_LOAD] 'wifiEn' key found. Loaded value: %s\n", isWiFiEnabled ? "true" : "false");
        } else {
            isWiFiEnabled = false; 
            Serial.println("[NVS_LOAD] 'wifiEn' key NOT found. Defaulting isWiFiEnabled to false.");
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
        Serial.printf("[NVS] Effective WiFi Config after load: SSID='%s', Enabled=%s\n", current_ssid, isWiFiEnabled ? "Yes" : "No");

    } else {
        Serial.println("[NVS_LOAD_ERR] Failed to open 'wifi-cfg' for reading. isWiFiEnabled defaults to false.");
        isWiFiEnabled = false; 
    }
}


void saveFanCurveToNVS() {
  if (preferences.begin("fan-curve", false)) {
    preferences.putInt("numPoints", numCurvePoints);
    Serial.printf("[NVS] Saving fan curve with %d points:\n", numCurvePoints);
    for (int i = 0; i < numCurvePoints; i++) {
        String tempKey = "tP" + String(i);
        String pwmKey = "pP" + String(i);
        preferences.putInt(tempKey.c_str(), tempPoints[i]);
        preferences.putInt(pwmKey.c_str(), pwmPercentagePoints[i]);
        Serial.printf("  Point %d: Temp=%d, PWM=%d\n", i, tempPoints[i], pwmPercentagePoints[i]);
    }
    preferences.end();
    Serial.println("[NVS] Fan curve saved.");
  } else {
    Serial.println("[NVS_SAVE_ERR] Failed to open 'fan-curve' for writing.");
  }
}

void loadFanCurveFromNVS() {
  if(!preferences.begin("fan-curve", true)) {
    Serial.println("[NVS_LOAD_ERR] Failed to open 'fan-curve' for reading. Using default curve.");
    setDefaultFanCurve(); 
    return;
  }
  int savedNumPoints = preferences.getInt("numPoints", 0);
  Serial.printf("[NVS] Attempting to load fan curve. Found %d points in NVS.\n", savedNumPoints);

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
        Serial.printf("[NVS] Invalid data for point %d. Temp: %d, PWM: %d\n", i, tempTempPoints[i], tempPwmPercentagePoints[i]);
        success = false;
        break;
      }
    }
    if (success) {
        numCurvePoints = savedNumPoints;
        for(int i=0; i < numCurvePoints; ++i) {
            tempPoints[i] = tempTempPoints[i];
            pwmPercentagePoints[i] = tempPwmPercentagePoints[i];
            Serial.printf("  Loaded Point %d: Temp=%d, PWM=%d\n", i, tempPoints[i], pwmPercentagePoints[i]);
        }
        Serial.println("[NVS] Fan curve successfully loaded from NVS.");
    } else {
        Serial.println("[NVS] Error/Invalid data in NVS fan curve, using default curve.");
        setDefaultFanCurve(); 
    }
  } else {
    Serial.println("[NVS] No valid fan curve in NVS or invalid point count, using default curve.");
    setDefaultFanCurve(); 
  }
  preferences.end();
}

// --- Core Helper Functions Definitions ---
void setDefaultFanCurve() {
    numCurvePoints = 5;
    tempPoints[0] = 25; pwmPercentagePoints[0] = 0;  
    tempPoints[1] = 35; pwmPercentagePoints[1] = 20; 
    tempPoints[2] = 45; pwmPercentagePoints[2] = 50; 
    tempPoints[3] = 55; pwmPercentagePoints[3] = 80; 
    tempPoints[4] = 60; pwmPercentagePoints[4] = 100;
    Serial.println("[SYSTEM] Default fan curve set.");
}

int calculateAutoFanPWMPercentage(float temp) {
    if (!tempSensorFound) { 
        return AUTO_MODE_NO_SENSOR_FAN_PERCENTAGE;
    }
    if (numCurvePoints == 0) return 0; 
    if (temp <= tempPoints[0]) return pwmPercentagePoints[0];
    if (temp >= tempPoints[numCurvePoints - 1]) return pwmPercentagePoints[numCurvePoints - 1];

    for (int i = 0; i < numCurvePoints - 1; i++) {
        if (temp >= tempPoints[i] && temp < tempPoints[i+1]) {
            float tempRange = tempPoints[i+1] - tempPoints[i];
            float pwmRange = pwmPercentagePoints[i+1] - pwmPercentagePoints[i];
            if (tempRange <= 0) return pwmPercentagePoints[i]; 
            float tempOffset = temp - tempPoints[i];
            int calculatedPwm = pwmPercentagePoints[i] + (tempOffset / tempRange) * pwmRange;
            return calculatedPwm;
        }
    }
    return pwmPercentagePoints[numCurvePoints - 1]; 
}

void setFanSpeed(int percentage) {
    fanSpeedPercentage = constrain(percentage, 0, 100);
    fanSpeedPWM_Raw = map(fanSpeedPercentage, 0, 100, 0, (1 << PWM_RESOLUTION_BITS) - 1);
    ledcWrite(PWM_CHANNEL, fanSpeedPWM_Raw);
    needsImmediateBroadcast = true;
}

void updateLCD_NormalMode() { 
    lcd.clear();
    lcd.setCursor(0, 0);
    String line0 = "";
    line0 += (isAutoMode ? "AUTO" : "MANUAL");
    if (isWiFiEnabled && WiFi.status() == WL_CONNECTED) {
        String ipStr = WiFi.localIP().toString();
        if (ipStr.length() <= 9) {
             line0 += (isAutoMode ? " " : "  "); 
             line0 += ipStr;
        } else if (ipStr.length() <= 6) {
             line0 += "      "; 
             line0 += ipStr;
        }
    } else if (isWiFiEnabled) {
        line0 += (isAutoMode ? " " : "  ");
        line0 += "WiFi...";
    } else {
        line0 += (isAutoMode ? " " : "  ");
        line0 += "WiFi OFF";
    }
    lcd.print(line0);
    
    lcd.setCursor(0, 1);
    String line1 = "T:";
    if (!tempSensorFound || currentTemperature <= -990.0) { 
        line1 += "N/A "; 
    } else {
        line1 += String(currentTemperature, 1);
    }
    
    line1 += " F:";
    if(fanSpeedPercentage < 10) line1 += " ";
    if(fanSpeedPercentage < 100) line1 += " ";
    line1 += String(fanSpeedPercentage);
    line1 += "%";

    String rpmStr;
    if (fanRpm > 0) {
        if (fanRpm >= 1000) {
            rpmStr = String(fanRpm / 1000.0, 1) + "K";
        } else {
            rpmStr = String(fanRpm);
        }
        line1 += " R"; 
        line1 += rpmStr;
    }
    lcd.print(line1.substring(0,16)); 
}

void broadcastWebSocketData() {
    if (!isWiFiEnabled || WiFi.status() != WL_CONNECTED) return;
    
    ArduinoJson::JsonDocument jsonDoc; 

    jsonDoc["temperature"] = tempSensorFound ? currentTemperature : -999.0; 
    jsonDoc["tempSensorFound"] = tempSensorFound; 
    jsonDoc["fanSpeed"] = fanSpeedPercentage;
    jsonDoc["isAutoMode"] = isAutoMode;
    jsonDoc["manualFanSpeed"] = manualFanSpeedPercentage;
    jsonDoc["fanRpm"] = fanRpm;
    jsonDoc["isWiFiEnabled"] = isWiFiEnabled; 

    ArduinoJson::JsonArray curveArray = jsonDoc["fanCurve"].to<ArduinoJson::JsonArray>();
    for (int i = 0; i < numCurvePoints; i++) {
        ArduinoJson::JsonObject point = curveArray.add<ArduinoJson::JsonObject>();
        point["temp"] = tempPoints[i];
        point["pwmPercent"] = pwmPercentagePoints[i];
    }
    String jsonString;
    serializeJson(jsonDoc, jsonString);
    webSocket.broadcastTXT(jsonString);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    if (!isWiFiEnabled) return; 

    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[WS][%u] Client Disconnected!\n", num);
            break;
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[WS][%u] Client Connected from %s, URL: %s\n", num, ip.toString().c_str(), payload);
            needsImmediateBroadcast = true;
            break;
        }
        case WStype_TEXT: {
            Serial.printf("[WS][%u] Received Text: %s\n", num, payload);
            ArduinoJson::JsonDocument doc; 
            
            DeserializationError error = deserializeJson(doc, payload, length);

            if (error) {
                Serial.print(F("[WS] deserializeJson() failed: "));
                Serial.println(error.f_str());
                return;
            }
            const char* action = doc["action"];
            if (action) {
                Serial.printf("[WS] Action received: %s\n", action);
                if (strcmp(action, "setModeAuto") == 0) {
                    isAutoMode = true;
                    needsImmediateBroadcast = true;
                    Serial.println("[SYSTEM] Mode changed to AUTO via WebSocket.");
                } else if (strcmp(action, "setModeManual") == 0) {
                    isAutoMode = false;
                    needsImmediateBroadcast = true;
                    Serial.println("[SYSTEM] Mode changed to MANUAL via WebSocket.");
                } else if (strcmp(action, "setManualSpeed") == 0) {
                    if (!isAutoMode) {
                        manualFanSpeedPercentage = doc["value"];
                        needsImmediateBroadcast = true;
                        Serial.printf("[SYSTEM] Manual speed set to %d%% via WebSocket.\n", manualFanSpeedPercentage);
                    } else {
                        Serial.println("[WS] Ignored setManualSpeed, not in manual mode.");
                    }
                } else if (strcmp(action, "setCurve") == 0) {
                    if (!tempSensorFound) {
                        Serial.println("[WS] Ignored setCurve, temperature sensor not found.");
                        break; 
                    }
                    ArduinoJson::JsonArray newCurve = doc["curve"];
                    if (newCurve) { 
                        Serial.println("[WS] Received new fan curve data.");
                        int newNumPoints = newCurve.size();
                        if (newNumPoints >= 2 && newNumPoints <= MAX_CURVE_POINTS) {
                            bool curveValid = true;
                            int lastTemp = -100; 
                            int tempTempPointsValidation[MAX_CURVE_POINTS];
                            int tempPwmPercentagePointsValidation[MAX_CURVE_POINTS]; 

                            for(int i=0; i < newNumPoints; ++i) {
                                ArduinoJson::JsonObject point = newCurve[i];
                                int t = point["temp"];
                                int p = point["pwmPercent"];

                                if (t < 0 || t > 120 || p < 0 || p > 100 || (i > 0 && t <= lastTemp) ) {
                                    curveValid = false;
                                    Serial.printf("[WS] Invalid curve point %d received: Temp=%d, PWM=%d\n", i, t, p);
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
                                Serial.println("[SYSTEM] Fan curve updated and validated via WebSocket.");
                                saveFanCurveToNVS();
                                needsImmediateBroadcast = true;
                            } else {
                                Serial.println("[SYSTEM] New fan curve from web rejected due to invalid data.");
                            }
                        } else {
                             Serial.printf("[SYSTEM] Invalid number of curve points (%d) from web. Must be 2-%d.\n", newNumPoints, MAX_CURVE_POINTS);
                        }
                    } else {
                        Serial.println("[WS] 'setCurve' action received, but 'curve' array missing or invalid.");
                    }
                } else {
                    Serial.printf("[WS] Unknown action received: %s\n", action);
                }
            } else {
                 Serial.println("[WS] Received text, but no 'action' field found in JSON.");
            }
            break;
        }
        default: 
            Serial.printf("[WS][%u] Received unhandled WStype: %d\n", num, type);
            break;
    }
}

// --- Menu System Functions (LCD Output Only) ---
void displayMenu() {
    lcd.clear();
    switch (currentMenuScreen) {
        case MAIN_MENU: displayMainMenu(); break;
        case WIFI_SETTINGS: displayWiFiSettingsMenu(); break;
        case WIFI_SCAN: displayWiFiScanMenu(); break;
        case WIFI_PASSWORD_ENTRY: displayPasswordEntryMenu(); break;
        case WIFI_STATUS: displayWiFiStatusMenu(); break;
        case CONFIRM_REBOOT: displayConfirmRebootMenu(); break;
        default: 
            lcd.print("Unknown Menu"); 
            break;
    }
}

void displayMainMenu() {
    lcd.setCursor(0, 0); lcd.print(selectedMenuItem == 0 ? ">WiFi Settings" : " WiFi Settings");
    lcd.setCursor(0, 1); lcd.print(selectedMenuItem == 1 ? ">View Status" : " View Status");
}

void displayWiFiSettingsMenu() {
    String line0 = (selectedMenuItem == 0 ? ">WiFi: " : " WiFi: ");
    line0 += (isWiFiEnabled ? "Enabled" : "Disabled");
    String line1 = "";

    switch(selectedMenuItem) {
        case 0: line1 = " (Toggle State)"; break;
        case 1: line1 = ">Scan Networks"; break;
        case 2: line1 = ">SSID: "; line1 += String(current_ssid).substring(0,8); break;
        case 3: line1 = ">Password Set"; break;
        case 4: line1 = ">Connect WiFi"; break;
        case 5: line1 = ">DisconnectWiFi"; break;
        case 6: line1 = ">Back to Main"; break;
        default: line1 = " Select Option"; break;
    }
    if (selectedMenuItem > 0 && selectedMenuItem <=6) { 
        line0 = " WiFi: "; line0 += (isWiFiEnabled ? "Enabled" : "Disabled");
    } else if (selectedMenuItem != 0) { 
        line0 = " WiFi: "; line0 += (isWiFiEnabled ? "Enabled" : "Disabled");
    }

    lcd.setCursor(0, 0); lcd.print(line0.substring(0,16));
    lcd.setCursor(0, 1); lcd.print(line1.substring(0,16));
}

void displayWiFiScanMenu() {
    lcd.setCursor(0,0);
    if (scanResultCount == -1) { 
        lcd.print("Scanning WiFi..");
        lcd.setCursor(0,1); lcd.print("Please wait...");
        return;
    }
    if (scanResultCount == 0) {
        lcd.print("No Networks Found");
        lcd.setCursor(0,1); lcd.print("Press BACK");
        return;
    }
    lcd.print("Select Network:");
    lcd.setCursor(0,1);
    if (selectedMenuItem < scanResultCount) {
        lcd.print(">");
        lcd.print(scannedSSIDs[selectedMenuItem].substring(0,15)); 
    } else {
        lcd.print("Scroll Error");
    }
}

void displayPasswordEntryMenu() {
    lcd.setCursor(0,0); lcd.print("Enter Pass Char:");
    String passMask = "";
    for(int i=0; i < passwordCharIndex; ++i) passMask += "*";
    passMask += currentPasswordEditChar;
    lcd.setCursor(0,1); lcd.print(passMask.substring(0,16));
}

void displayWiFiStatusMenu(){
    lcd.setCursor(0,0); lcd.print("WiFi: "); lcd.print(isWiFiEnabled ? "ON" : "OFF");
    lcd.setCursor(0,1);
    if(isWiFiEnabled && WiFi.status() == WL_CONNECTED){
        lcd.print(WiFi.localIP());
    } else if (isWiFiEnabled) {
        lcd.print("Connecting...");
    } else {
        lcd.print("Disabled.");
    }
}

void displayConfirmRebootMenu() {
    lcd.setCursor(0,0); lcd.print("Reboot needed!");
    String line1 = (selectedMenuItem == 0 ? ">Yes " : " Yes ");
    line1 += (selectedMenuItem == 1 ? ">No" : " No");
    lcd.setCursor(0,1); lcd.print(line1);
}


void performWiFiScan() {
    Serial.println("[MENU_LCD] Starting WiFi Scan...");
    scanResultCount = -1; 
    displayMenu(); 
    
    WiFi.disconnect(); 
    delay(100);
    int n = WiFi.scanNetworks();
    Serial.printf("[WiFi] Scan found %d networks (for LCD menu).\n", n);

    if (n == 0) {
        scanResultCount = 0;
    } else {
        scanResultCount = min(n, 10); 
        for (int i = 0; i < scanResultCount; ++i) {
            scannedSSIDs[i] = WiFi.SSID(i);
        }
    }
    selectedMenuItem = 0; 
}

void attemptWiFiConnection() {
    if (!isWiFiEnabled) {
        Serial.println("[WiFi_LCD] Attempt connection: WiFi is disabled by user.");
        lcd.clear(); lcd.print("WiFi Disabled!"); 
        delay(1500);
        currentMenuScreen = WIFI_SETTINGS; 
        selectedMenuItem = 0;
        return;
    }
    if (strlen(current_ssid) == 0 || strcmp(current_ssid, "YOUR_WIFI_SSID") == 0) {
        Serial.println("[WiFi_LCD] Attempt connection: SSID not set or is default.");
        lcd.clear(); lcd.print("SSID Not Set!"); 
        delay(1500);
        currentMenuScreen = WIFI_SETTINGS; 
        selectedMenuItem = 2; 
        return;
    }

    Serial.printf("[WiFi_LCD] Attempting to connect to SSID: %s\n", current_ssid);
    lcd.clear();
    lcd.print("Connecting to:"); 
    lcd.setCursor(0,1);
    lcd.print(String(current_ssid).substring(0,16)); 
    
    WiFi.disconnect(true); 
    delay(100);
    WiFi.begin(current_ssid, current_password);
    
    int timeout = 0;
    while(WiFi.status() != WL_CONNECTED && timeout < 30) { 
        delay(500);
        Serial.print("."); 
        timeout++;
    }
    if(WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi_LCD] Connection successful!");
        lcd.clear(); lcd.print("Connected!"); 
        lcd.setCursor(0,1); lcd.print(WiFi.localIP()); 
        saveWiFiConfig(); 
        rebootNeeded = true; 
        currentMenuScreen = CONFIRM_REBOOT;
        selectedMenuItem = 0;
    } else {
        Serial.println("\n[WiFi_LCD] Connection failed.");
        lcd.clear(); lcd.print("Connect Failed"); 
        currentMenuScreen = WIFI_SETTINGS; 
        selectedMenuItem = 0; 
    }
    delay(2000); 
}

void disconnectWiFi(){ // Called by LCD Menu
    Serial.println("[WiFi_LCD] Disconnecting WiFi via menu...");
    WiFi.disconnect(true);
    delay(100);
    lcd.clear(); lcd.print("WiFi Dscnnctd"); 
    delay(1000);
}


void handleMenuInput() {
    bool button_states[5]; 
    int button_pins[5] = {BTN_MENU_PIN, BTN_UP_PIN, BTN_DOWN_PIN, BTN_SELECT_PIN, BTN_BACK_PIN};
    unsigned long currentTime = millis();

    for(int i=0; i<5; ++i) {
        button_states[i] = digitalRead(button_pins[i]) == LOW;
        if (button_states[i] && !buttonPressedState[i] && (currentTime - buttonPressTime[i] > debounceDelay)) {
            buttonPressTime[i] = currentTime;
            buttonPressedState[i] = true;

            if (i == 0) { 
                isInMenuMode = !isInMenuMode;
                if (isInMenuMode) {
                    Serial.println("[MENU_LCD] Entered Menu Mode.");
                    currentMenuScreen = MAIN_MENU; 
                    selectedMenuItem = 0;
                } else {
                    Serial.println("[MENU_LCD] Exited Menu Mode.");
                    if (rebootNeeded) { 
                        currentMenuScreen = CONFIRM_REBOOT; 
                        isInMenuMode = true; 
                        selectedMenuItem = 0;
                    }
                }
            }

            if (!isInMenuMode && i > 0) continue; 
            if (!isInMenuMode && i == 0) { 
                 if(!rebootNeeded) updateLCD_NormalMode(); else displayMenu();
                 return;
            }
            if (!isInMenuMode) return; 

            switch(i) { 
                case 1: // UP
                    if (currentMenuScreen == WIFI_PASSWORD_ENTRY) {
                        currentPasswordEditChar++;
                        if (currentPasswordEditChar > '~') currentPasswordEditChar = ' '; 
                    } else if (selectedMenuItem > 0) {
                        selectedMenuItem--;
                    }
                    break;
                case 2: // DOWN
                    if (currentMenuScreen == WIFI_PASSWORD_ENTRY) {
                        currentPasswordEditChar--;
                        if (currentPasswordEditChar < ' ') currentPasswordEditChar = '~';
                    } else {
                        int maxItems = 0;
                        if (currentMenuScreen == MAIN_MENU) maxItems = 1;
                        else if (currentMenuScreen == WIFI_SETTINGS) maxItems = 6; 
                        else if (currentMenuScreen == WIFI_SCAN) maxItems = scanResultCount > 0 ? scanResultCount - 1 : 0;
                        else if (currentMenuScreen == CONFIRM_REBOOT) maxItems = 1;
                        if (selectedMenuItem < maxItems) selectedMenuItem++;
                    }
                    break;
                case 3: // SELECT
                    Serial.printf("[MENU_LCD_ACTION] Screen: %d, Item: %d\n", currentMenuScreen, selectedMenuItem);
                    if (currentMenuScreen == MAIN_MENU) {
                        if (selectedMenuItem == 0) { currentMenuScreen = WIFI_SETTINGS; selectedMenuItem = 0; }
                        else if (selectedMenuItem == 1) { isInMenuMode = false; if(rebootNeeded){currentMenuScreen = CONFIRM_REBOOT; isInMenuMode=true; selectedMenuItem=0;}}
                    } else if (currentMenuScreen == WIFI_SETTINGS) {
                        if (selectedMenuItem == 0) { 
                            isWiFiEnabled = !isWiFiEnabled;
                            saveWiFiConfig();
                            rebootNeeded = true;
                            currentMenuScreen = CONFIRM_REBOOT; selectedMenuItem = 0;
                        } else if (selectedMenuItem == 1) { 
                            currentMenuScreen = WIFI_SCAN; selectedMenuItem = 0; performWiFiScan();
                        } else if (selectedMenuItem == 2) { 
                            currentMenuScreen = WIFI_SCAN; selectedMenuItem = 0; performWiFiScan(); 
                        } else if (selectedMenuItem == 3) { 
                            passwordCharIndex = 0; currentPasswordEditChar = 'a'; memset(passwordInputBuffer, 0, sizeof(passwordInputBuffer));
                            currentMenuScreen = WIFI_PASSWORD_ENTRY; selectedMenuItem = 0;
                        } else if (selectedMenuItem == 4) { 
                            attemptWiFiConnection(); 
                        } else if (selectedMenuItem == 5) { 
                            disconnectWiFi();
                        } else if (selectedMenuItem == 6) { 
                            currentMenuScreen = MAIN_MENU; selectedMenuItem = 0;
                        }
                    } else if (currentMenuScreen == WIFI_SCAN) {
                        if (scanResultCount > 0 && selectedMenuItem < scanResultCount) {
                            strncpy(current_ssid, scannedSSIDs[selectedMenuItem].c_str(), sizeof(current_ssid) - 1);
                            current_ssid[sizeof(current_ssid)-1] = '\0';
                            Serial.printf("[MENU_LCD] SSID Selected: %s\n", current_ssid);
                            currentMenuScreen = WIFI_SETTINGS; selectedMenuItem = 3; 
                        }
                    } else if (currentMenuScreen == WIFI_PASSWORD_ENTRY) { 
                        if (passwordCharIndex < sizeof(passwordInputBuffer) - 2) {
                            passwordInputBuffer[passwordCharIndex] = currentPasswordEditChar;
                            passwordCharIndex++;
                            currentPasswordEditChar = 'a'; 
                        } else { 
                             strncpy(current_password, passwordInputBuffer, sizeof(current_password) -1);
                             current_password[sizeof(current_password)-1] = '\0';
                             Serial.printf("[MENU_LCD] Password Entered (length %d)\n", strlen(current_password));
                             saveWiFiConfig(); 
                             currentMenuScreen = WIFI_SETTINGS; selectedMenuItem = 4; 
                        }
                    } else if (currentMenuScreen == CONFIRM_REBOOT) {
                        if(selectedMenuItem == 0) { 
                            Serial.println("[SYSTEM_LCD] Rebooting now...");
                            lcd.clear(); lcd.print("Rebooting..."); 
                            delay(1000); ESP.restart();
                        } else { 
                            rebootNeeded = false; currentMenuScreen = MAIN_MENU; selectedMenuItem = 0;
                        }
                    }
                    break;
                case 4: // BACK
                     if (currentMenuScreen == WIFI_PASSWORD_ENTRY) { 
                        if (passwordCharIndex > 0) {
                            passwordCharIndex--;
                            passwordInputBuffer[passwordCharIndex] = '\0';
                            currentPasswordEditChar = 'a'; 
                        } else { 
                            currentMenuScreen = WIFI_SETTINGS; selectedMenuItem = 3;
                        }
                    } else if (currentMenuScreen == WIFI_SETTINGS || currentMenuScreen == WIFI_STATUS) {
                        currentMenuScreen = MAIN_MENU; selectedMenuItem = 0;
                    } else if (currentMenuScreen == WIFI_SCAN) {
                        currentMenuScreen = WIFI_SETTINGS; selectedMenuItem = 1;
                    } else if (currentMenuScreen == CONFIRM_REBOOT) {
                        rebootNeeded = false; currentMenuScreen = MAIN_MENU; selectedMenuItem = 0;
                    } else if (currentMenuScreen == MAIN_MENU) {
                        isInMenuMode = false;
                         if (rebootNeeded) { currentMenuScreen = CONFIRM_REBOOT; isInMenuMode = true; selectedMenuItem = 0;}
                    } else {
                        currentMenuScreen = MAIN_MENU; selectedMenuItem = 0;
                    }
                    break;
            }
            
            if(isInMenuMode) {
                displayMenu(); 
            } else {
                updateLCD_NormalMode(); 
            }
        }
        if (!button_states[i] && buttonPressedState[i]) {
            buttonPressedState[i] = false; 
        }
    }
}

// --- Serial Command Handling ---
void handleSerialCommands() {
    if (!serialDebugEnabled) return; 

    if (Serial.available() > 0) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        Serial.print("[SERIAL_CMD_RCVD] << "); Serial.println(command);

        if (command.equalsIgnoreCase("help")) {
            Serial.println("--- Available Serial Commands ---");
            Serial.println("status                     : View current status");
            Serial.println("set_mode auto              : Set Auto fan mode");
            Serial.println("set_mode manual <0-100>    : Set Manual fan mode and speed %");
            Serial.println("wifi_enable                : Enable WiFi (reboot needed)");
            Serial.println("wifi_disable               : Disable WiFi (reboot needed)");
            Serial.println("set_ssid <your_ssid>       : Set WiFi SSID");
            Serial.println("set_pass <your_password>   : Set WiFi Password");
            Serial.println("connect_wifi               : Attempt WiFi connection (prompts reboot on success)");
            Serial.println("disconnect_wifi            : Disconnect from current WiFi");
            Serial.println("scan_wifi                  : Scan for WiFi networks");
            Serial.println("view_curve                 : View current fan curve");
            Serial.println("clear_staging_curve        : Clear temporary fan curve for editing");
            Serial.println("stage_curve_point <t> <p>  : Add point (temp pwm%) to staging curve");
            Serial.println("apply_staged_curve         : Apply and save staged fan curve");
            Serial.println("load_default_curve         : Load default fan curve");
            Serial.println("reboot                     : Reboot the ESP32");
            Serial.println("-------------------------------");
        } else if (command.equalsIgnoreCase("status")) {
            Serial.println("--- Current Status ---");
            Serial.printf("Mode: %s\n", isAutoMode ? "AUTO" : "MANUAL");
            Serial.printf("Fan Speed: %d%%\n", fanSpeedPercentage);
            Serial.printf("Temperature: %.1f C %s\n", tempSensorFound ? currentTemperature : -999.0, tempSensorFound ? "" : "(N/A)");
            Serial.printf("Fan RPM: %d\n", fanRpm);
            Serial.printf("WiFi Enabled: %s\n", isWiFiEnabled ? "Yes" : "No");
            if (isWiFiEnabled) {
                Serial.printf("WiFi Status: %s\n", WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected/Connecting");
                if (WiFi.status() == WL_CONNECTED) {
                    Serial.print("IP Address: "); Serial.println(WiFi.localIP());
                }
                Serial.print("Configured SSID: "); Serial.println(current_ssid);
            }
            Serial.println("----------------------");
        } else if (command.equalsIgnoreCase("set_mode auto")) {
            isAutoMode = true;
            needsImmediateBroadcast = true; 
            Serial.println("[SERIAL_CMD] Mode set to AUTO.");
        } else if (command.startsWith("set_mode manual ")) {
            int val = command.substring(16).toInt();
            if (val >= 0 && val <= 100) {
                isAutoMode = false;
                manualFanSpeedPercentage = val;
                needsImmediateBroadcast = true;
                Serial.printf("[SERIAL_CMD] Mode set to MANUAL, speed %d%%.\n", val);
            } else {
                Serial.println("[SERIAL_CMD_ERR] Invalid percentage for manual mode (0-100).");
            }
        } else if (command.equalsIgnoreCase("wifi_enable")) {
            if (!isWiFiEnabled) {
                isWiFiEnabled = true;
                saveWiFiConfig();
                rebootNeeded = true;
                Serial.println("[SERIAL_CMD] WiFi ENABLED. Reboot required to start network services. Type 'reboot'.");
            } else {
                Serial.println("[SERIAL_CMD] WiFi is already enabled.");
            }
        } else if (command.equalsIgnoreCase("wifi_disable")) {
            if (isWiFiEnabled) {
                isWiFiEnabled = false;
                saveWiFiConfig();
                rebootNeeded = true;
                Serial.println("[SERIAL_CMD] WiFi DISABLED. Reboot required to stop network services. Type 'reboot'.");
            } else {
                Serial.println("[SERIAL_CMD] WiFi is already disabled.");
            }
        } else if (command.startsWith("set_ssid ")) {
            String newSsid = command.substring(9);
            newSsid.trim();
            if (newSsid.length() > 0 && newSsid.length() < sizeof(current_ssid)) {
                strcpy(current_ssid, newSsid.c_str());
                saveWiFiConfig();
                Serial.printf("[SERIAL_CMD] SSID set to: '%s'.\n", current_ssid);
            } else {
                Serial.println("[SERIAL_CMD_ERR] Invalid SSID length.");
            }
        } else if (command.startsWith("set_pass ")) {
            String newPass = command.substring(9);
             if (newPass.length() < sizeof(current_password)) { 
                strcpy(current_password, newPass.c_str());
                saveWiFiConfig();
                Serial.println("[SERIAL_CMD] Password set.");
            } else {
                 Serial.println("[SERIAL_CMD_ERR] Password too long.");
            }
        } else if (command.equalsIgnoreCase("connect_wifi")) {
            if (!isWiFiEnabled) {
                Serial.println("[SERIAL_CMD] Cannot connect, WiFi is currently disabled by setting. Use 'wifi_enable' then 'reboot'.");
            } else if (strlen(current_ssid) == 0 || strcmp(current_ssid, "YOUR_WIFI_SSID") == 0) {
                Serial.println("[SERIAL_CMD] Cannot connect, SSID not configured. Use 'set_ssid'.");
            } else {
                Serial.println("[SERIAL_CMD] Attempting WiFi connection (via Serial command)... this will set rebootNeeded if successful.");
                attemptWiFiConnection(); 
                if (WiFi.status() == WL_CONNECTED && rebootNeeded) {
                    Serial.println("[SERIAL_CMD] Connection successful. Reboot is recommended to ensure network services run correctly. Type 'reboot'.");
                } else if (WiFi.status() != WL_CONNECTED) {
                    Serial.println("[SERIAL_CMD] Connection attempt finished. Check status. If settings changed, reboot might be needed.");
                }
            }
        } else if (command.equalsIgnoreCase("disconnect_wifi")) {
            Serial.println("[SERIAL_CMD] Disconnecting WiFi...");
            WiFi.disconnect(true);
            Serial.println("[SERIAL_CMD] WiFi disconnected (if it was connected). Note: This doesn't disable WiFi. Use 'wifi_disable' for that.");
        } else if (command.equalsIgnoreCase("scan_wifi")) {
            Serial.println("[SERIAL_CMD] Starting WiFi Scan...");
            WiFi.disconnect(); 
            delay(100);
            int n = WiFi.scanNetworks();
            Serial.printf("[WiFi_SCAN_SERIAL] Scan found %d networks:\n", n);
            if (n == 0) {
                Serial.println("  No networks found.");
            } else {
                for (int i = 0; i < min(n, 15); ++i) { 
                    Serial.printf("  %d: %s (%d dBm) %s\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? " " : "*");
                }
            }
        } else if (command.equalsIgnoreCase("view_curve")) {
            Serial.println("--- Current Fan Curve ---");
            if (numCurvePoints == 0) {
                Serial.println("  No curve points defined.");
            }
            for (int i = 0; i < numCurvePoints; i++) {
                Serial.printf("  Point %d: Temp = %d C, PWM = %d%%\n", i, tempPoints[i], pwmPercentagePoints[i]);
            }
            Serial.println("-------------------------");
        } else if (command.equalsIgnoreCase("clear_staging_curve")) {
            stagingNumCurvePoints = 0;
            Serial.println("[SERIAL_CMD] Staging fan curve cleared.");
        } else if (command.startsWith("stage_curve_point ")) {
            if (stagingNumCurvePoints >= MAX_CURVE_POINTS) {
                Serial.println("[SERIAL_CMD_ERR] Max staging curve points reached. Apply or clear first.");
            } else {
                int temp, pwm;
                if (sscanf(command.c_str(), "stage_curve_point %d %d", &temp, &pwm) == 2) {
                    if (temp >= 0 && temp <= 120 && pwm >= 0 && pwm <= 100) {
                        if (stagingNumCurvePoints > 0 && temp <= stagingTempPoints[stagingNumCurvePoints -1]){
                             Serial.println("[SERIAL_CMD_ERR] Temperature must be greater than previous point.");
                        } else {
                            stagingTempPoints[stagingNumCurvePoints] = temp;
                            stagingPwmPercentagePoints[stagingNumCurvePoints] = pwm;
                            stagingNumCurvePoints++;
                            Serial.printf("[SERIAL_CMD] Staged point %d: Temp=%d, PWM=%d. Total staged: %d\n", stagingNumCurvePoints -1, temp, pwm, stagingNumCurvePoints);
                        }
                    } else {
                        Serial.println("[SERIAL_CMD_ERR] Invalid temp (0-120) or PWM (0-100) value.");
                    }
                } else {
                    Serial.println("[SERIAL_CMD_ERR] Invalid format. Use: stage_curve_point <temp> <pwm_percent>");
                }
            }
        } else if (command.equalsIgnoreCase("apply_staged_curve")) {
            if (stagingNumCurvePoints < 2) {
                Serial.println("[SERIAL_CMD_ERR] Need at least 2 points to apply a curve.");
            } else {
                numCurvePoints = stagingNumCurvePoints;
                for (int i = 0; i < numCurvePoints; i++) {
                    tempPoints[i] = stagingTempPoints[i];
                    pwmPercentagePoints[i] = stagingPwmPercentagePoints[i];
                }
                saveFanCurveToNVS();
                stagingNumCurvePoints = 0; 
                needsImmediateBroadcast = true; 
                Serial.println("[SERIAL_CMD] Staged fan curve applied and saved.");
            }
        } else if (command.equalsIgnoreCase("load_default_curve")) {
            setDefaultFanCurve();
            saveFanCurveToNVS(); 
            needsImmediateBroadcast = true;
            Serial.println("[SERIAL_CMD] Default fan curve loaded and saved.");
        }
         else if (command.equalsIgnoreCase("reboot")) {
            Serial.println("[SERIAL_CMD] Rebooting device now...");
            delay(100);
            ESP.restart();
        } else {
            Serial.println("[SERIAL_CMD_ERR] Unknown command. Type 'help' for a list of commands.");
        }
    }
}


// --- Network Task (Core 0) ---
void networkTask(void *pvParameters) {
    Serial.println("[TASK] Network Task started on Core 0.");
    unsigned long lastPeriodicBroadcastTime = 0;

    if (!isWiFiEnabled) { 
        Serial.println("[NETWORK_TASK] WiFi is disabled by NVS config. Network task ending.");
        vTaskDelete(NULL); 
        return;
    }

    Serial.print("[WiFi] NetworkTask: Attempting connection to SSID: '"); Serial.print(current_ssid); Serial.println("'");
    
    if (strlen(current_ssid) > 0 && strcmp(current_ssid, "YOUR_WIFI_SSID") != 0 && strcmp(current_ssid, "") != 0 ) {
        WiFi.mode(WIFI_STA); 
        WiFi.begin(current_ssid, current_password);
        int wifiTimeout = 0;
        while (WiFi.status() != WL_CONNECTED && wifiTimeout < 30) { 
            delay(500); Serial.print("."); 
            wifiTimeout++;
        }
    } else {
        Serial.println("[WiFi] NetworkTask: SSID not configured or is default. Skipping connection attempt.");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] NetworkTask: Connected successfully!");
        Serial.print("[WiFi] NetworkTask: IP Address: "); Serial.println(WiFi.localIP());
    } else { 
        Serial.println("\n[WiFi] NetworkTask: Connection Failed or not attempted. Web services may not be available.");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
            String html_page = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32 Fan Controller V1.6 (Serial CMD)</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; margin: 0; padding: 15px; background-color: #eef2f5; color: #333; display: flex; flex-direction: column; align-items: center; }
    .container { background-color: #fff; padding: 20px; border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); width: 90%; max-width: 600px; margin-bottom: 20px; }
    h1 { color: #2c3e50; text-align: center; margin-bottom: 20px; }
    p { margin: 8px 0; line-height: 1.6; }
    .data-grid { display: grid; grid-template-columns: auto 1fr; gap: 5px 10px; align-items: center; margin-bottom:15px;}
    .data-label { font-weight: 600; color: #555; }
    .data-value { font-weight: 500; color: #2980b9; }
    button, input[type="submit"] { background-color: #3498db; color: white; padding: 10px 18px; border: none; border-radius: 6px; cursor: pointer; font-size: 1em; margin: 5px; transition: background-color 0.2s ease; }
    button:hover, input[type="submit"]:hover { background-color: #2980b9; }
    button.secondary { background-color: #7f8c8d; }
    button.secondary:hover { background-color: #606f70; }
    button.danger { background-color: #e74c3c;}
    button.danger:hover { background-color: #c0392b;}
    .slider-container { margin: 15px 0; padding:10px; background-color: #f8f9fa; border-radius: 6px;}
    input[type="range"] { width: calc(100% - 60px); margin-right: 10px; vertical-align: middle; }
    #manualSpeedValue, #temp, #fanSpeed, #mode, #rpm { font-weight: bold; }
    .curve-editor { margin-top: 20px; padding: 15px; border: 1px solid #ddd; border-radius: 8px; background-color: #fdfdfd;}
    .curve-point { display: flex; align-items: center; margin-bottom: 8px; padding: 5px; border-radius:4px; background-color:#f9f9f9; }
    .curve-point label { margin-right: 5px; min-width:50px; }
    .curve-point input[type="number"] { width: 70px; padding: 6px; border: 1px solid #ccc; border-radius: 4px; margin-right: 10px; }
    #fanCurvePointsContainer .placeholder { color: #888; font-style: italic; }
    .hidden { display: none !important; }
  </style>
</head>
<body>
  <div class="container">
    <h1>PC Fan Controller (ESP32 - Serial CMD)</h1>
    <div class="data-grid">
      <span class="data-label">Temperature:</span> <span id="temp" class="data-value">--</span> &deg;C
      <span class="data-label">Fan Speed:</span> <span id="fanSpeed" class="data-value">--</span> %
      <span class="data-label">Fan RPM:</span> <span id="rpm" class="data-value">--</span> RPM
      <span class="data-label">Mode:</span> <span id="mode" class="data-value">--</span> <span id="autoModeNotice" class="data-value" style="font-size:0.8em; color:#e67e22;"></span>
    </div>

    <button id="autoModeButton" onclick="sendCommand({action: 'setModeAuto'})">Auto Mode</button>
    <button onclick="sendCommand({action: 'setModeManual'})">Manual Mode</button>

    <div class="slider-container" id="manualControl" style="display:none;">
      <p>Manual Fan Speed: <span id="manualSpeedValue">50</span>%</p>
      <input type="range" min="0" max="100" value="50" id="speedSlider" oninput="updateSliderValueDisplay(this.value)" onchange="setManualSpeed(this.value)">
    </div>
  </div>

  <div class="container curve-editor" id="curveEditorContainer">
    <h2>Fan Curve Editor (Auto Mode)</h2>
    <div id="fanCurvePointsContainer">
      <p class="placeholder">Curve points will load here...</p>
    </div>
    <button onclick="addCurvePointUI()">+ Add Point</button>
    <button onclick="saveFanCurve()" class="secondary">Save Curve to Device</button>
    <p style="font-size:0.9em; color:#555;">Note: Curve is stored on device. Min 2 points, max 8. Temps must be increasing.</p>
  </div>

  <script>
    var gateway = `ws://${window.location.hostname}:81/`;
    var websocket;
    const MAX_CURVE_POINTS_UI = 8;

    window.addEventListener('load', onLoad);

    function onLoad(event) { initWebSocket(); }

    function initWebSocket() {
      console.log('Trying to open a WebSocket connection...');
      websocket = new WebSocket(gateway);
      websocket.onopen    = onOpen;
      websocket.onclose   = onClose;
      websocket.onmessage = onMessage;
    }

    function onOpen(event) { console.log('Connection opened'); }
    function onClose(event) { 
        console.log('Connection closed'); 
        setTimeout(initWebSocket, 5000); 
    }

    function onMessage(event) {
      let data;
      try {
        data = JSON.parse(event.data);
      } catch (e) { console.error("Error parsing JSON:", e, event.data); return; }

      if (data.isWiFiEnabled === false) {
          console.log("ESP32 reports WiFi is disabled. Web UI may not function fully.");
      }

      if (data.tempSensorFound !== undefined) {
        const curveEditor = document.getElementById('curveEditorContainer');
        const autoModeNotice = document.getElementById('autoModeNotice');
        const tempDisplay = document.getElementById('temp');

        if (data.tempSensorFound) {
          tempDisplay.innerText = data.temperature !== undefined && data.temperature > -990 ? data.temperature.toFixed(1) : 'N/A';
          curveEditor.classList.remove('hidden');
          autoModeNotice.innerText = '';
        } else {
          tempDisplay.innerText = 'N/A';
          curveEditor.classList.add('hidden');
           if (data.isAutoMode) { 
            autoModeNotice.innerText = '(Sensor N/A - Fixed Speed)';
          } else {
            autoModeNotice.innerText = '';
          }
        }
      } else if (data.temperature !== undefined) { 
         document.getElementById('temp').innerText = data.temperature > -990 ? data.temperature.toFixed(1) : 'N/A';
      }


      if(data.fanSpeed !== undefined) document.getElementById('fanSpeed').innerText = data.fanSpeed;
      if(data.fanRpm !== undefined) document.getElementById('rpm').innerText = data.fanRpm;
      
      if(data.isAutoMode !== undefined) {
        const modeStr = data.isAutoMode ? "AUTO" : "MANUAL";
        document.getElementById('mode').innerText = modeStr;
        document.getElementById('manualControl').style.display = data.isAutoMode ? 'none' : 'block';
        const autoModeNotice = document.getElementById('autoModeNotice');
        if (data.isAutoMode && data.tempSensorFound === false) {
            autoModeNotice.innerText = '(Sensor N/A - Fixed Speed)';
        } else if (data.isAutoMode && data.tempSensorFound === true) {
             autoModeNotice.innerText = ''; 
        }
      }
      
      if(data.manualFanSpeed !== undefined && !data.isAutoMode) {
        document.getElementById('speedSlider').value = data.manualFanSpeed;
        document.getElementById('manualSpeedValue').innerText = data.manualFanSpeed;
      }

      if(data.fanCurve && Array.isArray(data.fanCurve) && data.tempSensorFound) { 
        displayFanCurve(data.fanCurve); 
      } else if (!data.tempSensorFound) {
        document.getElementById('fanCurvePointsContainer').innerHTML = '<p class="placeholder">Fan curve editor disabled: Temperature sensor not detected.</p>';
      }
    }

    function sendCommand(commandPayload) {
      if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send(JSON.stringify(commandPayload));
      } else { console.log("WebSocket not open or WiFi disabled on ESP32."); }
    }
    function updateSliderValueDisplay(value) { document.getElementById('manualSpeedValue').innerText = value; }
    function setManualSpeed(value) {
      updateSliderValueDisplay(value); 
      sendCommand({ action: 'setManualSpeed', value: parseInt(value) });
    }
    function displayFanCurve(curvePoints) {
      const container = document.getElementById('fanCurvePointsContainer');
      container.innerHTML = ''; 
      if (!curvePoints || curvePoints.length === 0) {
        container.innerHTML = '<p class="placeholder">No curve points. Add points.</p>'; return;
      }
      curvePoints.forEach((point, index) => {
        const div = document.createElement('div');
        div.classList.add('curve-point');
        div.innerHTML = `
          <label>Temp (&deg;C):</label> <input type="number" value="${point.temp}" min="0" max="120" id="temp_p_${index}">
          <label>Fan (%):</label> <input type="number" value="${point.pwmPercent}" min="0" max="100" id="pwm_p_${index}">
          <button onclick="removeCurvePointUI(${index})" class="danger" style="padding:5px 10px; font-size:0.8em;">X</button>`;
        container.appendChild(div);
      });
    }
    function addCurvePointUI() {
      const container = document.getElementById('fanCurvePointsContainer');
      const placeholder = container.querySelector('.placeholder');
      if (placeholder) placeholder.remove();
      const currentPoints = container.querySelectorAll('.curve-point').length;
      if (currentPoints >= MAX_CURVE_POINTS_UI) {
        alert(`Max ${MAX_CURVE_POINTS_UI} points.`); return;
      }
      const index = currentPoints; 
      const div = document.createElement('div');
      div.classList.add('curve-point');
      let lastTemp = 0, lastPwm = 0;
      if (index > 0) {
          try { 
            lastTemp = parseInt(document.getElementById(`temp_p_${index-1}`).value) || 0;
            lastPwm = parseInt(document.getElementById(`pwm_p_${index-1}`).value) || 0;
          } catch (e) { /* ignore */ }
      }
      const defaultNewTemp = Math.min(120, lastTemp + 10); 
      const defaultNewPwm = Math.min(100, lastPwm + 10);  
      div.innerHTML = `
        <label>Temp (&deg;C):</label> <input type="number" value="${defaultNewTemp}" min="0" max="120" id="temp_p_${index}">
        <label>Fan (%):</label> <input type="number" value="${defaultNewPwm}" min="0" max="100" id="pwm_p_${index}">
        <button onclick="removeCurvePointUI(${index})" class="danger" style="padding:5px 10px; font-size:0.8em;">X</button>`;
      container.appendChild(div);
    }
    function removeCurvePointUI(idx) {
        const points = document.querySelectorAll('#fanCurvePointsContainer .curve-point');
        if (points[idx]) points[idx].remove();
    }
    function saveFanCurve() {
      const points = [];
      const pElements = document.querySelectorAll('#fanCurvePointsContainer .curve-point');
      let isValid = true, lastTemp = -100; 
      pElements.forEach((el, index) => {
        const tempIn = el.querySelector('input[type="number"][id^="temp_p_"]');
        const pwmIn = el.querySelector('input[type="number"][id^="pwm_p_"]');
        if (!tempIn || !pwmIn) { isValid = false; return; }
        const temp = parseInt(tempIn.value), pwmPercent = parseInt(pwmIn.value);
        if (isNaN(temp) || isNaN(pwmPercent) || temp < 0 || temp > 120 || pwmPercent < 0 || pwmPercent > 100 || (index > 0 && temp <= lastTemp)) {
          alert(`Invalid data at point ${index + 1}. Temps must increase.`); isValid = false; return;
        }
        lastTemp = temp; points.push({ temp: temp, pwmPercent: pwmPercent });
      });
      if (!isValid) return;
      if (points.length < 2) { alert("Min 2 points."); return; }
      if (points.length > MAX_CURVE_POINTS_UI) { alert(`Max ${MAX_CURVE_POINTS_UI} points.`); return; }
      sendCommand({ action: 'setCurve', curve: points });
      alert("Fan curve sent and saved.");
    }
  </script>
</body>
</html>
        )rawliteral";
        request->send(200, "text/html", html_page);
    });
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    server.begin();
    Serial.println("[SYSTEM] HTTP server started on Core 0.");
    } // End of if (WiFi.status() == WL_CONNECTED) for web server setup

    for(;;) {
        if (isWiFiEnabled && WiFi.status() == WL_CONNECTED) { 
            webSocket.loop();
            unsigned long currentTime = millis();
            if (needsImmediateBroadcast || (currentTime - lastPeriodicBroadcastTime > 5000)) { 
                broadcastWebSocketData();
                needsImmediateBroadcast = false; 
                lastPeriodicBroadcastTime = currentTime;
            }
        } else if (isWiFiEnabled && WiFi.status() != WL_CONNECTED) {
            // Optional: More robust retry or status update
            // Serial.println("[NETWORK_TASK] WiFi enabled but not connected.");
        }
        vTaskDelay(pdMS_TO_TICKS(50)); 
    }
}

// --- Main Application Task (Core 1) ---
void mainAppTask(void *pvParameters) {
    Serial.println("[TASK] Main Application Task started on Core 1.");
    unsigned long lastTempReadTime = 0;
    unsigned long lastRpmCalculationTime = 0;
    unsigned long lastLcdUpdateTime = 0;
    lastRpmReadTime_Task = millis();

    if (isInMenuMode) displayMenu(); else updateLCD_NormalMode();

    for(;;) {
        unsigned long currentTime = millis();

        if(serialDebugEnabled) { // Check the flag before handling serial commands
            handleSerialCommands(); 
        }
        handleMenuInput();      

        if (!isInMenuMode) {
            if (tempSensorFound) {
                if (currentTime - lastTempReadTime > 2000) { 
                    lastTempReadTime = currentTime;
                    float newTemp = bmp.readTemperature();
                    if (!isnan(newTemp)) { 
                        currentTemperature = newTemp;
                    } else {
                        Serial.println("[SENSOR] Failed to read from BMP280 sensor!");
                        currentTemperature = -999.0; 
                    }
                }
            } else {
                currentTemperature = -999.0; 
            }

            if (currentTime - lastRpmCalculationTime > 1000) { 
                lastRpmCalculationTime = currentTime;
                noInterrupts(); 
                unsigned long currentPulses = pulseCount;
                pulseCount = 0; 
                interrupts(); 
                
                unsigned long elapsedMillis = currentTime - lastRpmReadTime_Task;
                lastRpmReadTime_Task = currentTime; 

                if (elapsedMillis > 0 && PULSES_PER_REVOLUTION > 0) {
                    fanRpm = (currentPulses / (float)PULSES_PER_REVOLUTION) * (60000.0f / elapsedMillis);
                } else {
                    fanRpm = 0;
                }
            }

            int oldFanSpeedPercentage = fanSpeedPercentage; 

            if (isAutoMode) {
                if (tempSensorFound) {
                    int autoPwmPerc = calculateAutoFanPWMPercentage(currentTemperature);
                    if (autoPwmPerc != fanSpeedPercentage) {
                        setFanSpeed(autoPwmPerc);
                    }
                } else { 
                    if (AUTO_MODE_NO_SENSOR_FAN_PERCENTAGE != fanSpeedPercentage) {
                        setFanSpeed(AUTO_MODE_NO_SENSOR_FAN_PERCENTAGE);
                    }
                }
            } else { 
                if (manualFanSpeedPercentage != fanSpeedPercentage) {
                    setFanSpeed(manualFanSpeedPercentage);
                }
            }
            
            if (currentTime - lastLcdUpdateTime > 1000 || oldFanSpeedPercentage != fanSpeedPercentage || needsImmediateBroadcast) { 
                updateLCD_NormalMode();
                lastLcdUpdateTime = currentTime;
            }
        } 
        
        vTaskDelay(pdMS_TO_TICKS(50)); 
    }
}

// --- Arduino Setup Function (runs on Core 1) ---
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\nESP32 Fan Controller Initializing (Serial & LCD Menu, Optional WiFi)...");
    
    pinMode(DEBUG_ENABLE_PIN, INPUT_PULLDOWN); // Initialize DEBUG_ENABLE_PIN
    pinMode(LED_DEBUG_PIN, OUTPUT);            // Initialize Debug LED Pin
    serialDebugEnabled = digitalRead(DEBUG_ENABLE_PIN) == HIGH;
    digitalWrite(LED_DEBUG_PIN, serialDebugEnabled ? HIGH : LOW); // Set LED based on debug state


    if (serialDebugEnabled) {
        Serial.println("[SYSTEM] Serial Debug Command Interface IS ENABLED (DEBUG_ENABLE_PIN is HIGH). Debug LED ON.");
        Serial.println("Type 'help' for a list of serial commands.");
    } else {
        Serial.println("[SYSTEM] Serial Debug Command Interface IS DISABLED (DEBUG_ENABLE_PIN is LOW). Debug LED OFF.");
    }


    loadWiFiConfig(); 

    Wire.begin(); 

    Serial.println("[INIT] Initializing BMP280 sensor...");
    if (!bmp.begin(0x76)) { 
        Serial.println(F("[INIT] BMP280 at 0x76 fail, try 0x77"));
        if (!bmp.begin(0x77)) { 
            Serial.println(F("[INIT] BMP280 not found. Temperature sensor will be optional."));
            tempSensorFound = false;
            currentTemperature = -999.0; 
        } else {
            Serial.println(F("[INIT] BMP280 found at 0x77."));
            tempSensorFound = true;
        }
    } else {
        Serial.println(F("[INIT] BMP280 found at 0x76."));
        tempSensorFound = true;
    }

    if (tempSensorFound) {
        bmp.setSampling(Adafruit_BMP280::MODE_NORMAL, Adafruit_BMP280::SAMPLING_X2,
                        Adafruit_BMP280::SAMPLING_NONE, Adafruit_BMP280::FILTER_X16,
                        Adafruit_BMP280::STANDBY_MS_500);
        Serial.println("[INIT] BMP280 Initialized and configured.");
    }

    Serial.println("[INIT] Initializing LCD...");
    lcd.init();
    lcd.backlight();
    lcd.print("Fan Controller");
    lcd.setCursor(0,1);
    lcd.print("Booting..."); 
    delay(1000);
    Serial.println("[INIT] LCD Initialized.");

    Serial.println("[INIT] Setting up LEDC PWM for fan...");
    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION_BITS);
    ledcAttachPin(FAN_PWM_PIN, PWM_CHANNEL);             
    Serial.println("[INIT] LEDC PWM Setup Complete.");
    
    Serial.println("[INIT] Setting up Fan Tachometer Interrupt...");
    pinMode(FAN_TACH_PIN_ACTUAL, INPUT_PULLUP); // Use FAN_TACH_PIN_ACTUAL
    attachInterrupt(digitalPinToInterrupt(FAN_TACH_PIN_ACTUAL), countPulse, FALLING); // Use FAN_TACH_PIN_ACTUAL
    Serial.println("[INIT] Fan Tachometer Interrupt Setup Complete.");

    setDefaultFanCurve(); 
    loadFanCurveFromNVS(); 
    
    ledcWrite(PWM_CHANNEL, 0); 
    fanSpeedPercentage = 0;
    fanSpeedPWM_Raw = 0;
    Serial.println("[INIT] Fan set to 0% initially.");

    Serial.println("[INIT] Setting up Buttons...");
    pinMode(BTN_MENU_PIN, INPUT_PULLUP);
    pinMode(BTN_UP_PIN, INPUT_PULLUP);
    pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
    pinMode(BTN_SELECT_PIN, INPUT_PULLUP);
    pinMode(BTN_BACK_PIN, INPUT_PULLUP);
    Serial.println("[INIT] Buttons Setup Complete.");

    Serial.println("[INIT] Creating FreeRTOS Tasks...");
    if (isWiFiEnabled) { 
        Serial.println("[INIT] WiFi is enabled by config, starting NetworkTask.");
        xTaskCreatePinnedToCore(networkTask, "NetworkTask", 10000, NULL, 1, &networkTaskHandle, 0);
    } else {
        Serial.println("[INIT] WiFi is disabled by config. NetworkTask will not be started.");
    }
    xTaskCreatePinnedToCore(mainAppTask, "MainAppTask", 10000, NULL, 2, &mainAppTaskHandle, 1); 

    Serial.println("[INIT] Setup complete. Tasks launched (if applicable).");
}

void loop() {
  // Check DEBUG_ENABLE_PIN periodically in loop if it can change dynamically
  // For now, it's only checked in setup.
  // bool currentDebugState = digitalRead(DEBUG_ENABLE_PIN) == HIGH;
  // if (currentDebugState != serialDebugEnabled) {
  //    serialDebugEnabled = currentDebugState;
  //    digitalWrite(LED_DEBUG_PIN, serialDebugEnabled ? HIGH : LOW);
  //    Serial.printf("[SYSTEM] Serial Debug mode %s.\n", serialDebugEnabled ? "ENABLED" : "DISABLED");
  // }
  vTaskDelay(pdMS_TO_TICKS(1000));
}
