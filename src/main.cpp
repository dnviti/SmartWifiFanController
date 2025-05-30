#include "config.h"         // Must be first for global externs
#include <SPIFFS.h>         
#include "nvs_handler.h"
#include "fan_control.h"
#include "display_handler.h"
#include "input_handler.h"
#include "network_handler.h" 
#include "tasks.h"
#include "mqtt_handler.h"   
#include "ota_updater.h"    

// --- Global Variable Definitions (these are declared extern in config.h) ---
// Pin Definitions
const int FAN_PWM_PIN = 18;
const int BTN_MENU_PIN = 19;    
const int BTN_UP_PIN = 23;      
const int BTN_DOWN_PIN = 22;    
const int BTN_SELECT_PIN = 21;  
const int BTN_BACK_PIN = 5;     
const int DEBUG_ENABLE_PIN = 4; 
const int LED_DEBUG_PIN = 2;    
const int FAN_TACH_PIN_ACTUAL = 15; 

// Fan Control Constants
const int PWM_CHANNEL = 0;
const int PWM_FREQ = 25000;
const int PWM_RESOLUTION_BITS = 8;
const int AUTO_MODE_NO_SENSOR_FAN_PERCENTAGE = 60;
const int PULSES_PER_REVOLUTION = 2;

// Modes & States
volatile bool isAutoMode = true;
volatile int manualFanSpeedPercentage = 50;
volatile bool isInMenuMode = false;
volatile bool isWiFiEnabled = false; 
volatile bool serialDebugEnabled = false; 
volatile float currentTemperature = -999.0; 
volatile bool tempSensorFound = false;      
volatile int fanRpm = 0;
volatile int fanSpeedPercentage = 0;
volatile int fanSpeedPWM_Raw = 0;
volatile unsigned long pulseCount = 0;
volatile unsigned long lastRpmReadTime_Task = 0; // Marked volatile as it's shared across contexts (tasks/timing)

// Menu System Variables
volatile MenuScreen currentMenuScreen = MAIN_MENU;
volatile int selectedMenuItem = 0;
volatile int scanResultCount = 0;
String scannedSSIDs[10]; 
char passwordInputBuffer[64] = ""; 
char generalInputBuffer[128] = ""; 
volatile int generalInputCharIndex = 0;
volatile char currentGeneralEditChar = 'a';

volatile int passwordCharIndex = 0; 
volatile char currentPasswordEditChar = 'a'; 

// Fan Curve
int tempPoints[MAX_CURVE_POINTS];
int pwmPercentagePoints[MAX_CURVE_POINTS];
int numCurvePoints = 0;
volatile bool fanCurveChanged = false; 

// Staging Fan Curve for Serial Commands
int stagingTempPoints[MAX_CURVE_POINTS];
int stagingPwmPercentagePoints[MAX_CURVE_POINTS];
int stagingNumCurvePoints = 0;

// Task Communication
volatile bool needsImmediateBroadcast = false;
volatile bool rebootNeeded = false; 

// MQTT Configuration
volatile bool isMqttEnabled = false; 
char mqttServer[64] = "your_mqtt_broker_ip"; 
int mqttPort = 1883;                         
char mqttUser[64] = "";                      
char mqttPassword[64] = "";                  
char mqttBaseTopic[64] = "fancontroller";    

// MQTT Discovery Configuration
volatile bool isMqttDiscoveryEnabled = true; 
char mqttDiscoveryPrefix[32] = "homeassistant"; 
char mqttDeviceId[64] = "esp32fanctrl";   
char mqttDeviceName[64] = "ESP32 Fan Controller"; 


// OTA Update Status
volatile bool ota_in_progress = false;
String ota_status_message = "OTA Idle"; 
String GITHUB_API_ROOT_CA_STRING = ""; // <<< Actual definition of the global variable


// Global Objects
Preferences preferences;
Adafruit_BMP280 bmp;
LiquidCrystal_I2C lcd(0x27, 16, 2);
AsyncWebServer server(80);
WebSocketsServer webSocket(81);
WiFiClient espClient; 
PubSubClient mqttClient(espClient); 


