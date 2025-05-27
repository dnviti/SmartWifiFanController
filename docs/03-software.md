# **Chapter 3: Software**

This chapter details the software aspects of the ESP32 PC Fan Controller, including the development environment, required libraries, firmware architecture, and the structure of the codebase.

## **3.1. Development Environment**

* **Recommended IDE: PlatformIO IDE within Visual Studio Code** * **Why:** PlatformIO offers superior library management (via platformio.ini), robust build system configuration, easy switching between boards/frameworks, and excellent integration with VS Code for features like IntelliSense, debugging, and version control.  
* **Alternative: Arduino IDE with ESP32 Core** * Ensure the latest ESP32 core is installed via the Boards Manager.  
  * Library management is done manually through the Arduino Library Manager.

## **3.2. Libraries Required**

The following libraries are crucial for the project's functionality. When using PlatformIO, these are specified in the platformio.ini file for automatic management.

* **Core ESP32 Libraries (Bundled with ESP32 Arduino Core):** * WiFi.h: For WiFi connectivity (station mode).  
  * Wire.h: For I2C communication with the LCD and BMP280 sensor.  
  * Preferences.h: For accessing Non-Volatile Storage (NVS) to save settings.  
  * SPIFFS.h: For accessing the SPI Flash File System to serve web files.  
  * Arduino.h: (Often implicitly included, but good practice to ensure core Arduino functions are available).  
* **Asynchronous Web Server & TCP:** * **AsyncTCP.h**: (e.g., esp32async/AsyncTCP or me-no-dev/AsyncTCP for ESP32)  
    * Provides the underlying asynchronous TCP layer required by ESPAsyncWebServer.  
  * **ESPAsyncWebServer.h**: (e.g., esp32async/ESPAsyncWebServer or me-no-dev/ESP Async WebServer)  
    * A lightweight and efficient asynchronous HTTP and WebSocket server library. Essential for handling web requests and WebSocket connections without blocking other operations.  
* **WebSockets:** * **WebSocketsServer.h**: (e.g., links2004/WebSockets by Markus Sattler)  
    * Used to implement the WebSocket server for real-time bidirectional communication with the web UI.  
* **MQTT Client:**
  * **PubSubClient.h**: (e.g., knolleary/PubSubClient)
    * A popular and lightweight MQTT client library for Arduino and ESP8266/ESP32. Used for connecting to an MQTT broker, publishing status, and subscribing to commands.
* **Sensors:** * **Adafruit\_BMP280.h**: (e.g., adafruit/Adafruit BMP280 Library)  
    * Driver for the BMP280 temperature and pressure sensor.  
  * **Adafruit\_Sensor.h**: (e.g., adafruit/Adafruit Unified Sensor)  
    * A unified sensor abstraction layer, a dependency for many Adafruit sensor libraries including the BMP280.  
* **Display:** * **LiquidCrystal\_I2C.h**: (e.g., marcoschwartz/LiquidCrystal\_I2C or "LiquidCrystal I2C" by Frank de Brabander \- ensure ESP32 compatibility).  
    * Controls I2C-based character LCDs.  
* **JSON Handling:** * **ArduinoJson.h**: (e.g., bblanchon/ArduinoJson \- version 7.x is recommended and used in the current codebase).  
    * Efficiently parses and generates JSON data for WebSocket and MQTT communication.

**Example platformio.ini lib\_deps section:**

    lib_deps = 
        ; Async Web Server and TCP
        ottowinter/ESPAsyncWebServer-esphome @ ^3.3.0     
        esphome/AsyncTCP-esphome @ ^2.1.4                 

        ; WebSockets
        links2004/WebSockets @ ^2.6.1                     

        ; MQTT Client
        knolleary/PubSubClient @ ^2.8

        ; Temperature Sensor
        adafruit/Adafruit BMP280 Library @ ^2.6.8
        adafruit/Adafruit Unified Sensor @ ^1.1.15        

        ; I2C LCD
        iakop/LiquidCrystal_I2C_ESP32 @ ^1.1.6           

        ; JSON Handling
        bblanchon/ArduinoJson @ ^7.4.1

*Note: Specific versions (@^x.y.z) can be added for better build reproducibility.*

## **3.3. Firmware Architecture**

The firmware is designed around the ESP32's dual-core capabilities using FreeRTOS tasks to ensure responsiveness and efficient handling of concurrent operations.

* **Dual-Core Task Allocation:** * **Core 0 (Protocol Core \- networkTask):** * **Responsibilities:** Exclusively handles network-related operations.  
    * Manages WiFi connection lifecycle (connecting, monitoring status).  
    * Runs the ESPAsyncWebServer to serve static web files (HTML, CSS, JS) from SPIFFS.  
    * Manages the WebSocketsServer for real-time communication with web clients.  
    * **Manages the MQTT client: connects to the broker, handles publishing status, subscribes to command topics, and processes incoming messages.**
    * Processes incoming WebSocket messages and sends outgoing data broadcasts.  
    * This task handles network services only if WiFi is enabled in the configuration. MQTT services run if both WiFi and MQTT are enabled.
  * **Core 1 (Application Core \- mainAppTask):** * **Responsibilities:** Handles all main device logic and user interaction.  
    * Reads data from the BMP280 temperature sensor (if tempSensorFound).  
    * Calculates fan RPM using the tachometer input via an ISR.  
    * Manages the LCD, including updating the normal status display and rendering all menu screens.  
    * Processing inputs from the physical buttons for LCD menu navigation.  
    * Executing the core fan control algorithms (Auto mode based on temperature curve, Manual mode).  
    * Handling commands received via the Serial interface (if serialDebugEnabled).  
    * Updating shared state variables that are then read by networkTask for broadcasting (WebSockets and MQTT).  
