# **Chapter 6: Technical Details & Protocols**

This chapter delves into the specific technologies, protocols, and implementation details used within the ESP32 PC Fan Controller project.

## **6.1. PWM Fan Control**

*(No changes needed)*

## **6.2. I2C Communication**

*(No changes needed)*

## **6.3. Fan Tachometer (RPM Sensing)**

*(No changes needed)*

## **6.4. WiFi and Networking (Web Server & WebSockets)**

*(Minor update to mention MQTT/Discovery config in Web UI)*

* **WiFi Client Mode (STA):** The ESP32 connects to an existing WiFi network using WiFi.begin(ssid, password).  
* **ESPAsyncWebServer Library:**  
  * Used for creating an HTTP server that can serve static files and handle other HTTP requests asynchronously.  
  * server.on("/path", HTTP\_GET, handlerFunction) is used to define routes.  
  * In this project, it serves index.html, style.css, and script.js from SPIFFS. The web UI allows configuration of MQTT and MQTT Discovery settings.  
* **WebSockets (WebSocketsServer library by Markus Sattler):**  
  * Provides a persistent, full-duplex communication channel over a single TCP connection for the web UI.  
  * webSocket.begin(): Starts the WebSocket server (typically on port 81).  
  * webSocket.onEvent(webSocketEvent): Registers a callback function (webSocketEvent) to handle WebSocket events.  
  * webSocket.broadcastTXT(jsonData): Sends a JSON string to all connected WebSocket clients. This JSON includes status and current configuration values (like MQTT settings, Discovery settings).  
  * **Data Format:** JSON (JavaScript Object Notation) is used for exchanging structured data between the ESP32 and the web UI. ArduinoJson library handles serialization and deserialization. Incoming WebSocket messages can trigger changes to MQTT and Discovery configurations, which are then saved to NVS.

## **6.5. MQTT Integration for Home Automation**

The controller supports MQTT for integration with home automation platforms, with a strong focus on Home Assistant MQTT Discovery.

* **Protocol:** MQTT (Message Queuing Telemetry Transport) is a lightweight publish-subscribe network protocol ideal for IoT devices.  
* **Library:** The PubSubClient library by Nick O'Leary is used for MQTT communication.  
* **Configuration:**  
  * MQTT client can be enabled/disabled.  
  * Broker settings (server address, port, username, password) and a base topic are configurable via LCD menu, Web UI, or serial commands.  
  * **MQTT Discovery:** Can be enabled/disabled, and the discovery prefix (default: "homeassistant") can be configured via LCD menu, Web UI, or serial commands.  
  * These settings are stored persistently in NVS.  
* **Connection:**  
  * The mqtt\_handler.cpp module manages the connection to the MQTT broker.  
  * It attempts to connect if MQTT is enabled and WiFi is available. Reconnection attempts are made periodically if the connection drops.  
  * **Last Will and Testament (LWT):** The client sets an LWT to publish an "offline" message to the availability topic if it disconnects unexpectedly.  
* **Home Assistant MQTT Discovery:**  
  * If enabled, upon successful MQTT connection, the ESP32 publishes configuration payloads to special topics under the mqttDiscoveryPrefix (e.g., homeassistant/sensor/esp32fanctrl\_MAC/temperature/config).  
  * These payloads describe the available entities (fan, sensors, configuration switches/texts/buttons) to Home Assistant, allowing for automatic setup.  
  * **Discovered Entities:**  
    * **Fan:** On/Off, Speed %, Presets (AUTO/MANUAL).  
    * **Sensors:** Temperature, Fan RPM, Manual Target Speed, WiFi RSSI, IP Address, current WiFi SSID, current MQTT broker details (server, port, user, base topic).  
    * **Binary Sensors:** Temp Sensor Status, WiFi Connection, Serial Debug, Reboot Needed, WiFi Enabled Setting Status, MQTT Client Setting Status.  
    * **Configuration Entities (via HA):** Fan Curve (JSON text), MQTT Discovery Enable (switch), MQTT Discovery Prefix (text), Reboot Device (button).  
  * The mqttDeviceId (derived from MAC address for uniqueness) and mqttDeviceName are used in discovery messages to group entities under a single device in Home Assistant.  
  * Firmware version (FIRMWARE\_VERSION) is also included in the device information.  
* **Topics and Payloads:**  
  * All device-specific topics are prefixed with the user-configured mqttBaseTopic.  
  * **Availability Topic:** (e.g., YOUR\_BASE\_TOPIC/online\_status) \- Publishes online/offline. Retained.  
  * **Status Topic (JSON):** (e.g., YOUR\_BASE\_TOPIC/status\_json) \- Publishes a comprehensive JSON object with all sensor readings and relevant state information for discovered entities. Retained.  
  * **Fan Curve Status Topic (JSON):** (e.g., YOUR\_BASE\_TOPIC/fancurve/status) \- Publishes the current fan curve as a JSON array. Retained.  
  * **Command Topics (examples):**  
    * YOUR\_BASE\_TOPIC/fan/set (payload: ON/OFF)  
    * YOUR\_BASE\_TOPIC/speed/set (payload: 0-100)  
    * YOUR\_BASE\_TOPIC/mode/set (payload: AUTO/MANUAL)  
    * YOUR\_BASE\_TOPIC/fancurve/set (payload: JSON array string)  
    * YOUR\_BASE\_TOPIC/discovery\_enabled/set (payload: ON/OFF)  
    * YOUR\_BASE\_TOPIC/discovery\_prefix/set (payload: e.g., homeassistant)  
    * YOUR\_BASE\_TOPIC/reboot/set (payload: REBOOT)  
* **Processing:**  
  * The mqttCallback function in mqtt\_handler.cpp processes incoming messages on subscribed command topics.  
  * The networkTask (Core 0\) is responsible for running mqttClient.loop() and triggering status publishes.  
* **JSON Handling:** The ArduinoJson library is used for creating and parsing JSON payloads.

## **6.6. SPIFFS Filesystem Usage**

*(No changes needed, but Web UI now also configures MQTT/Discovery)*

* **Usage:** Stores the web interface files (index.html, style.css, script.js). These files allow for viewing status and configuring all operational parameters including WiFi, MQTT, and MQTT Discovery settings.

## **6.7. NVS (Non-Volatile Storage) for Persistence**

* **Stored Settings:**  
  * wifi-cfg namespace: ssid, password, wifiEn (WiFi enabled state).  
  * mqtt-cfg namespace: mqttEn (MQTT enabled state), mqttSrv (server), mqttPrt (port), mqttUsr (user), mqttPwd (password), mqttTop (base topic).  
  * **mqtt-disc-cfg namespace (New): discEn (Discovery enabled state), discPfx (Discovery prefix).**  
  * fan-curve namespace: numPoints, and individual curve points (tP0, pP0, etc.).

## **6.8. Conditional Debug Mode**

*(No changes needed)*

## **6.9. Dual-Core Operation (FreeRTOS Tasks)**

*(No changes needed)*

[Previous Chapter: Usage Guide](http://docs.google.com/05-usage-guide.md) | [Next Chapter: Troubleshooting](http://docs.google.com/07-troubleshooting.md)