// Button Debouncing
unsigned long buttonPressTime[5]; 
bool buttonPressedState[5];
const long debounceDelay = 50; 
const long longPressDelay = 1000; 

// SSID and Password
char current_ssid[64] = "YOUR_WIFI_SSID"; 
char current_password[64] = "YOUR_WIFI_PASSWORD"; 

TaskHandle_t networkTaskHandle = NULL; 
TaskHandle_t mainAppTaskHandle = NULL;


// Function to load Root CA from SPIFFS
void loadRootCA() {
    if (SPIFFS.exists(GITHUB_ROOT_CA_FILENAME)) {
        File caFile = SPIFFS.open(GITHUB_ROOT_CA_FILENAME, "r");
        if (caFile) {
            GITHUB_API_ROOT_CA_STRING = caFile.readString(); // Assign to the global String
            caFile.close();
            if (GITHUB_API_ROOT_CA_STRING.length() > 0) {
                if (serialDebugEnabled) Serial.printf("[INIT] Successfully loaded Root CA from %s (%d bytes)\n", GITHUB_ROOT_CA_FILENAME, GITHUB_API_ROOT_CA_STRING.length());
            } else {
                if (serialDebugEnabled) Serial.printf("[INIT_ERR] Root CA file %s is empty.\n", GITHUB_ROOT_CA_FILENAME);
            }
        } else {
            if (serialDebugEnabled) Serial.printf("[INIT_ERR] Failed to open Root CA file %s for reading.\n", GITHUB_ROOT_CA_FILENAME);
        }
    } else {
        if (serialDebugEnabled) Serial.printf("[INIT_WARN] Root CA file %s not found on SPIFFS. Secure OTA might fail if server cert is not otherwise trusted.\n", GITHUB_ROOT_CA_FILENAME);
    }
}


