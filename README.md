# **SmartWifiFanController 💨🌡️**

Developer: Daniele Viti

Repository: https://github.com/dnviti/SmartWifiFanController

A comprehensive ESP32-based fan controller with WiFi connectivity, web interface, LCD display, persistent configuration, **and advanced MQTT support with Home Assistant Discovery** for PC fans or similar applications. It uses a BMP280 temperature sensor to control fan speed via PWM, either through a configurable fan curve (Auto Mode) or a manually set speed (Manual Mode).

## **🌟 Features**

* **Dual Control Modes**:  
  * **Auto Mode**: Adjusts fan speed based on a user-defined temperature curve.  
  * **Manual Mode**: Allows setting a fixed fan speed percentage.  
* **Web Interface**:  
  * Real-time monitoring of temperature, fan speed (%), fan RPM, and current mode.  
  * Switch between Auto/Manual modes.  
  * Adjust manual fan speed via a slider.  
  * **Fan Curve Editor**: Graphically add, remove, and adjust points (temperature vs. PWM %) for Auto Mode. Curve is saved on the ESP32's non-volatile storage (NVS). Max 8 points, min 2 points per curve.  
  * **MQTT Configuration**: Configure MQTT broker details and enable/disable MQTT.  
  * **MQTT Discovery Configuration**: Enable/disable Home Assistant Discovery and set the discovery prefix.  
* **LCD Display (16x2 I2C)**:  
  * Shows current temperature, fan speed percentage, fan RPM, current mode, WiFi IP, **and MQTT & Discovery status indication.** \* **On-device Menu System**: Navigate using physical buttons to:  
    * Toggle WiFi enable/disable, scan, set SSID/password.  
    * **Toggle MQTT enable/disable, set broker server, port, user, password, base topic.** \* **Configure MQTT Discovery (enable/disable, set prefix).**  
    * View status and confirm reboots.  
* **MQTT Integration**:  
  * Publish device status (temperature, fan speed, RPM, mode, IP, RSSI, various settings states) to configurable MQTT topics.  
  * Subscribe to command topics to control mode, manual fan speed, and fan curve.  
  * **Home Assistant MQTT Discovery**: Automatically discovers and configures entities in Home Assistant for:  
    * Fan control (on/off, speed percentage, preset modes: AUTO/MANUAL).  
    * Sensors: Temperature, Fan RPM, Manual Target Speed, WiFi RSSI, IP Address.  
    * Diagnostic binary sensors: Temperature Sensor Status, WiFi Connection, Serial Debug, Reboot Needed.  
    * Configuration entities: Fan Curve (JSON text input), MQTT Discovery Enable (switch), MQTT Discovery Prefix (text input), Reboot Device (button).  
    * Diagnostic sensors for viewing current WiFi/MQTT configurations (SSID, Broker, Port, User, Base Topic).  
  * Supports MQTT Last Will and Testament for availability status.  
  * Configurable via LCD menu, serial commands, and relevant parts via Web UI/MQTT.  
* **Fan Control**:  
  * PWM output for fan speed control (25kHz frequency).  
  * Tachometer input for RPM monitoring.  
* **Temperature Sensing**:  
  * Utilizes Adafruit BMP280 sensor for ambient temperature readings.  
* **Persistent Configuration**:  
  * Saves WiFi credentials, **MQTT configuration, MQTT Discovery settings,** and the fan curve to NVS.  
* **Serial Debug/Command Interface**:  
  * Optional serial output for debugging.  
  * Allows advanced control, status checks, and configuration (including WiFi, MQTT, and MQTT Discovery) via serial commands.  
* **Connectivity**:  
  * Connects to your local WiFi network (STA mode).  
  * Serves a web page from SPIFFS.  
  * Uses WebSockets for real-time web UI communication.  
  * **Connects to an MQTT broker for home automation integration.** \* **Software Architecture**:  
  * Built using PlatformIO and the Arduino framework for ESP32.  
  * Utilizes FreeRTOS tasks for concurrent handling of network operations (Core 0: WiFi, Web, WebSockets, MQTT) and main application logic (Core 1).  
  * Modular C++ code structure.  
* **Firmware Version Tracking**: FIRMWARE\_VERSION defined in config.h.

## **⚙️ Hardware Requirements**

