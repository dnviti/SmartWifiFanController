# **SmartWifiFanController 💨🌡️**

Developer: Daniele Viti

Repository: https://github.com/dnviti/SmartWifiFanController

A comprehensive ESP32-based fan controller with WiFi connectivity, web interface, LCD display, persistent configuration, advanced MQTT support with Home Assistant Discovery, and Over-the-Air (OTA) firmware updates for PC fans or similar applications. It uses an optional BMP280 temperature sensor to control fan speed via PWM, either through a configurable fan curve (Auto Mode) or a manually set speed (Manual Mode).

## **🌟 Features Summary**

* **Controls PC Fans:** Manages one or more 4-pin PWM PC fans (current software focuses on one, easily expandable).  
* **Temperature Sensing:** Optional Adafruit BMP280 sensor for precise thermal readings.  
* **Dual Control Modes:**  
  * **Auto Mode:** Adjusts fan speed based on a user-configurable multi-point temperature curve. Editable via Web UI, MQTT, or Serial.  
  * **Manual Mode:** Allows setting a fixed fan speed percentage (0-100%) selectable via LCD, Web UI, Serial, or MQTT.  
* **Real-time Monitoring:**  
  * Fan RPM monitoring via tachometer input.  
  * Displays temperature, fan speed (%), RPM, and mode on LCD, Web UI, and publishes via MQTT.  
* **User Interfaces:**  
  * **16x2 I2C LCD Display:** Live status and comprehensive menu-driven configuration.  
  * **Physical Buttons (5x):** Intuitive LCD menu navigation (Menu, Up, Down, Select, Back).  
  * **Web Interface (Optional WiFi):**  
    * Served from SPIFFS for remote monitoring and configuration.  
    * Real-time data synchronization using WebSockets.  
    * Includes Fan Curve Editor, MQTT/Discovery settings, and OTA update trigger.  
  * **Conditional Serial Debug/Command Interface:**  
    * Activated by a physical GPIO pin (DEBUG\_ENABLE\_PIN).  
    * Provides extensive runtime logs and a command-line interface for advanced control and diagnostics.  
* **Over-the-Air (OTA) Firmware Updates:**  
  * **ElegantOTA Integration:** Web endpoint (/update) for manually uploading firmware.bin or spiffs.bin files.  
  * **GitHub Release Updater:**  
    * Checks the project's GitHub repository (dnviti/SmartWifiFanController) for the latest release.  
    * Compares release version with current firmware version.  
    * If newer, allows download and update of firmware and SPIFFS image from GitHub release assets.  
    * Triggerable via LCD Menu, Web UI, or Serial Command.  
    * Uses a Root CA certificate loaded from SPIFFS (e.g., /github\_api\_ca.pem) for secure HTTPS connections.  
* **MQTT & Home Automation:**  
  * Optional MQTT connectivity.  
  * Publishes device status (temperature, fan speed, RPM, mode, IP, RSSI, settings states, firmware version, OTA status) to configurable topics.  
  * Subscribes to command topics for remote control (mode, manual speed, fan curve, select configurations).  
  * **Home Assistant MQTT Discovery:** Automatically creates entities in Home Assistant for fan control, sensors, and configurations.  
  * Supports MQTT Last Will and Testament (LWT).  
* **Configuration Management:**  
  * Full WiFi, MQTT, and MQTT Discovery configuration manageable via all interfaces.  
  * Persistent storage of all critical settings (WiFi, MQTT, Discovery, fan curve, operational states) in ESP32's Non-Volatile Storage (NVS).  
* **System Architecture:**  
  * **Dual-Core Operation (FreeRTOS):**  
    * Core 0: Network tasks (WiFi, Web Server, WebSockets, MQTT Client, ElegantOTA).  
    * Core 1: Main application logic (sensors, LCD, buttons, fan control, serial commands, GitHub OTA checks).  
  * Modular C++ code structure for maintainability.  
