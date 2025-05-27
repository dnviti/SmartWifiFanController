# **Chapter 5: Usage Guide**

This chapter explains how to interact with and use the ESP32 PC Fan Controller after setup and installation.

## **5.1. First Boot and Initial State**

* **Power On:** After uploading the firmware and filesystem, power on your ESP32 fan controller.  
* **Debug Mode Check:** * If the DEBUG\_ENABLE\_PIN is connected to HIGH (3.3V) at boot:  
    * The onboard LED\_DEBUG\_PIN (GPIO2) will turn ON.  
    * Serial output will be active at 115200 baud. You can open the Serial Monitor in PlatformIO/Arduino IDE to see boot messages and interact with the serial command interface. The first message should indicate that Serial Debug is enabled.  
  * If the DEBUG\_ENABLE_PIN is LOW (or floating, due to INPUT\_PULLDOWN):  
    * The LED\_DEBUG\_PIN will be OFF.  
    * The Serial port will be completely inactive; no messages will be printed.  
* **Initial WiFi & MQTT State:** By default (and on first boot if NVS is empty), `isWiFiEnabled` and `isMqttEnabled` are set to false. The device will **not** attempt to connect to any WiFi network or MQTT broker.  
* **LCD Display:** The LCD will initialize and show "Fan Controller" and "Booting..." briefly. Once the mainAppTask starts, it will switch to the normal status display:  
  * Line 1: Mode (e.g., "AUTO"), and WiFi status ("WiFi OFF", "WiFi...", or IP address). May also show MQTT connection status (e.g., "M" or "m").  
  * Line 2: Temperature (or "N/A"), Fan Speed %, and RPM.  
* **Fan State:** The fan will initially be set to 0% speed.  
* **Settings:** Default fan curve will be active. Other settings (WiFi, MQTT) will be loaded from NVS if previously saved, otherwise defaults will apply.

## **5.2. LCD Menu Navigation**

The primary way to configure the device standalone is through the LCD menu system using the five physical buttons.

* **Button Functions:** * BTN\_MENU\_PIN: Press to enter the main menu. Press again while in the main menu (or use the "Back" option) to exit to the normal status display.  
  * BTN\_UP\_PIN: Navigate upwards in menu lists, or cycle through characters/values in edit modes.  
  * BTN\_DOWN\_PIN: Navigate downwards in menu lists, or cycle through characters/values in edit modes.  
  * BTN\_SELECT\_PIN: Confirm a menu selection, enter a submenu, or confirm a character/value in edit modes.  
  * BTN\_BACK\_PIN: Go back to the previous menu screen, or cancel an action. In input entry, it acts as a backspace or confirms current input and exits.
* **Main Menu Flow:** 1. **Normal Status Display**
     * Press BTN\_MENU\_PIN -> **Main Menu**
       * \>WiFi Settings
       * MQTT Settings
       * View Status (Exits menu)
  2. **WiFi Settings Menu:** (Selected from Main Menu)
     * \>WiFi: \[Enabled/Disabled\] (Toggle, sets rebootNeeded)
     * Scan Networks
     * SSID: \[current\_SSID\] (Selects to go to Scan)
     * Password Set (Enters WiFi Password Entry)
     * Connect WiFi (Attempts connection, sets rebootNeeded if successful)
     * DisconnectWiFi
     * Back to Main
  3. **WiFi Scan Menu (WIFI\_SCAN):**
     * Shows "Scanning...", then lists SSIDs. Select an SSID to set it.
  4. **Password Entry Menu (WIFI\_PASSWORD\_ENTRY):**
     * For WiFi password. Cycle chars with UP/DOWN, confirm char with SELECT. Max length or pressing BACK after some chars saves.
  5. **MQTT Settings Menu:** (Selected from Main Menu)
     * \>MQTT: \[Enabled/Disabled\] (Toggle, sets rebootNeeded)
     * Server: \[mqtt\_server\] (Enters MQTT Server Entry)
     * Port: \[mqtt\_port\] (Enters MQTT Port Entry)
     * User: \[mqtt\_user\] (Enters MQTT User Entry)
     * Password: \[\*\*\*\] (Enters MQTT Password Entry)
     * Base Topic: \[base\_topic\] (Enters MQTT Base Topic Entry)
     * Back to Main
  6. **MQTT Entry Screens (MQTT\_SERVER\_ENTRY, MQTT\_PORT\_ENTRY, etc.):**
     * Title shows what you're editing (e.g., "MQTT Server:").
     * Second line shows current input characters and the char being edited with UP/DOWN.
     * Press SELECT to confirm the current char and move to the next.
     * Press BACK to delete the last char. If no chars entered or after some input, pressing BACK will save the current buffer content to the respective MQTT setting in NVS and return to the MQTT Settings menu.
     * For Port, only numeric input is allowed. Max length applies to all fields.
  7. **Confirm Reboot Menu (CONFIRM\_REBOOT):**
     * Displays "Reboot needed\!"
     * Options: \>Yes (reboots) and No (cancels reboot, returns to Main Menu).