(No changes needed here unless MQTT requires specific hardware considerations, which it typically doesn't beyond network connectivity)

* **Microcontroller**: ESP32 (project configured for upesy\_wroom board, but adaptable).  
* **Temperature Sensor**: Adafruit BMP280 (I2C).  
* **Display**: 16x2 I2C LCD.  
* **Fan**: 3-pin or 4-pin PC Fan with PWM control and optionally tachometer.  
* **Buttons**: 5 push buttons for menu navigation.  
* **Optional**: Jumper/Switch for Serial Debug mode.  
* Standard electronic components.

Refer to src/config.h and src/main.cpp for default pin assignments.

## **📚 Software & Libraries**

This project is built using [PlatformIO](https://platformio.org/). Key libraries used include:

* espressif32 platform  
* arduino framework  
* ottowinter/ESPAsyncWebServer-esphome  
* esphome/AsyncTCP-esphome  
* links2004/WebSockets  
* **knolleary/PubSubClient (for MQTT)** \* adafruit/Adafruit BMP280 Library  
* adafruit/Adafruit Unified Sensor  
* iakop/LiquidCrystal\_I2C\_ESP32  
* bblanchon/ArduinoJson  
* Preferences (ESP32 NVS)  
* SPIFFS (ESP32 flash filesystem)

## **📂 Project Structure**

* data/: Web files (HTML, CSS, JS).  
* src/: C++ source code.  
  * main.cpp, config.h  
  * fan\_control.(h/cpp)  
  * display\_handler.(h/cpp)  
  * input\_handler.(h/cpp)  
  * network\_handler.(h/cpp) (WebSockets/HTTP)  
  * **mqtt\_handler.(h/cpp) (MQTT logic, including Discovery)** \* nvs\_handler.(h/cpp)  
  * tasks.(h/cpp)  
* platformio.ini: Project configuration.

## **🛠️ Setup & Installation**

1. Clone Repository.  
2. PlatformIO setup in VS Code.  
3. Configure Hardware Pins (if different from defaults in src/config.h or src/main.cpp).  
4. Build & Upload Firmware.  
5. Upload SPIFFS Filesystem Image.

## **🚀 Usage**

### **Initial Setup (WiFi & MQTT)**

* Use the LCD menu to:  
  1. Enable WiFi, scan, and connect to your network.  
  2. Enable MQTT and configure your broker details (server, port, user, password, base topic).  
  3. Optionally, configure MQTT Discovery settings (enable, prefix).  
  4. Reboot when prompted for settings to take effect.

### **Web Interface**

* Access via ESP32's IP. Monitor status, control modes, edit fan curve, configure MQTT and MQTT Discovery settings.

### **LCD Menu**

* Navigate to configure WiFi, **MQTT (including Discovery)**, and view status.

### **MQTT Interface & Home Assistant Discovery**

* **Availability:** YOUR\_BASE\_TOPIC/online\_status (payload: online/offline)  
* **Status (JSON):** YOUR\_BASE\_TOPIC/status\_json (publishes detailed state including sensor values and configuration states)  
* **Commands (examples):** \* YOUR\_BASE\_TOPIC/mode/set (payload: AUTO or MANUAL)  
  * YOUR\_BASE\_TOPIC/speed/set (payload: 0-100, for manual mode)  
  * YOUR\_BASE\_TOPIC/fancurve/set (payload: JSON array of curve points)  
  * YOUR\_BASE\_TOPIC/discovery\_enabled/set (payload: ON or OFF)  
  * YOUR\_BASE\_TOPIC/discovery\_prefix/set (payload: e.g., homeassistant)  
  * YOUR\_BASE\_TOPIC/reboot/set (payload: REBOOT)  
* **Home Assistant:** If MQTT Discovery is enabled on the ESP32 and Home Assistant's MQTT integration is configured for discovery (usually default), entities for the fan, sensors, and relevant configurations will be automatically created.

### **Serial Commands (Optional)**

* If serial debug is enabled (DEBUG\_ENABLE\_PIN HIGH at boot), connect at 115200 baud.  
* Type help for commands, including WiFi, **MQTT, and MQTT Discovery configuration** (e.g., mqtt\_discovery\_enable, set\_mqtt\_discovery\_prefix).

## **🔧 Configuration**

* **WiFi Credentials, MQTT Settings, MQTT Discovery Settings**: Set via LCD menu, Web UI, or serial commands. Saved to NVS.  
* **Fan Curve**: Configured via Web UI, MQTT, or serial commands. Saved to NVS.  
* **Pin Definitions**: Modify in src/config.h or src/main.cpp.  
* **PWM Parameters**: In src/main.cpp.

## **🤝 Contributing**

Contributions are welcome\! Please read the [CONTRIBUTING.md](http://docs.google.com/CONTRIBUTING.md) file for guidelines on how to contribute to this project.

## **📜 License**

This project is licensed under the MIT License \- see the [LICENSE.md](http://docs.google.com/LICENSE.md) file for details.