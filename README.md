# SmartWifiFanController 💨🌡️

**Developer**: Daniele Viti
**Repository**: [https://github.com/dnviti/SmartWifiFanController](https://github.com/dnviti/SmartWifiFanController)

A comprehensive ESP32-based fan controller with WiFi connectivity, web interface, LCD display, and persistent configuration for PC fans or similar applications. It uses a BMP280 temperature sensor to control fan speed via PWM, either through a configurable fan curve (Auto Mode) or a manually set speed (Manual Mode).

## 🌟 Features

* **Dual Control Modes**:
    * **Auto Mode**: Adjusts fan speed based on a user-defined temperature curve.
    * **Manual Mode**: Allows setting a fixed fan speed percentage.
* **Web Interface**:
    * Real-time monitoring of temperature, fan speed (%), fan RPM, and current mode.
    * Switch between Auto/Manual modes.
    * Adjust manual fan speed via a slider.
    * **Fan Curve Editor**: Graphically add, remove, and adjust points (temperature vs. PWM %) for Auto Mode. Curve is saved on the ESP32's non-volatile storage (NVS). Max 8 points, min 2 points per curve.
* **LCD Display (16x2 I2C)**:
    * Shows current temperature, fan speed percentage, fan RPM (if tachometer connected), current mode (Auto/Manual), and WiFi IP address when connected.
    * **On-device Menu System**: Navigate using physical buttons to:
        * Toggle WiFi enable/disable.
        * Scan for WiFi networks.
        * Select SSID and enter password.
        * View WiFi status.
        * Confirm reboot if settings are changed.
* **Fan Control**:
    * PWM output for fan speed control (25kHz frequency).
    * Tachometer input for RPM monitoring.
* **Temperature Sensing**:
    * Utilizes Adafruit BMP280 sensor for ambient temperature readings.
    * If the sensor is not found, Auto Mode uses a default fixed fan speed, and the curve editor is disabled in the web UI.
* **Persistent Configuration**:
    * Saves WiFi credentials (SSID, password, enabled state) and the fan curve to NVS.
* **Serial Debug/Command Interface**:
    * Optional serial output for debugging (enable via a physical pin/jumper).
    * Allows advanced control and status checks via serial commands (e.g., `set_mode`, `set_ssid`, `view_curve`, `reboot`).
* **Connectivity**:
    * Connects to your local WiFi network (STA mode).
    * Serves a web page from SPIFFS (ESP32's flash filesystem).
    * Uses WebSockets for efficient, real-time two-way communication between the ESP32 and the web interface.
* **Software Architecture**:
    * Built using PlatformIO and the Arduino framework for ESP32.
    * Utilizes FreeRTOS tasks for concurrent handling of network operations (Core 0) and main application logic (Core 1).
    * Modular C++ code structure for better organization and maintainability.

## ⚙️ Hardware Requirements

* **Microcontroller**: ESP32 (project configured for `upesy_wroom` board, but adaptable).
* **Temperature Sensor**: Adafruit BMP280 (I2C).
* **Display**: 16x2 I2C LCD.
* **Fan**: 3-pin or 4-pin PC Fan (or similar) with PWM control input and optionally a tachometer output.
* **Buttons**: Typically 5 push buttons for menu navigation (Menu, Up, Down, Select, Back).
* **Optional**: Jumper/Switch for enabling Serial Debug mode.
* Standard electronic components (resistors, breadboard, wires as needed).

Refer to `src/config.h` and `src/main.cpp` for default pin assignments.

## 📚 Software & Libraries

This project is built using [PlatformIO](https://platformio.org/). Key libraries used include:

* `espressif32` platform
* `arduino` framework
* `ottowinter/ESPAsyncWebServer-esphome` (Async Web Server)
* `esphome/AsyncTCP-esphome` (Async TCP support)
* `links2004/WebSockets` (WebSocket server)
* `adafruit/Adafruit BMP280 Library`
* `adafruit/Adafruit Unified Sensor`
* `iakop/LiquidCrystal_I2C_ESP32`
* `bblanchon/ArduinoJson` (JSON handling)
* `Preferences` (ESP32 NVS library, part of ESP32 core)
* `SPIFFS` (ESP32 flash filesystem, part of ESP32 core)

## 📂 Project Structure

The project is organized into several key modules:

* `data/`: Contains web files (HTML, CSS, JavaScript) to be uploaded to SPIFFS.
* `src/`: Contains the C++ source code.
    * `main.cpp`: Main application entry point, setup, and core logic orchestration.
    * `config.h`: Global configurations, pin definitions, and extern declarations.
    * `fan_control.(h/cpp)`: Logic for fan PWM control, RPM calculation, and fan curve interpolation.
    * `display_handler.(h/cpp)`: Manages the LCD display for normal mode and menu navigation.
    * `input_handler.(h/cpp)`: Handles button presses for the LCD menu and processes serial commands.
    * `network_handler.(h/cpp)`: Manages WiFi connection, web server routes, and WebSocket communication.
    * `nvs_handler.(h/cpp)`: Handles saving and loading configurations (WiFi, fan curve) from/to Non-Volatile Storage.
    * `tasks.(h/cpp)`: Defines and manages FreeRTOS tasks for concurrent operations.
* `platformio.ini`: PlatformIO project configuration file, including library dependencies and board settings.

## 🛠️ Setup & Installation

1.  **Clone the Repository**:
    ```bash
    git clone [https://github.com/dnviti/SmartWifiFanController.git](https://github.com/dnviti/SmartWifiFanController.git)
    cd SmartWifiFanController
    ```
2.  **PlatformIO**:
    * Ensure you have [Visual Studio Code](https://code.visualstudio.com/) with the [PlatformIO IDE extension](https://platformio.org/platformio-ide) installed.
    * Open the cloned project folder in VS Code. PlatformIO should automatically recognize it.
3.  **Configure Hardware Pins**:
    * Review and update pin definitions in `src/config.h` or `src/main.cpp` if your wiring differs from the defaults.
4.  **Build & Upload Firmware**:
    * Use the PlatformIO "Build" task.
    * Use the PlatformIO "Upload" task to flash the firmware to your ESP32.
5.  **Upload SPIFFS Filesystem Image**:
    * The web interface files (`index.html`, `style.css`, `script.js` located in the `data` directory) need to be uploaded to the ESP32's SPIFFS.
    * Use the PlatformIO task "Upload Filesystem Image" (or `pio run --target uploadfs` from the PlatformIO CLI).

## 🚀 Usage

### Initial Setup (WiFi)

* On the first boot, or if WiFi is not configured, you'll need to set up the WiFi connection using the LCD menu:
    1.  Press the **Menu** button to enter the menu.
    2.  Navigate to "WiFi Settings" and press **Select**.
    3.  If WiFi is "Disabled", select "WiFi: Disabled" to toggle it to "Enabled". This will require a reboot (confirm "Yes").
    4.  After reboot and re-entering the menu, go to "WiFi Settings" -> "Scan Networks".
    5.  Select your desired SSID from the scanned list.
    6.  Go to "Password Set" to enter your WiFi password character by character using Up/Down for character selection and Select to confirm each character. Select "OK" (or similar, usually implied by filling the buffer and pressing select) when done.
    7.  Select "Connect WiFi". The device will attempt to connect. If successful, it will prompt for a reboot to ensure network services start correctly.

### Web Interface

* Once the ESP32 is connected to WiFi, the LCD will display its IP address.
* Open this IP address in a web browser on a device connected to the same network.
* You can now monitor the fan status, switch modes, adjust manual speed, and edit the fan curve.

### LCD Menu

* **Menu Button**: Enter/Exit the menu system. If `rebootNeeded` is true, exiting the menu might take you to a "Confirm Reboot" screen.
* **Up/Down Buttons**: Navigate through menu items or change values (e.g., password characters).
* **Select Button**: Choose a menu item or confirm an action.
* **Back Button**: Go to the previous menu screen or cancel an action.

### Serial Commands (Optional, for Debugging/Advanced Users)

* If serial debug is enabled (via the `DEBUG_ENABLE_PIN` being HIGH at boot), connect to the ESP32 via a serial terminal (e.g., Arduino IDE Serial Monitor, PlatformIO Serial Monitor) at 115200 baud.
* Type `help` to see a list of available commands like `status`, `set_mode auto`, `set_mode manual <speed>`, `scan_wifi`, `view_curve`, `stage_curve_point <temp> <pwm>`, `apply_staged_curve`, `reboot`, etc..

## 🔧 Configuration

* **WiFi Credentials**: Set via the LCD menu or serial commands (`set_ssid`, `set_pass`). Saved to NVS.
* **Fan Curve**: Configured via the web interface or serial commands (`stage_curve_point`, `apply_staged_curve`). Saved to NVS.
    * A default fan curve is applied if no curve is found in NVS or if the stored curve is invalid.
    * The web UI allows up to 8 points. Temperatures must be increasing for subsequent points.
* **Pin Definitions**: Modify in `src/config.h` or `src/main.cpp` if needed.
* **PWM Parameters**: `PWM_FREQ`, `PWM_RESOLUTION_BITS` can be adjusted in `src/main.cpp` if required for your specific fan.

## 🤝 Contributing

Contributions are welcome! Please read the [CONTRIBUTING.md](CONTRIBUTING.md) file for guidelines on how to contribute to this project.

## 📜 License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details.