# **Chapter 4: Setup and Installation**

This chapter guides you through assembling the hardware, setting up the software environment using PlatformIO, and uploading the firmware and web files to your ESP32.

## **4.1. Hardware Assembly Tips**

Assembling the hardware requires careful attention to connections to avoid damage to components.

* **Safety First:** Always disconnect power before making any wiring changes.  
* **Power Supply:**  
  * Connect your 12V DC source to the input of your 12V-to-5V buck converter. Verify the output is a stable 5V with a multimeter **before** connecting it to the ESP32.  
  * Connect the 5V output of the buck converter to the ESP32's VIN (or 5V) pin and its GND to an ESP32 GND pin.  
* **ESP32 Pinout:** Refer to the pinout diagram for your specific ESP32 development board. GPIO numbers used in config.h must match your wiring.  
* **I2C Devices (LCD & BMP280):**  
  * Connect SDA lines together and to the ESP32's designated SDA pin (default GPIO21).  
  * Connect SCL lines together and to the ESP32's designated SCL pin (default GPIO22).  
  * Power the LCD typically with 5V. Power the BMP280 typically with 3.3V (from the ESP32's 3V3 pin).  
  * Ensure I2C pull-up resistors (4.7kΩ \- 10kΩ to 3.3V) are present on both SDA and SCL lines. Some modules have them built-in; if not, you must add them externally.  
* **Buttons:**  
  * Connect one terminal of each button to its assigned GPIO pin (e.g., BTN\_MENU\_PIN).  
  * Connect the other terminal of each button to GND. The firmware uses INPUT\_PULLUP, so external pull-up resistors are not strictly needed for the buttons themselves if wired this way.  
* **Fan Connection (4-pin PWM Fan):**  
  * **GND (Pin 1):** Connect to the common circuit ground.  
  * **\+12V (Pin 2):** Connect directly to the 12V power source that also feeds your buck converter.  
  * **Tachometer/Sense (Pin 3):** Connect to FAN\_TACH\_PIN\_ACTUAL on the ESP32. Add a 10kΩ pull-up resistor between this pin and the ESP32's 3.3V line.  
  * **Control/PWM (Pin 4):** This is critical. Connect the ESP32's FAN\_PWM\_PIN to the **input** of a 3.3V to 5V logic level shifter. Connect the **output** of the logic level shifter to the fan's Pin 4\. Ensure the shifter is also powered correctly (usually with both 3.3V and 5V references).  
* **Debug Enable Pin (DEBUG\_ENABLE\_PIN):**  
  * Connect this pin to a switch or jumper. One side of the switch/jumper goes to the GPIO, the other to 3.3V.  
  * The pin is configured as INPUT\_PULLDOWN. Leaving it open or connecting it to GND will disable debug mode. Connecting it to 3.3V will enable debug mode.  
* **Debug LED (LED\_DEBUG\_PIN):**  
  * Connect the anode (longer leg) of an LED to this GPIO.  
  * Connect the cathode (shorter leg) to a current-limiting resistor (220Ω to 330Ω is typical for a standard LED with 3.3V logic).  
  * Connect the other end of the resistor to GND.  
* **Prototyping:** Start on a breadboard to easily test connections and make changes. Once confirmed, consider soldering to a perfboard or designing a custom PCB for a permanent solution.

## **4.2. Software Setup (PlatformIO)**

PlatformIO IDE within Visual Studio Code is the recommended development environment.

1. **Install Visual Studio Code:** Download and install from [code.visualstudio.com](https://code.visualstudio.com/).  
2. **Install PlatformIO IDE Extension:**  
   * Open VS Code.  
   * Go to the Extensions view (Ctrl+Shift+X or click the square icon on the sidebar).  
   * Search for "PlatformIO IDE" and install it.  
3. **Obtain Project Files:**  
   * Clone the project repository from GitHub or download the source code ZIP.  
4. **Open Project in PlatformIO:**  
   * In VS Code, go to File \> Open Folder... and select the root folder of the cloned/downloaded project.  
   * PlatformIO should automatically recognize the platformio.ini file and set up the project environment.  
5. **Review platformio.ini:**  
   * Open the platformio.ini file in the project root.  
   * **Crucially, ensure the board setting matches your specific ESP32 development board model** (e.g., esp32dev, nodemcu-32s, upesy\_wroom, etc.). Refer to [PlatformIO Boards documentation](https://docs.platformio.org/en/latest/boards/espressif32/index.html) for identifiers.  
   * Verify the lib\_deps section lists all required libraries (see [Chapter 3.2](#bookmark=id.jyxdfsov5lj0)). PlatformIO will attempt to download and install these automatically during the first build.  
6. **Configure Default WiFi (Optional):**  
   * Open src/main.cpp (or src/config.h if you moved definitions there).  
   * Locate char current\_ssid\[64\] \= "YOUR\_WIFI\_SSID"; and char current\_password\[64\] \= "YOUR\_WIFI\_PASSWORD";.  
   * You can change "YOUR\_WIFI\_SSID" and "YOUR\_WIFI\_PASSWORD" to your network's credentials if you want the device to attempt connecting to this network by default if no other credentials are saved in NVS and WiFi is enabled. Otherwise, these can be configured later via the LCD menu or Serial commands.

## **4.3. Uploading Firmware and Filesystem Data**

The project requires both the main firmware and the web interface files (HTML, CSS, JS) to be uploaded to the ESP32.

1. **Prepare Web Files for SPIFFS:**  
   * Ensure you have an index.html, style.css, and script.js file.  
   * In the root directory of your PlatformIO project (the same level as src/, lib/, platformio.ini), create a folder named data.  
   * Place your index.html, style.css, and script.js files directly inside this data folder.  
2. **Build and Upload Filesystem Image:**  
   * This step uploads the contents of the data folder to the ESP32's SPIFFS partition.  
   * **In VS Code with PlatformIO:**  
     * Click on the PlatformIO icon (alien head) in the VS Code activity bar (left sidebar).  
     * Expand your project environment (e.g., env:esp32dev or your specific board).  
     * Under "Platform", find and click on "Upload Filesystem Image".  
   * **Using PlatformIO Core CLI:**  
     * Open a PlatformIO CLI terminal in VS Code (Terminal \> New Terminal, then ensure it's a PlatformIO terminal or type pio to activate it).  
     * Run the command: pio run \-t uploadfs  
   * This process might take a minute or two. It will build a SPIFFS image and upload it. You only need to do this when you change the web files in the data folder.  
3. **Build and Upload Firmware:**  
   * This compiles your C++ code and uploads the resulting binary to the ESP32.  
   * **In VS Code with PlatformIO:**  
     * Click the "Build" button (checkmark icon in the bottom status bar or find it in the PlatformIO project tasks).  
     * After a successful build, click the "Upload" button (right arrow icon in the bottom status bar or find it in the PlatformIO project tasks).  
   * **Using PlatformIO Core CLI:**  
     * Build: pio run  
     * Upload: pio run \-t upload  
   * PlatformIO will usually auto-detect the correct serial port for uploading. If not, you may need to specify it in platformio.ini using the upload\_port option.

After completing these steps, your ESP32 should be running the fan controller firmware with the web interface files available on its filesystem.

[Previous Chapter: Software](03-software.md) | [Next Chapter: Usage Guide](05-usage-guide.md)