# **Chapter 3: Software**

This chapter details the software aspects of the ESP32 PC Fan Controller, including the development environment, required libraries, firmware architecture, and the structure of the codebase.

## **3.1. Development Environment**

* **Recommended IDE: PlatformIO IDE within Visual Studio Code**  
  * **Why:** PlatformIO offers superior library management (via platformio.ini), robust build system configuration, easy switching between boards/frameworks, and excellent integration with VS Code for features like IntelliSense, debugging, and version control.  
* **Alternative: Arduino IDE with ESP32 Core**  
  * Ensure the latest ESP32 core is installed via the Boards Manager.  
  * Library management is done manually through the Arduino Library Manager.

## **3.2. Libraries Required**

The following libraries are crucial for the project's functionality. When using PlatformIO, these are specified in the platformio.ini file for automatic management.

* **Core ESP32 Libraries (Bundled with ESP32 Arduino Core):**  
  * WiFi.h: For WiFi connectivity (station mode).  
  * Wire.h: For I2C communication with the LCD and BMP280 sensor.  
  * Preferences.h: For accessing Non-Volatile Storage (NVS) to save settings.  
  * SPIFFS.h: For accessing the SPI Flash File System to serve web files.  
  * Arduino.h: (Often implicitly included, but good practice to ensure core Arduino functions are available).  
* **Asynchronous** Web **Server & TCP:**  
  * **AsyncTCP.h**: (e.g., esp32async/AsyncTCP or me-no-dev/AsyncTCP for ESP32)  
    * Provides the underlying asynchronous TCP layer required by ESPAsyncWebServer.  
  * **ESPAsyncWebServer.h**: (e.g., esp32async/ESPAsyncWebServer or me-no-dev/ESP Async WebServer)  
    * A lightweight and efficient asynchronous HTTP and WebSocket server library. Essential for handling web requests and WebSocket connections without blocking other operations.  
* **WebSockets:**  
  * **WebSocketsServer.h**: (e.g., links2004/WebSockets by Markus Sattler)  
    * Used to implement the WebSocket server for real-time bidirectional communication with the web UI.  
* **Sensors:**  
  * **Adafruit\_BMP280.h**: (e.g., adafruit/Adafruit BMP280 Library)  
    * Driver for the BMP280 temperature and pressure sensor.  
  * **Adafruit\_Sensor.h**: (e.g., adafruit/Adafruit Unified Sensor)  
    * A unified sensor abstraction layer, a dependency for many Adafruit sensor libraries including the BMP280.  
* **Display:**  
  * **LiquidCrystal\_I2C.h**: (e.g., marcoschwartz/LiquidCrystal\_I2C or "LiquidCrystal I2C" by Frank de Brabander \- ensure ESP32 compatibility).  
    * Controls I2C-based character LCDs.  
* **JSON Handling:**  
  * **ArduinoJson.h**: (e.g., bblanchon/ArduinoJson \- version 7.x is recommended and used in the current codebase).  
    * Efficiently parses and generates JSON data for WebSocket communication.

**Example platformio.ini lib\_deps section:**

    lib_deps = 
        ; Async Web Server and TCP
        ottowinter/ESPAsyncWebServer-esphome @ ^3.3.0     ; A well-maintained fork, or use me-no-dev/ESP Async WebServer
        esphome/AsyncTCP-esphome @ ^2.1.4                 ; Dependency for the ESPAsyncWebServer fork above, or use me-no-dev/AsyncTCP

        ; WebSockets
        links2004/WebSockets @ ^2.6.1                     ; By Markus Sattler, very common

        ; Temperature Sensor
        adafruit/Adafruit BMP280 Library @ ^2.6.8
        adafruit/Adafruit Unified Sensor @ ^1.1.15        ; Dependency for BMP280

        ; I2C LCD
        iakop/LiquidCrystal_I2C_ESP32 @ ^1.1.6           ; By Frank de Brabander

        ; JSON Handling
        bblanchon/ArduinoJson @ ^7.4.1

*Note: Specific versions (@^x.y.z) can be added for better build reproducibility.*

## **3.3. Firmware Architecture**

The firmware is designed around the ESP32's dual-core capabilities using FreeRTOS tasks to ensure responsiveness and efficient handling of concurrent operations.

* **Dual-Core Task Allocation:**  
  * **Core 0 (Protocol Core \- networkTask):**  
    * **Responsibilities:** Exclusively handles network-related operations.  
    * Manages WiFi connection lifecycle (connecting, monitoring status).  
    * Runs the ESPAsyncWebServer to serve static web files (HTML, CSS, JS) from SPIFFS.  
    * Manages the WebSocketsServer for real-time communication with web clients.  
    * Processes incoming WebSocket messages and sends outgoing data broadcasts.  
    * This task is only created and run if WiFi is enabled in the configuration.  
  * **Core 1 (Application Core \- mainAppTask):**  
    * **Responsibilities:** Handles all main device logic and user interaction.  
    * Reads data from the BMP280 temperature sensor (if tempSensorFound).  
    * Calculates fan RPM using the tachometer input via an ISR.  
    * Manages the LCD, including updating the normal status display and rendering all menu screens.  
    * Processing inputs from the physical buttons for LCD menu navigation.  
    * Executing the core fan control algorithms (Auto mode based on temperature curve, Manual mode).  
    * Handling commands received via the Serial interface (if serialDebugEnabled).  
    * Updating shared state variables that are then read by networkTask for broadcasting.  
