#ifndef CONFIG_H
#define CONFIG_H

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
#include <PubSubClient.h> // Added for MQTT
#include <HTTPClient.h>   // For OTA from URL
#include <HTTPUpdate.h>   // For OTA from URL
#include <SPIFFS.h>       // For loading CA from SPIFFS

// --- Firmware Version ---
#define FIRMWARE_VERSION "0.1.2" // Define firmware version
#define PIO_BUILD_ENV_NAME "esp32_fancontrol" // MUST MATCH 'PIO_ENV' in GitHub Actions release.yml

// --- GitHub OTA Configuration ---
#define GITHUB_REPO_OWNER "dnviti"
#define GITHUB_REPO_NAME "SmartWifiFanController"
#define GITHUB_API_LATEST_RELEASE_URL "https://api.github.com/repos/" GITHUB_REPO_OWNER "/" GITHUB_REPO_NAME "/releases/latest"
#define GITHUB_ROOT_CA_FILENAME "/github_root_ca.pem" // Filename on SPIFFS for the Root CA

// --- OTA Update Status ---
extern volatile bool ota_in_progress;
extern String ota_status_message; 
extern String GITHUB_API_ROOT_CA_STRING; // Extern declaration

// --- Pin Definitions ---
extern const int FAN_PWM_PIN;
extern const int BTN_MENU_PIN;    
extern const int BTN_UP_PIN;      
extern const int BTN_DOWN_PIN;    
extern const int BTN_SELECT_PIN;  
extern const int BTN_BACK_PIN;     
extern const int DEBUG_ENABLE_PIN; 
extern const int LED_DEBUG_PIN;    
extern const int FAN_TACH_PIN_ACTUAL; 

// --- Fan Control Constants ---
extern const int PWM_CHANNEL;
extern const int PWM_FREQ;
extern const int PWM_RESOLUTION_BITS;
extern const int AUTO_MODE_NO_SENSOR_FAN_PERCENTAGE;
extern const int PULSES_PER_REVOLUTION;

// --- Modes & States (Global Volatile Variables) ---
extern volatile bool isAutoMode;
extern volatile int manualFanSpeedPercentage;
extern volatile bool isInMenuMode;
extern volatile bool isWiFiEnabled; 
extern volatile bool serialDebugEnabled; 
extern volatile float currentTemperature; 
extern volatile bool tempSensorFound;      
extern volatile int fanRpm;
extern volatile int fanSpeedPercentage;
extern volatile int fanSpeedPWM_Raw;
extern volatile unsigned long pulseCount; // For ISR
extern unsigned long lastRpmReadTime_Task; 

// --- Menu System Variables ---
enum MenuScreen { 
    MAIN_MENU, 
    WIFI_SETTINGS, WIFI_SCAN, WIFI_PASSWORD_ENTRY, WIFI_STATUS, 
    MQTT_SETTINGS, MQTT_SERVER_ENTRY, MQTT_PORT_ENTRY, MQTT_USER_ENTRY, MQTT_PASS_ENTRY, MQTT_TOPIC_ENTRY,
    MQTT_DISCOVERY_SETTINGS,        
    MQTT_DISCOVERY_PREFIX_ENTRY,  
    OTA_UPDATE_SCREEN, 
    VIEW_STATUS, 
    CONFIRM_REBOOT 
};
extern volatile MenuScreen currentMenuScreen;
extern volatile int selectedMenuItem;
extern volatile int scanResultCount;

// New enum for WiFi scan state
enum WiFiScanState {
    WIFI_SCAN_IDLE,
    WIFI_SCAN_IN_PROGRESS,
    WIFI_SCAN_COMPLETED
};
extern volatile WiFiScanState currentWiFiScanState; // Declare new state variable

extern String scannedSSIDs[10]; 
extern char passwordInputBuffer[64]; 
extern char generalInputBuffer[128]; 
extern volatile int generalInputCharIndex;
extern volatile char currentGeneralEditChar;


extern volatile int passwordCharIndex; 
extern volatile char currentPasswordEditChar; 

// --- Fan Curve ---
const int MAX_CURVE_POINTS = 8; 
extern int tempPoints[MAX_CURVE_POINTS];
extern int pwmPercentagePoints[MAX_CURVE_POINTS];
extern int numCurvePoints;
extern volatile bool fanCurveChanged; 

// Staging Fan Curve for Serial Commands
extern int stagingTempPoints[MAX_CURVE_POINTS];
extern int stagingPwmPercentagePoints[MAX_CURVE_POINTS];
extern int stagingNumCurvePoints;

// --- Task Communication ---
extern volatile bool needsImmediateBroadcast; 
extern volatile bool rebootNeeded; 

// --- MQTT Configuration ---
extern volatile bool isMqttEnabled;
extern char mqttServer[64];
extern int mqttPort;
extern char mqttUser[64];
extern char mqttPassword[64]; 
extern char mqttBaseTopic[64];

// --- MQTT Discovery Configuration ---
extern volatile bool isMqttDiscoveryEnabled; 
extern char mqttDiscoveryPrefix[32];     
extern char mqttDeviceId[64];            
extern char mqttDeviceName[64];          

// --- OTA Update Status ---
extern volatile bool ota_in_progress;
extern String ota_status_message; 
extern String GITHUB_API_ROOT_CA_STRING; // Will hold the CA loaded from SPIFFS


// --- Global Objects (declared extern, defined in main.cpp) ---
extern Preferences preferences;
extern Adafruit_BMP280 bmp;
extern LiquidCrystal_I2C lcd;
extern AsyncWebServer server;
extern WebSocketsServer webSocket;
extern WiFiClient espClient; 
extern PubSubClient mqttClient; 

// --- Button Debouncing ---
extern unsigned long buttonPressTime[5]; 
extern bool buttonPressedState[5];
extern const long debounceDelay; 
extern const long longPressDelay; 

// --- SSID and Password (defined in main.cpp, might be loaded from NVS) ---
extern char current_ssid[64]; 
extern char current_password[64]; 

#endif // CONFIG_H