// --- Arduino Setup Function (runs on Core 1) ---
void setup() {
    pinMode(DEBUG_ENABLE_PIN, INPUT_PULLDOWN); 
    serialDebugEnabled = digitalRead(DEBUG_ENABLE_PIN) == HIGH;

    if (serialDebugEnabled) {
        Serial.begin(115200);
        while(!Serial && millis() < 1000); 
        delay(100); 
        Serial.println("\n[SYSTEM] Serial Debug ENABLED. Fan Controller Initializing...");
        Serial.printf("[SYSTEM] Firmware Version: %s\n", FIRMWARE_VERSION);
        Serial.printf("[SYSTEM] PIO Build Env: %s\n", PIO_BUILD_ENV_NAME);
        Serial.println("Type 'help' for a list of serial commands.");
    } 
    
    pinMode(LED_DEBUG_PIN, OUTPUT);            
    digitalWrite(LED_DEBUG_PIN, serialDebugEnabled ? HIGH : LOW); 
    if (serialDebugEnabled) {
        Serial.printf("[SYSTEM] Debug LED %s.\n", serialDebugEnabled ? "ON" : "OFF");
    }

    if(serialDebugEnabled) Serial.println("[INIT] Initializing SPIFFS...");
    if(!SPIFFS.begin(true)){ 
        if(serialDebugEnabled) Serial.println("[INIT_ERR] SPIFFS Mount Failed! Web server may not work.");
    } else {
        if(serialDebugEnabled) Serial.println("[INIT] SPIFFS Mounted Successfully.");
        loadRootCA(); // Load CA after SPIFFS is initialized
    }

    loadWiFiConfig(); 
    loadMqttConfig(); 
    loadMqttDiscoveryConfig(); 

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA); 
    snprintf(mqttDeviceId, sizeof(mqttDeviceId), "esp32fanctrl_%02X%02X%02X", mac[3], mac[4], mac[5]);
    if(serialDebugEnabled) Serial.printf("[INIT] MQTT Device ID set to: %s\n", mqttDeviceId);
    if(serialDebugEnabled) Serial.printf("[INIT] MQTT Device Name: %s\n", mqttDeviceName);
    if(serialDebugEnabled) Serial.printf("[INIT] MQTT Discovery Prefix (after NVS load): %s, Enabled: %s\n", mqttDiscoveryPrefix, isMqttDiscoveryEnabled ? "Yes" : "No");


    Wire.begin(); 

    if(serialDebugEnabled) Serial.println("[INIT] Initializing BMP280 sensor...");
    if (!bmp.begin(0x76)) { 
        if(serialDebugEnabled) Serial.println(F("[INIT] BMP280 at 0x76 fail, try 0x77"));
        if (!bmp.begin(0x77)) { 
            if(serialDebugEnabled) Serial.println(F("[INIT] BMP280 not found. Temperature sensor will be optional."));
            tempSensorFound = false;
            currentTemperature = -999.0; 
        } else {
            if(serialDebugEnabled) Serial.println(F("[INIT] BMP280 found at 0x77."));
            tempSensorFound = true;
        }
    } else {
        if(serialDebugEnabled) Serial.println(F("[INIT] BMP280 found at 0x76."));
        tempSensorFound = true;
    }

    if (tempSensorFound) {
        bmp.setSampling(Adafruit_BMP280::MODE_NORMAL, Adafruit_BMP280::SAMPLING_X2,
                        Adafruit_BMP280::SAMPLING_NONE, Adafruit_BMP280::FILTER_X16,
                        Adafruit_BMP280::STANDBY_MS_500);
        if(serialDebugEnabled) Serial.println("[INIT] BMP280 Initialized and configured.");
    }

    if(serialDebugEnabled) Serial.println("[INIT] Initializing LCD...");
    lcd.init();
    lcd.backlight();
    lcd.print("Fan Controller");
    lcd.setCursor(0,1);
    lcd.print(FIRMWARE_VERSION); 
    delay(1500);
    if(serialDebugEnabled) Serial.println("[INIT] LCD Initialized.");

    if(serialDebugEnabled) Serial.println("[INIT] Setting up LEDC PWM for fan...");
    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION_BITS);
    ledcAttachPin(FAN_PWM_PIN, PWM_CHANNEL);             
    if(serialDebugEnabled) Serial.println("[INIT] LEDC PWM Setup Complete.");
    
    if(serialDebugEnabled) Serial.println("[INIT] Setting up Fan Tachometer Interrupt...");
    pinMode(FAN_TACH_PIN_ACTUAL, INPUT_PULLUP); 
    attachInterrupt(digitalPinToInterrupt(FAN_TACH_PIN_ACTUAL), countPulse, FALLING); 
    if(serialDebugEnabled) Serial.println("[INIT] Fan Tachometer Interrupt Setup Complete.");

    setDefaultFanCurve(); 
    loadFanCurveFromNVS(); 
    
    ledcWrite(PWM_CHANNEL, 0); 
    fanSpeedPercentage = 0;
    fanSpeedPWM_Raw = 0;
    if(serialDebugEnabled) Serial.println("[INIT] Fan set to 0% initially.");

    if(serialDebugEnabled) Serial.println("[INIT] Setting up Buttons...");
    pinMode(BTN_MENU_PIN, INPUT_PULLUP);
    pinMode(BTN_UP_PIN, INPUT_PULLUP);
    pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
    pinMode(BTN_SELECT_PIN, INPUT_PULLUP);
    pinMode(BTN_BACK_PIN, INPUT_PULLUP);
    if(serialDebugEnabled) Serial.println("[INIT] Buttons Setup Complete.");
    
    if(serialDebugEnabled) Serial.println("[INIT] Creating FreeRTOS Tasks...");
    xTaskCreatePinnedToCore(networkTask, "NetworkTask", 12000, NULL, 1, &networkTaskHandle, 0); 
    xTaskCreatePinnedToCore(mainAppTask, "MainAppTask", 10000, NULL, 2, &mainAppTaskHandle, 1); 

    if(serialDebugEnabled) Serial.println("[INIT] Setup complete. Tasks launched.");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000)); 
}