* **Interrupt Service Routine (ISR):**  
  * countPulse(): Attached to the FAN\_TACH\_PIN\_ACTUAL. This ISR increments a volatile pulse counter on each falling edge of the fan's tachometer signal. Being an ISR, it executes with high priority and minimal latency, ensuring accurate pulse counting even while other tasks are running.  
* **Shared Data and Inter-Task Communication:**  
  * **volatile Variables:** Global variables shared between tasks (e.g., currentTemperature, fanRpm, isAutoMode, fanSpeedPercentage) are declared volatile to prevent compiler optimizations that might lead to stale data reads.  
  * **needsImmediateBroadcast Flag:** A volatile bool flag used by mainAppTask to signal networkTask that critical state has changed and an immediate WebSocket broadcast is required, rather than waiting for the next periodic broadcast.  
  * **Fan Curve Arrays:** tempPoints and pwmPercentagePoints are global. Updates from the web UI (via networkTask) or serial commands (via mainAppTask) modify these directly. NVS saving acts as the commit. For very high-frequency concurrent access, a mutex would be advisable, but for user-driven changes, this is generally acceptable.  
* **State-Driven Logic:**  
  * The system operates based on several key state flags like isAutoMode, isInMenuMode, isWiFiEnabled, and tempSensorFound. The behavior of tasks and functions adapts based on these states.

**Conceptual Task Interaction:**

    Core 0 (Network)                     Core 1 (Application)  
    +------------------+                    +-----------------------+  
    |  networkTask     |<-------------------| mainAppTask           |  
    | - WiFi Connect   | (needsBroadcast)   | - Read Sensors (Temp) |  
    | - Web Server     |                    | - Read Tachometer     |  
    | - WebSocket Srv  |---(Web Cmds)------>|   (via ISR)           |  
    |   - Listen       |<--(Shared State)---| - Update LCD          |  
    |   - Broadcast    |                    | - Handle Buttons      |  
    +------------------+                    | - Handle Serial Cmds  |  
                                            | - Fan Control Logic   |  
                                            +-----------------------+  
                                                      | (ISR)  
                                            +-----------------------+  
                                            | countPulse (Tach)     |  
                                            +-----------------------+

## **3.4. Code Structure (Key Files in src/)**

The project is organized into several header (.h) and source (.cpp) files to promote modularity and maintainability:

* **main.cpp:**  
  * Includes all other custom headers.  
  * Contains the definitions for all global variables declared extern in config.h.  
  * The main setup() function: Initializes hardware, loads configurations from NVS, and creates the FreeRTOS tasks.  
  * The main loop() function: Kept minimal, as primary operations are handled by FreeRTOS tasks.  
* **config.h:**  
  * Central header file included by most other files.  
  * Defines constants (pin numbers, PWM parameters, array sizes like MAX\_CURVE\_POINTS).  
  * Contains extern declarations for all global variables and objects (e.g., lcd, server, bmp, state flags). This allows other files to access these globals after they are defined in main.cpp.  
  * Defines enums (e.g., MenuScreen).  
* **nvs\_handler.h / nvs\_handler.cpp:**  
  * Encapsulates all functions related to Non-Volatile Storage (NVS) using the Preferences library.  
  * saveWiFiConfig(), loadWiFiConfig()  
  * saveFanCurveToNVS(), loadFanCurveFromNVS()  
* **fan\_control.h / fan\_control.cpp:**  
  * Contains logic related to fan operation.  
  * setDefaultFanCurve(): Initializes the default fan curve.  
  * calculateAutoFanPWMPercentage(): Calculates the target fan speed based on temperature and the current curve.  
  * setFanSpeed(): Applies a given percentage to the fan via PWM (using ESP32 LEDC functions).  
  * IRAM\_ATTR countPulse(): The Interrupt Service Routine for counting fan tachometer pulses.  
* **display\_handler.h / display\_handler.cpp:**  
  * Manages all output to the I2C LCD.  
  * updateLCD\_NormalMode(): Renders the standard status display.  
  * displayMenu(): Main function to call specific menu rendering functions.  
  * displayMainMenu(), displayWiFiSettingsMenu(), etc.: Functions to render each specific screen of the LCD menu system.  
* **input\_handler.h / input\_handler.cpp:**  
  * Handles user inputs.  
  * handleMenuInput(): Processes physical button presses, debounces them, and updates menu states or triggers actions.  
  * handleSerialCommands(): Parses and executes commands received via the Serial interface when debug mode is active.  
  * Includes helper functions called by menu/serial actions like performWiFiScan(), attemptWiFiConnection(), disconnectWiFi().  
* **network\_handler.h / network\_handler.cpp:**  
  * Manages all web-related functionalities.  
  * setupWebServerRoutes(): Configures the AsyncWebServer to serve static files (index.html, style.css, script.js) from SPIFFS.  
  * webSocketEvent(): The callback function for handling WebSocket events (connect, disconnect, text messages). Parses incoming JSON commands from the web UI.  
  * broadcastWebSocketData(): Constructs and sends JSON data containing the current system status to all connected WebSocket clients.  
* **tasks.h / tasks.cpp:**  
  * Defines and implements the FreeRTOS tasks.  
  * networkTask(void \*pvParameters): The function executed by Core 0\.  
  * mainAppTask(void \*pvParameters): The function executed by Core 1\.  
  * Includes extern TaskHandle\_t declarations for task handles (definitions are in main.cpp).

This modular structure makes the codebase easier to understand, debug, and extend.

[Previous Chapter: Hardware](02-hardware.md) | [Next Chapter: Setup and Installation](04-setup-and-installation.md)