* **Diagnostics & Usability:**  
  * Firmware version tracking (FIRMWARE\_VERSION).  
  * **Dynamic Hostname:** Sets hostname to fancontrol-\[macaddress\] for easier network identification.  
  * Onboard LED indicates debug mode status.  
  * Web UI features a loading animation.  
* **Development & Deployment:**  
  * **Automated GitHub Release Pipeline:** Builds firmware and SPIFFS (including the latest CA certificate downloaded during the pipeline) upon pushes to release/v\* branches, creating versioned and "latest" GitHub releases.

## **⚙️ Hardware Requirements**

* **Microcontroller**: ESP32 development board (e.g., ESP32-WROOM-32 based).  
* **Temperature Sensor (Optional)**: Adafruit BMP280 (I2C).  
* **Display**: 16x2 I2C LCD.  
* **Fan(s)**: At least one 4-pin PWM PC cooling fan (12V).  
* **Buttons**: 5x tactile push buttons.  
* **Power Supply**: 12V DC source and a 12V to 5V buck converter module.  
* **Logic Level Shifter**: For 3.3V ESP32 PWM to 5V fan PWM signal (recommended).  
* **Resistors**: For I2C pull-ups (if not on modules) and fan tachometer pull-up.  
* **Jumper/Switch (Optional)**: For enabling Serial Debug mode.  
* Standard prototyping supplies (breadboard, wires, connectors).

Refer to src/config.h and src/main.cpp for default pin assignments.

## **📚 Software & Libraries**

