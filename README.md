# SmartWifiFanController 💨🌡️

**Developer**: Daniele Viti
**Repository**: [https://github.com/dnviti/SmartWifiFanController](https://github.com/dnviti/SmartWifiFanController)

A comprehensive ESP32-based fan controller with WiFi connectivity, web interface, LCD display, persistent configuration, **and MQTT support** for PC fans or similar applications. It uses a BMP280 temperature sensor to control fan speed via PWM, either through a configurable fan curve (Auto Mode) or a manually set speed (Manual Mode).

## 🌟 Features

* **Dual Control Modes**:
    * **Auto Mode**: Adjusts fan speed based on a user-defined temperature curve.
    * **Manual Mode**: Allows setting a fixed fan speed percentage.
* **Web Interface**:
    * Real-time monitoring of temperature, fan speed (%), fan RPM, and current mode.
    * Switch between Auto/Manual modes.
    * Adjust manual fan speed via a slider.
    * **Fan Curve Editor**: Graphically add, remove, and adjust points (temperature vs. PWM %) for Auto Mode. Curve is saved on the ESP32's non-volatile storage (NVS). Max 8 points, min 2 points per curve.
* **LCD Display (16x2 I2C)**:
    * Shows current temperature, fan speed percentage, fan RPM, current mode, WiFi IP, **and MQTT status indication.**
    * **On-device Menu System**: Navigate using physical buttons to:
        * Toggle WiFi enable/disable, scan, set SSID/password.
        * **Toggle MQTT enable/disable, set broker server, port, user, password, base topic.**
        * View status and confirm reboots.
* **MQTT Integration**:
    * Publish device status (temperature, fan speed, RPM, mode, etc.) to configurable MQTT topics.
    * Subscribe to command topics to control mode and manual fan speed.
    * Supports MQTT Last Will and Testament for availability status.
    * Configurable via LCD menu and serial commands.
* **Fan Control**:
    * PWM output for fan speed control (25kHz frequency).
    * Tachometer input for RPM monitoring.
* **Temperature Sensing**:
    * Utilizes Adafruit BMP280 sensor for ambient temperature readings.
* **Persistent Configuration**:
    * Saves WiFi credentials, **MQTT configuration,** and the fan curve to NVS.
* **Serial Debug/Command Interface**:
    * Optional serial output for debugging.
    * Allows advanced control, status checks, and configuration (including MQTT) via serial commands.
* **Connectivity**:
    * Connects to your local WiFi network (STA mode).
    * Serves a web page from SPIFFS.
    * Uses WebSockets for real-time web UI communication.
    * **Connects to an MQTT broker for home automation integration.**
* **Software Architecture**:
    * Built using PlatformIO and the Arduino framework for ESP32.
    * Utilizes FreeRTOS tasks for concurrent handling of network operations (Core 0: WiFi, Web, WebSockets, MQTT) and main application logic (Core 1).
    * Modular C++ code structure.

## ⚙️ Hardware Requirements
(No changes needed here unless MQTT requires specific hardware considerations, which it typically doesn't beyond network connectivity)

* **Microcontroller**: ESP32 (project configured for `upesy_wroom` board, but adaptable).
* **Temperature Sensor**: Adafruit BMP280 (I2C).
* **Display**: 16x2 I2C LCD.
* **Fan**: 3-pin or 4-pin PC Fan with PWM control and optionally tachometer.
* **Buttons**: 5 push buttons for menu navigation.
* **Optional**: Jumper/Switch for Serial Debug mode.
* Standard electronic components.

Refer to `src/config.h` and `src/main.cpp` for default pin assignments.

## 📚 Software & Libraries

This project is built using [PlatformIO](https://platformio.org/). Key libraries used include:

* `espressif32` platform
* `arduino` framework
* `ottowinter/ESPAsyncWebServer-esphome`
* `esphome/AsyncTCP-esphome`
* `links2004/WebSockets`
* **`knolleary/PubSubClient` (for MQTT)**
* `adafruit/Adafruit BMP280 Library`
* `adafruit/Adafruit Unified Sensor`
* `iakop/LiquidCrystal_I2C_ESP32`
* `bblanchon/ArduinoJson`
* `Preferences` (ESP32 NVS)
* `SPIFFS` (ESP32 flash filesystem)

## 📂 Project Structure
(Minor update to mention mqtt_handler)
* `data/`: Web files (HTML, CSS, JS).
* `src/`: C++ source code.
    * `main.cpp`, `config.h`
    * `fan_control.(h/cpp)`
    * `display_handler.(h/cpp)`
    * `input_handler.(h/cpp)`
    * `network_handler.(h/cpp)` (WebSockets/HTTP)
    * **`mqtt_handler.(h/cpp)` (MQTT logic)**
    * `nvs_handler.(h/cpp)`
    * `tasks.(h/cpp)`
* `platformio.ini`: Project configuration.

## 🛠️ Setup & Installation
(No major changes, but MQTT config is now part of initial setup if desired)
1.  Clone Repository.
2.  PlatformIO setup in VS Code.
3.  Configure Hardware Pins (if different from defaults in `src/config.h` or `src/main.cpp`).
4.  Build & Upload Firmware.
5.  Upload SPIFFS Filesystem Image.

## 🚀 Usage

### Initial Setup (WiFi & MQTT)

* Use the LCD menu to:
    1.  Enable WiFi, scan, and connect to your network.
    2.  Enable MQTT and configure your broker details (server, port, user, password, base topic).
    3.  Reboot when prompted for settings to take effect.

### Web Interface
(No direct MQTT control in Web UI yet)
* Access via ESP32's IP. Monitor status, control modes, edit fan curve.

### LCD Menu
* Navigate to configure WiFi, **MQTT**, and view status.

### MQTT Interface
* **Availability:** `YOUR_BASE_TOPIC/online_status` (payload: `online`/`offline`)
* **Status (JSON):** `YOUR_BASE_TOPIC/status_json` (publishes temp, fan speed/RPM, mode, etc.)
* **Commands:**
    * `YOUR_BASE_TOPIC/mode/set` (payload: `AUTO` or `MANUAL`)
    * `YOUR_BASE_TOPIC/speed/set` (payload: `0`-`100`, for manual mode)

### Serial Commands (Optional)
* If serial debug is enabled (`DEBUG_ENABLE_PIN` HIGH at boot), connect at 115200 baud.
* Type `help` for commands, including WiFi and **MQTT configuration** (`mqtt_enable`, `set_mqtt_server`, etc.).

## 🔧 Configuration

* **WiFi Credentials & MQTT Settings**: Set via LCD menu or serial commands. Saved to NVS.
* **Fan Curve**: Configured via Web UI or serial commands. Saved to NVS.
* **Pin Definitions**: Modify in `src/config.h` or `src/main.cpp`.
* **PWM Parameters**: In `src/main.cpp`.

## 🤝 Contributing
(Content as before)

## 📜 License
(Content as before)
