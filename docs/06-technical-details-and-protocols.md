# **Chapter 6: Technical Details & Protocols**

This chapter delves into the specific technologies, protocols, and implementation details used within the ESP32 PC Fan Controller project.

## **6.1. PWM Fan Control**

* **Mechanism:** PC fans with 4-pin connectors are controlled using Pulse Width Modulation (PWM). The fourth pin (Control/PWM Input) accepts a PWM signal that dictates the fan's speed. The fan motor itself is still powered by the constant 12V line.  
* **ESP32 Implementation:** The ESP32's LEDC (LED Control) peripheral is utilized for generating hardware PWM signals. This is preferred over software PWM for accuracy and minimal CPU overhead.  
  * ledcSetup(channel, frequency, resolution\_bits): Configures a PWM channel with a specific frequency and resolution.  
    * frequency: Set to PWM\_FREQ (typically 25000 Hz, i.e., 25kHz), which is standard for PC fans.  
    * resolution\_bits: Set to PWM\_RESOLUTION\_BITS (typically 8-bit, for values 0-255), allowing 256 distinct speed steps.  
  * ledcAttachPin(gpio\_pin, channel): Assigns the configured PWM channel to a specific GPIO pin (FAN\_PWM\_PIN).  
  * ledcWrite(channel, duty\_cycle\_raw): Sets the duty cycle for the channel. The duty\_cycle\_raw value is mapped from the desired 0-100% fan speed to the 0-255 range corresponding to the 8-bit resolution.  
* **Signal Level:** PC fans typically expect a 5V TTL PWM signal. Since the ESP32 outputs 3.3V logic, a **logic level shifter** is highly recommended between the ESP32's PWM output pin and the fan's control input pin to ensure reliable operation across all fan models and the full speed range.

## **6.2. I2C Communication**

* **Protocol:** I2C (Inter-Integrated Circuit) is a synchronous, multi-master, multi-slave, packet-switched, single-ended, serial communication bus. It requires only two wires:  
  * SDA (Serial Data)  
  * SCL (Serial Clock)  
* **Usage:** In this project, I2C is used for communication with:  
  * **BMP280 Temperature Sensor:** The ESP32 acts as the I2C master and requests temperature (and pressure, though not primarily used) data from the BMP280 slave device.  
  * **I2C LCD Module:** The ESP32 sends commands and character data to the LCD module (which typically has a PCF8574 I/O expander acting as the I2C slave) to display information.  
* **Implementation:** The ESP32's Wire.h library provides the functions for I2C communication (Wire.begin(), Wire.beginTransmission(), Wire.write(), Wire.endTransmission(), Wire.requestFrom(), Wire.read()).  
* **Pull-up Resistors:** External pull-up resistors (typically 4.7kΩ to 10kΩ, connected to 3.3V) are required on both SDA and SCL lines if not already present on the I2C slave modules.

## **6.3. Fan Tachometer (RPM Sensing)**

* **Signal:** The third pin on a 4-pin (and many 3-pin) fan connector is the Tachometer (or "Sense") output. It provides a pulsed signal, typically two pulses for each full revolution of the fan. This is often an open-collector or open-drain output.  
* **Hardware Interface:**  
  * The tachometer pin is connected to an ESP32 GPIO (FAN\_TACH\_PIN\_ACTUAL).  
  * A pull-up resistor (e.g., 10kΩ to 3.3V) is essential on this line because the fan's tachometer output usually just pulls the line LOW for each pulse.  
* **Software Implementation:**  
  * **Interrupt Service Routine (ISR):** An ISR (IRAM\_ATTR countPulse()) is attached to the tachometer GPIO pin, configured to trigger on a specific edge (e.g., FALLING).  
    * attachInterrupt(digitalPinToInterrupt(pin), ISR, mode)  
    * The ISR is kept extremely short and simple: it only increments a volatile unsigned long pulseCount.  
  * **RPM Calculation (in mainAppTask):**  
    1. Periodically (e.g., every second), interrupts are temporarily disabled to safely read the pulseCount and reset it to zero.  
    2. The time elapsed since the last RPM calculation (elapsedMillis) is determined.  
    3. RPM is calculated using the formula:  
       fanRpm \= (currentPulses / PULSES\_PER\_REVOLUTION) \* (60000.0f / elapsedMillis);  
       (60000.0f converts milliseconds to minutes).

## **6.4. WiFi and Networking**

* **WiFi Client Mode (STA):** The ESP32 connects to an existing WiFi network using WiFi.begin(ssid, password).  
* **ESPAsyncWebServer Library:**  
  * Used for creating an HTTP server that can serve static files and handle other HTTP requests asynchronously.  
  * server.on("/path", HTTP\_GET, handlerFunction) is used to define routes.  
  * In this project, it serves index.html, style.css, and script.js from SPIFFS.  
