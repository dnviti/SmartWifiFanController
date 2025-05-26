# **Chapter 5: Usage Guide**

This chapter explains how to interact with and use the ESP32 PC Fan Controller after setup and installation.

## **5.1. First Boot and Initial State**

* **Power On:** After uploading the firmware and filesystem, power on your ESP32 fan controller.  
* **Debug Mode Check:**  
  * If the DEBUG\_ENABLE\_PIN is connected to HIGH (3.3V) at boot:  
    * The onboard LED\_DEBUG\_PIN (GPIO2) will turn ON.  
    * Serial output will be active at 115200 baud. You can open the Serial Monitor in PlatformIO/Arduino IDE to see boot messages and interact with the serial command interface. The first message should indicate that Serial Debug is enabled.  
  * If the DEBUG\_ENABLE\_PIN is LOW (or floating, due to INPUT\_PULLDOWN):  
    * The LED\_DEBUG\_PIN will be OFF.  
    * The Serial port will be completely inactive; no messages will be printed.  
* **Initial WiFi State:** By default (and on first boot if NVS is empty), isWiFiEnabled is set to false. The device will **not** attempt to connect to any WiFi network.  
* **LCD Display:** The LCD will initialize and show "Fan Controller" and "Booting..." briefly. Once the mainAppTask starts, it will switch to the normal status display:  
  * Line 1: Mode (e.g., "AUTO"), and "WiFi OFF" or "WiFi..." if attempting connection.  
  * Line 2: Temperature (or "N/A"), Fan Speed %, and RPM.  
* **Fan State:** The fan will initially be set to 0% speed.  
* **Settings:** Default fan curve will be active. Other settings will be loaded from NVS if previously saved, otherwise defaults will apply.

## **5.2. LCD Menu Navigation**

The primary way to configure the device standalone is through the LCD menu system using the five physical buttons.

* **Button Functions:**  
  * BTN\_MENU\_PIN: Press to enter the main menu. Press again while in the main menu (or use the "Back" option) to exit to the normal status display.  
  * BTN\_UP\_PIN: Navigate upwards in menu lists, or cycle through characters/values in edit modes.  
  * BTN\_DOWN\_PIN: Navigate downwards in menu lists, or cycle through characters/values in edit modes.  
  * BTN\_SELECT\_PIN: Confirm a menu selection, enter a submenu, or confirm a character/value in edit modes.  
  * BTN\_BACK\_PIN: Go back to the previous menu screen, or cancel an action. In password entry, it acts as a backspace.  
* **Menu Flow:**  
  1. **Normal Status Display:** Shows current operational data.  
     * Press BTN\_MENU\_PIN to enter the **Main Menu**.  
  2. **Main Menu:**  
     * \>WiFi Settings: (Selected by default) Press BTN\_SELECT\_PIN to enter.  
     * View Status: Press BTN\_SELECT\_PIN to exit the menu and return to the normal status display.  
     * *Navigate with UP/DOWN, select with SELECT.*  
  3. **WiFi Settings Menu:**  
     * \>WiFi: \[Enabled/Disabled\]: (Selected by default) Press BTN\_SELECT\_PIN to toggle the WiFi state between "Enabled" and "Disabled".  
       * This change is saved to NVS immediately.  
       * rebootNeeded flag is set. The system will guide you to the "Confirm Reboot" screen. A reboot is necessary for the networkTask to start or stop correctly.  
     * Scan Networks: Press BTN\_SELECT\_PIN to initiate a WiFi scan.  
     * SSID: \[current\_SSID\_prefix\]: Shows the currently configured SSID (first 8 chars). Press BTN\_SELECT\_PIN to go to the WiFi Scan screen to select a new SSID.  
     * Password Set: Press BTN\_SELECT\_PIN to enter the password input screen for the current SSID.  
     * Connect WiFi: Press BTN\_SELECT\_PIN to attempt connection to the configured SSID/Password. If successful and WiFi is enabled, rebootNeeded is set, and you'll be guided to confirm reboot (to ensure web services start).  
     * DisconnectWiFi: Press BTN\_SELECT\_PIN to disconnect from the current WiFi network (does not disable WiFi).  
     * Back to Main: Press BTN\_SELECT\_PIN to return to the Main Menu.  
     * *Navigate with UP/DOWN, select with SELECT, go back with BACK.*  
  4. **WiFi Scan Menu (WIFI\_SCAN):**  
     * Displays "Scanning WiFi..." initially.  
     * Once complete, lists available SSIDs (up to 10).  
     * Use BTN\_UP\_PIN/BTN\_DOWN\_PIN to scroll through the list.  
     * Press BTN\_SELECT\_PIN on a desired SSID to select it. The selected SSID is copied to current\_ssid (but not saved to NVS until "Connect WiFi" is successful or password is set). You are then returned to the WiFi Settings menu, usually highlighting the "Password Set" option.  
     * Press BTN\_BACK\_PIN to return to WiFi Settings Menu.  
  5. **Password Entry Menu (WIFI\_PASSWORD\_ENTRY):**  
     * Line 1: "Enter Pass Char:"  
     * Line 2: Shows entered characters as \* and the current character being edited.  
     * BTN\_UP\_PIN/BTN\_DOWN\_PIN: Cycle through characters (space, 'a'-'z', 'A'-'Z', '0'-'9', symbols).  
     * BTN\_SELECT\_PIN: Confirms the current character and moves to the next position. If the buffer is full, this acts as "Confirm Password".  
     * BTN\_BACK\_PIN: Deletes the last entered character (backspace). If no characters entered, goes back to WiFi Settings.  
     * Once the password is fully entered (or buffer full and SELECT pressed), it's copied to current\_password and saveWiFiConfig() is called. You are returned to WiFi Settings, usually highlighting "Connect WiFi".  
  6. **Confirm Reboot Menu (CONFIRM\_REBOOT):**  
     * Displays "Reboot needed\!"  
     * Options: \>Yes and No.  
     * BTN\_SELECT\_PIN on "Yes" will restart the ESP32.  
     * BTN\_SELECT\_PIN on "No" will clear the rebootNeeded flag and return to the Main Menu. The setting change will only fully apply after the next manual reboot.  
     * BTN\_BACK\_PIN also cancels the reboot and returns to Main Menu.

