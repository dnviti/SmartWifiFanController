# **Chapter 5: Usage Guide**

This chapter explains how to interact with and use the ESP32 PC Fan Controller after setup and installation, including the new MQTT Discovery features.

## **5.1. First Boot and Initial State**

* **Power On:** After uploading the firmware and filesystem, power on your ESP32 fan controller.  
* **Debug Mode Check:**  
  * If the DEBUG\_ENABLE\_PIN is connected to HIGH (3.3V) at boot:  
    * The onboard LED\_DEBUG\_PIN (GPIO2) will turn ON.  
    * Serial output will be active at 115200 baud. You can open the Serial Monitor in PlatformIO/Arduino IDE to see boot messages and interact with the serial command interface. The first message should indicate that Serial Debug is enabled and show the firmware version.  
  * If the DEBUG\_ENABLE\_PIN is LOW (or floating, due to INPUT\_PULLDOWN):  
    * The LED\_DEBUG\_PIN will be OFF.  
    * The Serial port will be completely inactive; no messages will be printed.  
* **Initial WiFi & MQTT State:** By default (and on first boot if NVS is empty), isWiFiEnabled and isMqttEnabled are set to false. isMqttDiscoveryEnabled defaults to true, but will only take effect if MQTT itself is enabled. The device will **not** attempt to connect to any WiFi network or MQTT broker initially.  
* **LCD Display:** The LCD will initialize and show "Fan Controller" and "Booting..." briefly. Once the mainAppTask starts, it will switch to the normal status display:  
  * Line 1: Mode (e.g., "AUTO"), and WiFi status ("WiFi OFF", "WiFi...", or IP address). May also show MQTT connection status (e.g., "M" for connected, "m" for attempting/enabled) and "D" if Discovery is active.  
  * Line 2: Temperature (or "N/A"), Fan Speed %, and RPM.  
* **Fan State:** The fan will initially be set to 0% speed.  
* **Settings:** Default fan curve will be active. Other settings (WiFi, MQTT, Discovery) will be loaded from NVS if previously saved, otherwise defaults will apply.

## **5.2. LCD Menu Navigation**

The primary way to configure the device standalone is through the LCD menu system using the five physical buttons.

* **Button Functions:** (Menu, Up, Down, Select, Back)  
* **Main Menu Flow:**  
  1. **Normal Status Display**  
     * Press BTN\_MENU\_PIN \-\> **Main Menu**  
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
     * **Discovery Cfg** (Enters MQTT Discovery Settings Menu)  
     * Back to Main  
  6. **MQTT Entry Screens (MQTT\_SERVER\_ENTRY, MQTT\_PORT\_ENTRY, etc.):**  
     * For MQTT server, port, user, password, base topic.  
  7. **MQTT Discovery Settings Menu (MQTT\_DISCOVERY\_SETTINGS):** (New)  
     * \>Discovery: \[Enabled/Disabled\] (Toggle, sets rebootNeeded)  
     * Prefix: \[discovery\_prefix\] (Enters MQTT Discovery Prefix Entry)  
     * Back (to MQTT Settings)  
  8. **MQTT Discovery Prefix Entry Menu (MQTT\_DISCOVERY\_PREFIX\_ENTRY):** (New)  
     * For the discovery prefix (e.g., "homeassistant").  
  9. **Confirm Reboot Menu (CONFIRM\_REBOOT):**  
     * Displays "Reboot needed\!"  
     * Options: \>Yes (reboots) and No (cancels reboot, returns to Main Menu).

## **5.3. Serial Command Interface (Debug Mode)**

* **Activation:** DEBUG\_ENABLE\_PIN HIGH at boot. LED\_DEBUG\_PIN ON.  
* **Connection:** Serial Monitor at 115200 baud.  
* **Key Serial Commands (type help for full list):**  
  * status: Displays current operational status including WiFi, MQTT state, and MQTT Discovery settings.  
  * set\_mode auto / set\_mode manual \<percentage\>  
  * wifi\_enable / wifi\_disable (sets rebootNeeded)  
  * set\_ssid \<ssid\> / set\_pass \<password\>  
  * connect\_wifi / disconnect\_wifi / scan\_wifi  
  * mqtt\_enable / mqtt\_disable (sets rebootNeeded)  
  * set\_mqtt\_server \<address\>  
  * set\_mqtt\_port \<port\>  
  * set\_mqtt\_user \<username\>  
  * set\_mqtt\_pass \<password\>  
  * set\_mqtt\_topic \<base\_topic\>  
  * **mqtt\_discovery\_enable / mqtt\_discovery\_disable** (sets rebootNeeded)  
  * **set\_mqtt\_discovery\_prefix \<prefix\>** (sets rebootNeeded)  
  * Fan curve commands: view\_curve, clear\_staging\_curve, stage\_curve\_point \<t\> \<p\>, apply\_staged\_curve, load\_default\_curve.  
  * reboot