* **WebSockets (WebSocketsServer library by Markus Sattler):**  
  * Provides a persistent, full-duplex communication channel over a single TCP connection.  
  * webSocket.begin(): Starts the WebSocket server (typically on port 81).  
  * webSocket.onEvent(webSocketEvent): Registers a callback function (webSocketEvent) to handle WebSocket events like client connections, disconnections, and incoming text/binary messages.  
  * webSocket.broadcastTXT(jsonData): Sends a text message (JSON string in this case) to all connected WebSocket clients.  
  * **Data Format:** JSON (JavaScript Object Notation) is used for exchanging structured data between the ESP32 and the web UI. ArduinoJson library handles serialization and deserialization.

## **6.5. SPIFFS Filesystem Usage**

* **SPI Flash File System (SPIFFS):** A lightweight filesystem designed for microcontrollers with SPI flash memory.  
* **Usage:** Stores the web interface files (index.html, style.css, script.js).  
* **Initialization:** SPIFFS.begin(true) is called in setup(). The true argument formats SPIFFS if it's not already mounted or corrupted.  
* Serving Files: AsyncWebServer can serve files directly from SPIFFS:  
  request-\>send(SPIFFS, "/filename.ext", "content\_type");  
* **Uploading Files:** In PlatformIO, a data directory is created in the project root. Web files are placed here, and the "Upload Filesystem Image" task (pio run \-t uploadfs) builds and uploads the SPIFFS image.

## **6.6. NVS (Non-Volatile Storage) for Persistence**

* **Non-Volatile Storage:** The ESP32 provides a region of its flash memory for NVS, allowing data to be stored persistently across reboots.  
* **Preferences.h Library:** The Arduino ESP32 core provides this library to easily read and write key-value pairs to NVS.  
* **Usage:**  
  * preferences.begin("namespace", readOnly): Opens a specific namespace. readOnly is false for writing, true for reading.  
  * preferences.putType("key", value): Stores data (e.g., putString, putBool, putInt).  
  * preferences.getType("key", defaultValue): Retrieves data.  
  * preferences.isKey("key"): Checks if a key exists.  
  * preferences.end(): Closes the namespace.  
* **Stored Settings:**  
  * wifi-cfg namespace: ssid, password, wifiEn (WiFi enabled state).  
  * fan-curve namespace: numPoints, and individual curve points (tP0, pP0, etc.).

## **6.7. Conditional Debug Mode**

* **Activation:** A physical GPIO pin (DEBUG\_ENABLE\_PIN) is read during setup(). It's configured with an internal pull-down resistor. If this pin is HIGH (e.g., connected to 3.3V via a jumper or switch), debug mode is enabled.  
* **Serial Output:**  
  * If serialDebugEnabled is true, Serial.begin(115200) is called, and detailed log messages are printed throughout the code using Serial.print(), Serial.println(), and Serial.printf().  
  * If serialDebugEnabled is false, Serial.begin() is **not** called, effectively disabling all serial output and keeping the UART pins free/quiet.  
* **Serial Command Interface:** The handleSerialCommands() function is only called in mainAppTask if serialDebugEnabled is true.  
* **Debug LED:** LED\_DEBUG\_PIN (GPIO2) is turned ON if debug mode is active, providing a visual indicator.

## **6.8. Dual-Core Operation (FreeRTOS Tasks)**

The ESP32's dual cores are utilized via FreeRTOS tasks to improve performance and responsiveness:

* **Task Creation:** xTaskCreatePinnedToCore() is used in setup() to create and assign tasks to specific cores:  
  * networkTask \-\> Core 0 (PRO\_CPU)  
  * mainAppTask \-\> Core 1 (APP\_CPU)  
* **Task Responsibilities:**  
  * **networkTask (Core 0):** Handles all potentially blocking network operations (WiFi, web server, WebSockets). This prevents network latency or activity from delaying the main control loop. It only runs if WiFi is enabled.  
  * **mainAppTask (Core 1):** Executes the primary application logic: sensor reading, button processing, LCD updates, fan control calculations, and serial command handling.  
* **Inter-Task Communication:**  
  * **volatile variables:** Used for simple shared state flags and data that can be read/written by different tasks or ISRs (e.g., currentTemperature, fanRpm, isAutoMode).  
  * **needsImmediateBroadcast flag:** A volatile bool used by mainAppTask to signal networkTask when a state change requires an immediate WebSocket update to clients, rather than waiting for a periodic broadcast.  
* **Benefits:**  
  * Improved responsiveness of the LCD menu and button inputs.  
  * More stable network performance as it's not interrupted by sensor polling or display updates.  
  * Better overall system stability by isolating potentially blocking operations.

[Previous Chapter: Usage Guide](http://docs.google.com/05-usage-guide.md) | [Next Chapter: Troubleshooting](http://docs.google.com/07-troubleshooting.md)