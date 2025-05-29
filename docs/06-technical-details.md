# **Chapter 6: Technical Details & Protocols**

This chapter delves into the specific technologies, protocols, and implementation details used within the ESP32 PC Fan Controller project.

## **6.1. PWM Fan Control**

*(No changes)*

## **6.2. I2C Communication**

*(No changes)*

## **6.3. Fan Tachometer (RPM Sensing)**

*(No changes)*

## **6.4. WiFi and Networking (Web Server & WebSockets)**

* **WiFi Client Mode (STA):** The ESP32 connects to an existing WiFi network. **The hostname is dynamically set to fancontrol-\[macaddress\] for easier network identification.**  
* **ESPAsyncWebServer Library:**  
  * Used for creating an HTTP server.  
  * Serves static files (index.html, style.css, script.js) from SPIFFS.  
  * **Hosts the ElegantOTA endpoint (/update) for manual OTA updates.**  
* **WebSockets (WebSocketsServer library):**  
  * Provides persistent, full-duplex communication for the web UI.  
  * **Transmits OTA status messages and receives OTA trigger commands.**  
* **Data Format:** JSON (ArduinoJson library) for WebSocket data.

## **6.5. MQTT Integration for Home Automation**

*(Status JSON now includes OTA info)*

* **Protocol:** MQTT.  
* **Library:** PubSubClient.  
* **Configuration:** (As before).  
* **Home Assistant MQTT Discovery:** (As before).  
* **Topics and Payloads:**  
  * **Status Topic (JSON):** (e.g., YOUR\_BASE\_TOPIC/status\_json) \- Publishes a comprehensive JSON object. **Now includes firmwareVersion, otaInProgress, and otaStatusMessage.**  
  * Other topics (as before).  
* **Processing:** (As before).

## **6.6. SPIFFS Filesystem Usage**

* **Web Interface Files:** Stores index.html, style.css, script.js.  
* **Root CA Certificate for OTA:** Stores the Root CA certificate (e.g., /github\_api\_ca.pem) used for secure HTTPS connections to GitHub for OTA updates. This file is loaded into memory at boot. The GitHub Actions pipeline includes the latest CA in the spiffs.bin of releases.

## **6.7. NVS (Non-Volatile Storage) for Persistence**

*(No changes)*

## **6.8. Conditional Debug Mode**

*(No changes)*

## **6.9. Dual-Core Operation (FreeRTOS Tasks)**

* **Core 0 (networkTask):** Handles WiFi, ESPAsyncWebServer (including ElegantOTA), WebSockets, MQTT client.  
* **Core 1 (mainAppTask):** Handles main application logic, sensors, LCD, buttons, serial commands. **The GitHub OTA check and update process (HTTPClient, HTTPUpdate) are initiated from this core's context when triggered, which can be blocking during the download/flash phases.**

## **6.10. Over-the-Air (OTA) Updates**

The controller implements two OTA update mechanisms:

1. **ElegantOTA (Manual Web Upload):**  
   * **Library:** ayushsharma82/ElegantOTA.  
   * **Interface:** Provides a web page at the /update endpoint of the ESP32's IP address.  
   * **Functionality:** Allows users to manually upload pre-compiled firmware.bin or spiffs.bin files directly to the device.  
   * **Initialization:** ElegantOTA.begin(\&server); is called in networkTask after the web server starts. ElegantOTA.loop() is also called in networkTask.  
2. **GitHub Release Updater (Automated Check & Install):**  
   * **Module:** Custom logic in ota\_updater.h and ota\_updater.cpp.  
   * **Trigger:** Can be initiated via LCD menu, Web UI, or a serial command (ota\_update).  
   * **Process:**  
     1. **Fetch Latest Release Info:**  
        * Makes an HTTPS GET request to the GitHub API (GITHUB\_API\_LATEST\_RELEASE\_URL).  
        * Uses WiFiClientSecure configured with a Root CA certificate loaded from SPIFFS (GITHUB\_API\_ROOT\_CA\_STRING loaded from GITHUB\_ROOT\_CA\_FILENAME).  
        * Parses the JSON response (using ArduinoJson) to find the tag\_name (version) and asset download URLs for firmware (firmware\_PIO\_BUILD\_ENV\_NAME\_vX.Y.Z.bin) and SPIFFS (spiffs\_PIO\_BUILD\_ENV\_NAME\_vX.Y.Z.bin). PIO\_BUILD\_ENV\_NAME must match the environment name used in the GitHub Actions release workflow.  
     2. **Version Comparison:**  
        * Compares the fetched tag\_name with the FIRMWARE\_VERSION define.  
     3. **Update Execution (if newer version found):**  
        * Uses the HTTPUpdate library.  
        * **Firmware Update:** Calls httpUpdate.update(client, firmwareURL).  
        * **SPIFFS Update:** Calls httpUpdate.updateSpiffs(client, spiffsURL).  
        * Both use WiFiClientSecure configured with the loaded Root CA from SPIFFS.  
        * The device reboots automatically after a successful update by HTTPUpdate.  
   * **Root CA Management:** The Root CA certificate is stored on SPIFFS. The GitHub Actions workflow is designed to download the latest relevant CA during its build process and include it in the spiffs.bin of the release assets. This allows the CA to be updated via a SPIFFS OTA update.  
   * **Security:** Relies on HTTPS for communication with GitHub. The validity of the connection depends on the correctness and currency of the Root CA certificate stored on SPIFFS.

[Previous Chapter: Usage Guide](05-usage-guide.md) | [Next Chapter: Troubleshooting](07-troubleshooting.md)