## **5.4. Web Interface (If WiFi Enabled)**

* Access via ESP32's IP address.  
* Real-time status display (Temp, Fan Speed/RPM, Mode).  
* Mode control (Auto/Manual buttons).  
* Manual speed slider.  
* Fan Curve Editor.  
* **MQTT Configuration Section:**  
  * Enable/disable MQTT client.  
  * Configure Broker Server, Port, User, (Password input), Base Topic.  
* **MQTT Discovery Configuration Section (New):**  
  * Enable/disable Home Assistant MQTT Discovery.  
  * Configure Discovery Prefix (e.g., "homeassistant").  
* Save buttons for MQTT and Discovery settings will indicate if a reboot is needed.

## **5.5. MQTT Usage for Home Automation (with Home Assistant Discovery)**

Once MQTT is configured and enabled, and the ESP32 is connected to your WiFi and MQTT broker:

1. **Home Assistant Setup:**  
   * Ensure your Home Assistant instance has the MQTT integration installed and configured to connect to your broker.  
   * MQTT Discovery should be enabled in Home Assistant's MQTT integration (this is usually the default). The discovery prefix in HA (default: homeassistant) must match the prefix configured on the ESP32.  
2. **ESP32 Configuration for Discovery:**  
   * On the ESP32, enable "MQTT Enabled" and "Discovery Enabled" via the LCD menu, Web UI, or serial.  
   * Ensure the "Discovery Prefix" on the ESP32 matches Home Assistant's (default homeassistant).  
   * A reboot of the ESP32 might be necessary after these settings are applied for discovery messages to be published.  
3. **Automatic Entity Creation:**  
   * Home Assistant will automatically discover and create entities for the fan controller. These typically include:  
     * **Fan Entity:**  
       * On/Off control.  
       * Speed percentage control.  
       * Preset modes: "AUTO", "MANUAL".  
     * **Sensors:**  
       * Temperature (°C)  
       * Fan RPM (RPM)  
       * Manual Mode Target Speed (%)  
       * WiFi Signal Strength (dBm)  
       * IP Address  
       * Current WiFi SSID (diagnostic)  
       * MQTT Broker Server, Port, User, Base Topic (diagnostic)  
     * **Binary Sensors (Diagnostic):**  
       * Temperature Sensor Status (Connected/Not Connected)  
       * WiFi Connection Status (Connected/Disconnected)  
       * Serial Debug Status (Enabled/Disabled)  
       * Reboot Needed (Yes/No)  
       * WiFi Enabled Setting (On/Off \- reflects the ESP32's setting)  
       * MQTT Client Setting (On/Off \- reflects the ESP32's setting)  
     * **Configuration Entities:**  
       * MQTT Discovery Setting (Switch to enable/disable discovery on the ESP32)  
       * MQTT Discovery Prefix (Text input to change the prefix on the ESP32)  
       * Fan Curve (Text input, expects JSON format for the curve points)  
       * Reboot Device (Button to trigger a reboot of the ESP32)  
4. **Interacting via Home Assistant:**  
   * You can now monitor and control the fan (mode, speed), view sensor data, and adjust some configurations directly from your Home Assistant dashboard.  
   * Changing the Fan Curve via the HA text entity requires inputting a valid JSON array like: \[{"temp":25,"pwmPercent":0},{"temp":35,"pwmPercent":20}, ...\]  
5. **Key MQTT Topics (examples, YOUR\_BASE\_TOPIC is configurable):**  
   * **Availability:** YOUR\_BASE\_TOPIC/online\_status (payload: online/offline)  
   * **Status (JSON):** YOUR\_BASE\_TOPIC/status\_json (publishes all states for HA entities)  
   * **Fan Curve Status (JSON):** YOUR\_BASE\_TOPIC/fancurve/status  
   * **Commands (examples):**  
     * Fan control: YOUR\_BASE\_TOPIC/fan/set (payload: ON/OFF), YOUR\_BASE\_TOPIC/speed/set (payload: 0-100), YOUR\_BASE\_TOPIC/mode/set (payload: AUTO/MANUAL)  
     * Fan curve: YOUR\_BASE\_TOPIC/fancurve/set (payload: JSON array)  
     * Discovery enable: YOUR\_BASE\_TOPIC/discovery\_enabled/set (payload: ON/OFF)  
     * Discovery prefix: YOUR\_BASE\_TOPIC/discovery\_prefix/set (payload: e.g., homeassistant)  
     * Reboot: YOUR\_BASE\_TOPIC/reboot/set (payload: REBOOT)

[Previous Chapter: Setup and Installation](http://docs.google.com/04-setup-and-installation.md) | [Next Chapter: Technical Details & Protocols](http://docs.google.com/06-technical-details.md)