## **5.3. Serial Command Interface (Debug Mode)**

This interface provides more direct control and detailed feedback.

* **Activation:** DEBUG\_ENABLE\_PIN HIGH at boot. LED\_DEBUG\_PIN ON.
* **Connection:** Serial Monitor at 115200 baud.
* **Key Serial Commands (type `help` for full list):**
  * `status`: Displays current operational status including WiFi and MQTT state.
  * `set_mode auto` / `set_mode manual <percentage>`
  * `wifi_enable` / `wifi_disable` (sets rebootNeeded)
  * `set_ssid <ssid>` / `set_pass <password>`
  * `connect_wifi` / `disconnect_wifi` / `scan_wifi`
  * **`mqtt_enable` / `mqtt_disable`** (sets rebootNeeded)
  * **`set_mqtt_server <address>`**
  * **`set_mqtt_port <port>`**
  * **`set_mqtt_user <username>`**
  * **`set_mqtt_pass <password>`**
  * **`set_mqtt_topic <base_topic>`**
  * Fan curve commands: `view_curve`, `clear_staging_curve`, `stage_curve_point <t> <p>`, `apply_staged_curve`, `load_default_curve`.
  * `reboot`

## **5.4. Web Interface (If WiFi Enabled)**

(No direct MQTT configuration via Web UI in this iteration, but status reflects MQTT-driven changes if any.)
* Access via ESP32's IP address.
* Real-time status display (Temp, Fan Speed/RPM, Mode).
* Mode control (Auto/Manual buttons).
* Manual speed slider.
* Fan Curve Editor.

## **5.5. MQTT Usage for Home Automation**

Once MQTT is configured and enabled on the ESP32, and the ESP32 is connected to your WiFi and MQTT broker:

1.  **Availability:**
    * The ESP32 publishes its availability status (Last Will and Testament).
    * **Topic:** `YOUR_BASE_TOPIC/online_status`
    * **Payload:** `online` (when connected), `offline` (when disconnected). Retained message.

2.  **Status Monitoring:**
    * The ESP32 publishes its full status as a JSON payload.
    * **Topic:** `YOUR_BASE_TOPIC/status_json`
    * **Payload Example:**
      ```json
      {
        "temperature": 25.5,
        "tempSensorFound": true,
        "fanSpeedPercent": 50,
        "fanRpm": 1200,
        "mode": "AUTO",
        "manualSetSpeed": 60,
        "ipAddress": "192.168.1.123",
        "wifiRSSI": -55
      }
      ```
    * This message is retained and published periodically or on significant state changes.

3.  **Controlling the Fan Controller via MQTT:**
    * **Set Mode:**
        * **Topic:** `YOUR_BASE_TOPIC/mode/set`
        * **Payload:** Send `AUTO` or `MANUAL` as a string.
    * **Set Manual Speed:** (Only effective if mode is MANUAL)
        * **Topic:** `YOUR_BASE_TOPIC/speed/set`
        * **Payload:** Send a number from `0` to `100` as a string (e.g., `"75"`).

**Example using an MQTT client (like MQTT Explorer or mosquitto\_pub/sub):**

* **To set mode to MANUAL:**
    `mosquitto_pub -h YOUR_BROKER_IP -t "YOUR_BASE_TOPIC/mode/set" -m "MANUAL"`
* **To set manual speed to 65% (assuming it's in MANUAL mode):**
    `mosquitto_pub -h YOUR_BROKER_IP -t "YOUR_BASE_TOPIC/speed/set" -m "65"`
* **To listen to all status messages:**
    `mosquitto_sub -h YOUR_BROKER_IP -t "YOUR_BASE_TOPIC/#" -v`

Replace `YOUR_BASE_TOPIC` and `YOUR_BROKER_IP` with your actual configured values.

[Previous Chapter: Setup and Installation](04-setup-and-installation.md) | [Next Chapter: Technical Details & Protocols](06-technical-details.md)