This project is built using [PlatformIO](https://platformio.org/) with the Arduino framework. Key libraries include:

* esp32async/ESPAsyncWebServer & esp32async/AsyncTCP (for Web Server and base for ElegantOTA)  
* ayushsharma82/ElegantOTA (for manual web-based OTA updates)  
* links2004/WebSockets (for real-time Web UI communication)  
* knolleary/PubSubClient (for MQTT communication)  
* adafruit/Adafruit BMP280 Library & adafruit/Adafruit Unified Sensor  
* iakop/LiquidCrystal\_I2C\_ESP32 (or your preferred I2C LCD library)  
* bblanchon/ArduinoJson (for JSON parsing and generation)  
* Core ESP32 libraries: WiFi, Wire, Preferences, SPIFFS, HTTPClient, HTTPUpdate.

See platformio.ini for specific versions and dependencies.

## **📂 Project Structure**

* data/: Contains web files (index.html, style.css, script.js). The GitHub Actions pipeline also places the downloaded CA certificate (e.g., github\_api\_ca.pem) here before building the spiffs.bin for releases.  
* src/: Main C++ source code.  
  * main.cpp: Core setup, loop, global variable definitions.  
  * config.h: Pin definitions, constants, global extern declarations.  
  * fan\_control.(h/cpp): Fan PWM and RPM logic.  
  * display\_handler.(h/cpp): LCD menu and status display rendering.  
  * input\_handler.(h/cpp): Button and serial command processing.  
  * network\_handler.(h/cpp): Web server routes and WebSocket event handling.  
  * mqtt\_handler.(h/cpp): MQTT connection, publishing, and subscription logic.  
  * nvs\_handler.(h/cpp): Non-Volatile Storage management.  
  * tasks.(h/cpp): FreeRTOS task definitions and implementations.  
  * ota\_updater.(h/cpp): Logic for checking and applying updates from GitHub releases.  
* platformio.ini: PlatformIO project configuration.  
* partitions\_4MB.csv / partitions\_8MB.csv: Custom partition tables for OTA.  
* .github/workflows/release.yml: GitHub Actions workflow for automated builds and releases.  
* docs/: Detailed documentation for the project.

## **🛠️ Setup & Installation**

1. **Clone Repository:** git clone https://github.com/dnviti/SmartWifiFanController.git  
2. **PlatformIO Setup:** Open the cloned project in VS Code with the PlatformIO IDE extension installed. PlatformIO will handle library dependencies.  
3. **Hardware Assembly:** Wire components according to the schematics/pin definitions (see docs/02-hardware.md and src/config.h).  
4. **CA Certificate for Local Development (First Time/Testing):**  
   * If you plan to test the GitHub OTA update feature with a locally built firmware, create a file named github\_api\_ca.pem (or matching GITHUB\_ROOT\_CA\_FILENAME in src/config.h) inside the data/ directory.  
   * Paste the current Root CA certificate for api.github.com (e.g., USERTrust ECC Certification Authority PEM) into this file.  
   * *Note: For official releases built by the GitHub Actions pipeline, this certificate is downloaded and included automatically.*  
5. **Build & Upload Filesystem Image (SPIFFS):**  
   * In PlatformIO, run the "Upload Filesystem Image" task for your environment. This uploads files from the data/ directory.  
6. **Build & Upload Firmware (Initial):**  
   * In PlatformIO, ensure upload\_protocol \= espota is commented out in platformio.ini.  
   * Run the "Upload" task for your environment (this will use serial).

## **🚀 Usage**

### **Initial Configuration**

* Upon first boot, or if settings are not configured, use the **LCD Menu** to:  
  1. Navigate to "WiFi Settings", enable WiFi, scan for networks, select your SSID, and enter the password.  
  2. (Optional) Navigate to "MQTT Settings", enable MQTT, and configure your broker details (server, port, user, password, base topic).  
  3. (Optional) In "MQTT Settings" \-\> "Discovery Cfg", enable MQTT Discovery and set the prefix if needed.  
  4. The device may indicate rebootNeeded. Confirm reboot via the menu or serial command if prompted.

### **Accessing Interfaces**

* **LCD Menu:** Use the physical buttons to navigate and configure.  
* **Web Interface:** Once WiFi is connected, access http://\<ESP32\_IP\_ADDRESS\> (or http://fancontrol-\[macaddress\].local if mDNS is working on your network).  
  * View status, control modes, edit fan curve, manage MQTT/Discovery settings.  
  * **Trigger GitHub OTA Update:** A dedicated section allows checking for and installing updates.  
  * **Manual OTA (ElegantOTA):** Access http://\<ESP32\_IP\_ADDRESS\>/update to upload .bin files.  
* **Serial Commands:** If debug mode is enabled (DEBUG\_ENABLE\_PIN HIGH at boot), connect via Serial Monitor (115200 baud). Type help for commands.  
  * Includes status, mode control, WiFi/MQTT config, fan curve management, and ota\_update.

### **Over-the-Air (OTA) Updates**

1. **Via ElegantOTA Web UI (/update):**  
   * Manually upload firmware.bin or spiffs.bin obtained from a local build or a GitHub release.  
2. **Via GitHub Release Updater (LCD, Web UI, or Serial):**  
   * The device will check the [SmartWifiFanController GitHub Releases](https://github.com/dnviti/SmartWifiFanController/releases/latest).  
   * If a newer version (based on tag name) is found, it will download and apply the firmware.bin and then, if available, the spiffs.bin from that release.  
   * This requires the Root CA certificate (github\_api\_ca.pem) to be present on SPIFFS.  
   * The device reboots after successful updates.

## **🔧 Configuration Details**

* **WiFi Credentials, MQTT Settings, Discovery Settings:** Stored in NVS. Configurable via LCD, Web UI, Serial.  
* **Fan Curve:** Stored in NVS. Configurable via Web UI, MQTT, Serial.  
* **Pin Definitions:** In src/config.h and src/main.cpp.  
* **OTA GitHub Repository & CA Filename:** Defined in src/config.h.  
* **PlatformIO Build Environment Name (PIO\_BUILD\_ENV\_NAME):** Defined in src/config.h, must match the environment name used in the GitHub Actions workflow for release asset compatibility.

## **🤝 Contributing**

Contributions are welcome\! Please read the [CONTRIBUTING.md](http://docs.google.com/CONTRIBUTING.md) file for guidelines.

## **📜 License**

This project is licensed under the MIT License \- see the [LICENSE.md](http://docs.google.com/LICENSE.md) file for details.