* **Interrupt Service Routine (ISR):** * countPulse(): Attached to the FAN\_TACH\_PIN\_ACTUAL. This ISR increments a volatile pulse counter on each falling edge of the fan's tachometer signal. Being an ISR, it executes with high priority and minimal latency, ensuring accurate pulse counting even while other tasks are running.  
* **Shared Data and Inter-Task Communication:** * **volatile Variables:** Global variables shared between tasks (e.g., currentTemperature, fanRpm, isAutoMode, fanSpeedPercentage) are declared volatile to prevent compiler optimizations that might lead to stale data reads.  
  * **needsImmediateBroadcast Flag:** A volatile bool flag used by mainAppTask to signal networkTask that critical state has changed and an immediate WebSocket broadcast **and MQTT status publish** is required, rather than waiting for the next periodic broadcast/publish.  
  * **Fan Curve Arrays:** tempPoints and pwmPercentagePoints are global. Updates from the web UI (via networkTask) or serial commands (via mainAppTask) modify these directly. NVS saving acts as the commit.  
* **State-Driven Logic:** * The system operates based on several key state flags like isAutoMode, isInMenuMode, isWiFiEnabled, **isMqttEnabled,** and tempSensorFound. The behavior of tasks and functions adapts based on these states.

**Conceptual Task Interaction:**

    Core 0 (Network)                                Core 1 (Application)  
    +------------------------------------+              +-----------------------+  
    |  networkTask                       |<-------------| mainAppTask           |  
    | - WiFi Connect                     | (needsBcast) | - Read Sensors (Temp) |  
    | - Web Server                       |              | - Read Tachometer     |  
    | - WebSocket Srv                    |--(Web Cmds)-->|   (via ISR)           |  
    |   - Listen                         |<--(ShrdState)-| - Update LCD          |  
    |   - Broadcast                      |              | - Handle Buttons      |  
    | - MQTT Client                      |              | - Handle Serial Cmds  |  
    |   - Connect, Subscribe, Publish    |--(MQTT Cmds)-->| - Fan Control Logic   |  
    |   - Handle Incoming MQTT Msgs      |              +-----------------------+  
    +------------------------------------+                        | (ISR)  
                                                      +-----------------------+  
                                                      | countPulse (Tach)     |  
                                                      +-----------------------+

## **3.4. Code Structure (Key Files in src/)**

The project is organized into several header (.h) and source (.cpp) files to promote modularity and maintainability:

* **main.cpp:** * Includes all other custom headers.  
  * Contains the definitions for all global variables declared extern in config.h.  
  * The main setup() function: Initializes hardware, loads configurations from NVS (including WiFi and MQTT), and creates the FreeRTOS tasks.  
  * The main loop() function: Kept minimal, as primary operations are handled by FreeRTOS tasks.  
* **config.h:** * Central header file included by most other files.  
  * Defines constants (pin numbers, PWM parameters, array sizes like MAX\_CURVE\_POINTS).  
  * Contains extern declarations for all global variables and objects (e.g., lcd, server, bmp, state flags, **mqttClient**).  
  * Defines enums (e.g., MenuScreen, now including MQTT configuration screens).  
* **nvs\_handler.h / nvs\_handler.cpp:** * Encapsulates all functions related to Non-Volatile Storage (NVS) using the Preferences library.  
  * saveWiFiConfig(), loadWiFiConfig()  
  * **saveMqttConfig(), loadMqttConfig()**
  * saveFanCurveToNVS(), loadFanCurveFromNVS()  
* **fan\_control.h / fan_control.cpp:** (No direct MQTT changes, but its state is published by MQTT)
  * Contains logic related to fan operation.  
* **display\_handler.h / display_handler.cpp:** * Manages all output to the I2C LCD.  
  * updateLCD\_NormalMode(): Renders the standard status display, **potentially indicating MQTT connection status.** * displayMenu(): Main function to call specific menu rendering functions.  
  * displayMainMenu(), displayWiFiSettingsMenu(), etc.  
  * **displayMqttSettingsMenu(), displayMqttEntryMenu(): Functions to render MQTT configuration screens.**
* **input\_handler.h / input_handler.cpp:** * Handles user inputs.  
  * handleMenuInput(): Processes physical button presses, debounces them, and updates menu states or triggers actions for WiFi **and MQTT configuration**.  
  * handleSerialCommands(): Parses and executes commands received via the Serial interface when debug mode is active, **including new MQTT commands.** * **network\_handler.h / network_handler.cpp:** * Manages WebSocket and HTTP server functionalities. (MQTT logic is primarily in `mqtt_handler`).
* **mqtt\_handler.h / mqtt_handler.cpp:** (New Module)
  * **Manages all MQTT client operations.**
  * **setupMQTT(): Initializes MQTT client with server details and topics from config.**
  * **connectMQTT(): Handles connection and re-connection logic to the MQTT broker, including LWT.**
  * **loopMQTT(): Called regularly by networkTask to maintain connection and process incoming messages.**
  * **publishStatusMQTT(): Constructs and publishes JSON status payload to the defined MQTT topic.**
  * **mqttCallback(): Processes incoming messages from subscribed MQTT command topics.**
* **tasks.h / tasks.cpp:** * Defines and implements the FreeRTOS tasks.  
  * networkTask(void \*pvParameters): The function executed by Core 0. **Now also initializes, manages, and loops the MQTT client if enabled.** * mainAppTask(void \*pvParameters): The function executed by Core 1.  

This modular structure makes the codebase easier to understand, debug, and extend.

[Previous Chapter: Hardware](02-hardware.md) | [Next Chapter: Setup and Installation](04-setup-and-installation.md)
