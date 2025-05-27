# **Chapter 6: Technical Details & Protocols**

This chapter delves into the specific technologies, protocols, and implementation details used within the ESP32 PC Fan Controller project.

## **6.1. PWM Fan Control** (Content as before)

## **6.2. I2C Communication**
(Content as before)

## **6.3. Fan Tachometer (RPM Sensing)**
(Content as before)

## **6.4. WiFi and Networking (Web Server & WebSockets)**

* **WiFi Client Mode (STA):** The ESP32 connects to an existing WiFi network using `WiFi.begin(ssid, password)`.  
* **ESPAsyncWebServer Library:** * Used for creating an HTTP server that can serve static files and handle other HTTP requests asynchronously.  
  * `server.on("/path", HTTP_GET, handlerFunction)` is used to define routes.  
  * In this project, it serves `index.html`, `style.css`, and `script.js` from SPIFFS.  
* **WebSockets (WebSocketsServer library by Markus Sattler):** * Provides a persistent, full-duplex communication channel over a single TCP connection for the web UI.  
  * `webSocket.begin()`: Starts the WebSocket server (typically on port 81).  
  * `webSocket.onEvent(webSocketEvent)`: Registers a callback function (`webSocketEvent`) to handle WebSocket events.  
  * `webSocket.broadcastTXT(jsonData)`: Sends a JSON string to all connected WebSocket clients.  
  * **Data Format:** JSON (JavaScript Object Notation) is used for exchanging structured data between the ESP32 and the web UI. ArduinoJson library handles serialization and deserialization.

## **6.5. MQTT Integration for Home Automation**

The controller supports MQTT for integration with home automation platforms.

* **Protocol:** MQTT (Message Queuing Telemetry Transport) is a lightweight publish-subscribe network protocol ideal for IoT devices.
* **Library:** The `PubSubClient` library by Nick O'Leary is used for MQTT communication.
* **Configuration:**
    * MQTT can be enabled/disabled.
    * Broker settings (server address, port, username, password) and a base topic are configurable via the LCD menu or serial commands.
    * These settings are stored persistently in NVS under the "mqtt-cfg" namespace.
* **Connection:**
    * The `mqtt_handler.cpp` module manages the connection to the MQTT broker.
    * It attempts to connect if MQTT is enabled and WiFi is available. Reconnection attempts are made periodically if the connection drops.
    * **Last Will and Testament (LWT):** The client sets an LWT to publish an "offline" message to the availability topic if it disconnects unexpectedly.
* **Topics and Payloads:**
    * All topics are prefixed with the user-configured `mqttBaseTopic`.
    * **Availability Topic:** (e.g., `YOUR_BASE_TOPIC/online_status`)
        * Publishes `online` when connected, `offline` as LWT. Retained message.
    * **Status Topic (JSON):** (e.g., `YOUR_BASE_TOPIC/status_json`)
        * Publishes a JSON object containing temperature, sensor status, fan speed percentage, fan RPM, current mode, manual set speed, IP address, and WiFi RSSI.
        * Published periodically and on significant state changes. Retained message.
        * Example payload: `{"temperature":25.5,"tempSensorFound":true,"fanSpeedPercent":50,"fanRpm":1200,"mode":"AUTO", ...}`
    * **Command Topics:**
        * **Mode Set:** (e.g., `YOUR_BASE_TOPIC/mode/set`)
            * Payload: `AUTO` or `MANUAL` (case-insensitive string).
        * **Manual Speed Set:** (e.g., `YOUR_BASE_TOPIC/speed/set`)
            * Payload: Integer string from `0` to `100`. Only effective in MANUAL mode.
* **Processing:**
    * The `mqttCallback` function in `mqtt_handler.cpp` processes incoming messages on subscribed command topics.
    * The `networkTask` (Core 0) is responsible for running `mqttClient.loop()` and triggering status publishes.
* **JSON Handling:** The `ArduinoJson` library is used for creating the JSON status payload.

## **6.6. SPIFFS Filesystem Usage**
(Content as before, renumbered)

## **6.7. NVS (Non-Volatile Storage) for Persistence**
(Content as before, renumbered. Add MQTT to stored settings)
* **Stored Settings:** * `wifi-cfg` namespace: ssid, password, wifiEn (WiFi enabled state).  
  * **`mqtt-cfg` namespace: mqttEn (MQTT enabled state), mqttSrv (server), mqttPrt (port), mqttUsr (user), mqttPwd (password), mqttTop (base topic).**
  * `fan-curve` namespace: numPoints, and individual curve points (tP0, pP0, etc.).

## **6.8. Conditional Debug Mode**
(Content as before, renumbered)

## **6.9. Dual-Core Operation (FreeRTOS Tasks)**
(Content as before, renumbered. Update networkTask responsibilities)
* **Task Responsibilities:** * **networkTask (Core 0):** Handles all potentially blocking network operations (WiFi, web server, WebSockets, **MQTT client**). This prevents network latency or activity from delaying the main control loop. It only runs if WiFi is enabled. MQTT client runs if both WiFi and MQTT are enabled.
  * **mainAppTask (Core 1):** Executes the primary application logic.  

[Previous Chapter: Usage Guide](05-usage-guide.md) | [Next Chapter: Troubleshooting](07-troubleshooting.md)
