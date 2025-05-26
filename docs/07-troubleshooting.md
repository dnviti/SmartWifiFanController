# **Chapter 7: Troubleshooting**

This chapter provides solutions to common issues you might encounter while building and using the ESP32 PC Fan Controller.

* **No LCD Output / Gibberish on LCD:**  
  * **Check Wiring:** Verify SDA, SCL, VCC, and GND connections between the ESP32 and the I2C LCD module.  
  * **I2C Address:** The default I2C address in the code is 0x27. Your module might use a different address (e.g., 0x3F). Use an I2C scanner sketch to find the correct address and update LiquidCrystal\_I2C lcd(0x27, 16, 2); accordingly.  
  * **Pull-up Resistors:** Ensure 4.7kΩ to 10kΩ pull-up resistors are present on both the SDA and SCL lines, connected to 3.3V. Some modules have them built-in.  
  * **Power:** Confirm the LCD module is receiving the correct voltage (usually 5V for the backpack, but the I2C lines are 3.3V logic with the ESP32).  
  * **Contrast:** Some LCDs have a contrast potentiometer on the backpack. Try adjusting it.  
* **Fan Not Spinning or Not Responding to PWM:**  
  * **12V Power:** Ensure the fan is receiving a stable 12V supply directly.  
  * **PWM Signal Wiring:** Double-check the connection from the ESP32's FAN\_PWM\_PIN to the fan's control wire (Pin 4).  
  * **Logic Level Shifter (CRITICAL):** Most PC fans expect a 5V PWM signal. The ESP32 outputs 3.3V. **Using a 3.3V to 5V logic level shifter is highly recommended and often necessary for proper operation.** Without it, fans might not spin, spin at a fixed speed, or not respond to the full PWM range.  
  * **PWM Frequency:** The code sets PWM\_FREQ to 25000 (25kHz), which is standard. Ensure this is correctly configured.  
  * **Fan Type:** Confirm you are using a 4-pin PWM fan. 3-pin DC fans require voltage control, not PWM on a dedicated line, and would need different circuitry (e.g., a MOSFET to switch the main power or an L298N).  
  * **Ground Connection:** Ensure a common ground between the ESP32, the 12V power supply for the fan, and the fan itself.  
* **No RPM Reading or Incorrect RPM:**  
  * **Tachometer Wiring:** Verify the fan's tachometer (sense) wire (Pin 3\) is connected to the ESP32's FAN\_TACH\_PIN\_ACTUAL.  
  * **Pull-up Resistor:** A 10kΩ pull-up resistor from the tachometer line to 3.3V is essential.  
  * **PULSES\_PER\_REVOLUTION:** Most PC fans output 2 pulses per revolution. If you get half or double the expected RPM, this constant might need adjustment for your specific fan.  
  * **ISR and Pin:** Ensure the interrupt is correctly attached to the FAN\_TACH\_PIN\_ACTUAL in setup().  
  * **Fan Health:** Some very cheap or old fans might have faulty tachometer outputs.  
* **WiFi Connection Issues (No Connection, Drops):**  
  * **Credentials:** Double-check the SSID and password entered via the LCD menu or serial commands. They are case-sensitive.  
  * **Signal Strength:** Ensure the ESP32 has adequate WiFi signal. Metal PC cases can significantly attenuate signals. Consider using an ESP32 module with a U.FL connector for an external antenna if issues persist.  
  * **Router Configuration:** Check if your router has any MAC filtering or other security settings that might prevent the ESP32 from connecting.  
  * **Power Supply to ESP32:** Unstable power to the ESP32 can cause WiFi drops. Ensure your 5V supply from the buck converter is clean and stable.  
  * **Serial Logs (Debug Mode):** Enable debug mode (DEBUG\_ENABLE\_PIN HIGH) and check the serial monitor for detailed WiFi connection messages and error codes.  
* **Web Interface Not Loading or Not Updating:**  
  * **WiFi Connection:** Confirm the ESP32 is connected to WiFi and you are using the correct IP address in your browser. The device accessing the web UI must be on the *same network* as the ESP32.  
  * **SPIFFS Upload:** Ensure index.html, style.css, and script.js were correctly uploaded to the ESP32's SPIFFS using the "Upload Filesystem Image" task in PlatformIO. If these files are missing, the server will return 404 errors. Check serial logs for SPIFFS mount errors during boot.  
  * **WebSocket Connection:** Open your browser's developer console (usually F12). Check for WebSocket connection errors in the console tab. The JavaScript in script.js attempts to connect to ws://\<ESP32\_IP\_ADDRESS\>:81/.  
  * **Firewall:** Ensure no firewall on your PC or network is blocking WebSocket connections on port 81\.  
* **Settings Not Being Saved (NVS Issues):**  
  * **Serial Logs (Debug Mode):** Check for any NVS-related error messages like "Failed to open namespace" or errors during put operations.  
  * **NVS Full/Corrupted:** While unlikely for this amount of data, NVS can become corrupted. PlatformIO might have tools to erase the entire flash or NVS partition, after which settings would revert to defaults.  
  * **Namespace/Key Typos:** Ensure consistency in namespace names and keys used in save... and load... NVS functions.  
* **Serial Command Interface Not Working:**  
  * **DEBUG\_ENABLE\_PIN:** Verify this pin is pulled HIGH at boot time. The debug LED (LED\_DEBUG\_PIN) should be ON.  
  * **Baud Rate:** Ensure your Serial Monitor is set to 115200 baud.  
  * **Serial Port:** Confirm you have selected the correct COM port for the ESP32 in your IDE.  
  * **Serial.begin() Call:** The code is designed to only call Serial.begin() if serialDebugEnabled is true. If the pin is low, no serial communication will be initialized.  
* **ESP32 Keeps Rebooting (Boot Loop):**  
  * **Power Issues:** Insufficient or unstable power is a common cause. Check your 12V source and the 5V output of your buck converter.  
  * **Hardware Shorts:** Carefully inspect your wiring for any short circuits.  
  * **Software Crash:** A bug in the code (e.g., null pointer dereference, stack overflow in a task) can cause a crash and reboot. Enable serial debug and observe the output for crash reports or Guru Meditation Errors. The ESP32 often prints a backtrace that can help pinpoint the issue.  
  * **Stack Overflow:** If tasks are crashing, their allocated stack size (e.g., 10000 words in xTaskCreatePinnedToCore) might be insufficient. This is less likely with the current setup but possible if significant local variables or deep function calls are added.  
* **Compilation Errors:**  
  * **Library Not Found:** Ensure all required libraries are correctly installed (via PlatformIO's lib\_deps or Arduino Library Manager).  
  * **"'function' was not declared in this scope":** Often due to missing forward declarations or incorrect \#include order. Ensure header files are included before their contents are used.  
  * **Board Selection:** Double-check that the correct ESP32 board is selected in your IDE.

By systematically checking these areas and utilizing the serial debug output, most common problems can be diagnosed and resolved.

[Previous Chapter: Technical Details & Protocols](06-technical-details.md) | [Next Chapter: Future Enhancements](08-future-enhancements.md)