## **5.3. Serial Command Interface (Debug Mode)**

This interface provides more direct control and detailed feedback, useful for debugging and advanced configuration.

* **Activation:** Ensure the DEBUG\_ENABLE\_PIN is HIGH when the ESP32 boots. The LED\_DEBUG\_PIN will illuminate.  
* **Connection:** Connect to the ESP32 via a USB cable. Open the Serial Monitor in your IDE (PlatformIO or Arduino IDE) with the baud rate set to **115200**.  
* **Usage:**  
  * Upon successful connection, you should see boot messages and an indication that the serial debug interface is active.  
  * Type help and press Enter to see a list of available commands.  
  * Commands are case-insensitive.  
  * Arguments are typically space-separated.  
* **Key Serial Commands (refer to handleSerialCommands() in input\_handler.cpp for the full list and behavior):**  
  * status: Displays current operational status (mode, fan speed, temp, RPM, WiFi info).  
  * set\_mode auto: Switches to automatic fan control.  
  * set\_mode manual \<percentage\>: Switches to manual mode and sets fan speed (e.g., set\_mode manual 75).  
  * wifi\_enable / wifi\_disable: Toggles the WiFi operational state. Saves to NVS and sets rebootNeeded.  
  * set\_ssid \<your\_network\_ssid\>: Sets the WiFi SSID. Saves to NVS.  
  * set\_pass \<your\_network\_password\>: Sets the WiFi password. Saves to NVS.  
  * connect\_wifi: Attempts to connect to the configured WiFi network. Sets rebootNeeded if successful.  
  * disconnect\_wifi: Disconnects from the current WiFi network.  
  * scan\_wifi: Performs a WiFi scan and lists results in the Serial Monitor.  
  * view\_curve: Shows the current active fan curve points.  
  * clear\_staging\_curve: Clears the temporary curve being built via serial.  
  * stage\_curve\_point \<temperature\> \<pwm\_percentage\>: Adds a point to the temporary staging curve (e.g., stage\_curve\_point 30 20).  
  * apply\_staged\_curve: Validates the staged curve, makes it active, and saves it to NVS.  
  * load\_default\_curve: Loads the hardcoded default fan curve and saves it to NVS.  
  * reboot: Restarts the ESP32.

## **5.4. Web Interface (If WiFi Enabled)**

When WiFi is enabled and the ESP32 is connected to your network:

1. **Access:**  
   * Find the ESP32's IP address (displayed on the LCD during normal operation if WiFi is connected, or via the status serial command).  
   * Open a web browser on a device connected to the *same* WiFi network.  
   * Navigate to http://\<ESP32\_IP\_ADDRESS\>.  
2. **Features:**  
   * **Loading Screen:** A loading animation is shown briefly while the page establishes a WebSocket connection and receives initial data.  
   * **Debug Mode Notice:** If serial debug mode is active on the ESP32, a notice will be displayed at the top of the web page.  
   * **Status Display:** Real-time updates for:  
     * Temperature (or "N/A" if sensor not found).  
     * Current Fan Speed (%).  
     * Current Fan RPM.  
     * Current Mode (AUTO/MANUAL).  
     * A notice if in AUTO mode without a temperature sensor (indicating fixed speed).  
   * **Mode Control:**  
     * "Auto Mode" button: Switches the fan controller to automatic mode.  
     * "Manual Mode" button: Switches to manual mode.  
   * **Manual Speed Slider:**  
     * Visible only when in "MANUAL" mode.  
     * Allows setting fan speed from 0% to 100%. Changes are sent to the ESP32 as you adjust the slider (on onchange event, typically when released).  
   * **Fan Curve Editor:**  
     * Visible only when in "AUTO" mode **and** the temperature sensor is detected (tempSensorFound is true).  
     * Displays current curve points with input fields for temperature and PWM percentage.  
     * "+ Add Point" button: Adds a new editable point to the curve (up to MAX\_CURVE\_POINTS).  
     * "X" button next to each point: Removes that point from the UI.  
     * "Save Curve to Device" button: Sends the edited curve from the UI to the ESP32. The ESP32 validates it, applies it if valid, and saves it to NVS.  
   * **Real-time Updates:** All data displays are updated in real-time via WebSockets as the ESP32 broadcasts changes.

This multi-faceted approach to control and configuration provides flexibility for various user needs and scenarios.

[Previous Chapter: Setup and Installation](04-setup-and-installation.md) | [Next Chapter: Technical Details & Protocols](06-technical-details.md)