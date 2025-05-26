# **Chapter 1: Introduction**

## **1.1. Project Overview**

This project details the creation of an advanced and highly customizable PC fan controller utilizing the ESP32 microcontroller. It aims to offer superior control, monitoring, and configuration options compared to standard motherboard fan control systems or basic commercial fan hubs. The controller allows for both automated temperature-based fan regulation and precise manual adjustments, accessible through multiple interfaces: an onboard LCD menu with physical buttons, a web interface (when WiFi is active), and a conditional serial command interface designed for debugging and advanced interaction.

The core philosophy is to provide a flexible, open, and powerful tool for PC enthusiasts who want granular control over their system's cooling and acoustics.

## **1.2. Why This Project?**

Many PC users, especially those building custom systems or pushing their hardware, find stock fan control solutions lacking. Motherboard BIOS settings can be limited, and software solutions can sometimes be resource-intensive or platform-dependent. Commercial fan controllers exist, but they may not offer the desired level of customization, open-source flexibility, or specific features an enthusiast might want.

This ESP32-based project addresses these points by:

* **Offering Deep Customization:** Users can define precise fan curves, control multiple aspects of the system, and even extend the functionality due to its open nature.  
* **Providing Multiple Control Interfaces:** Catering to different user preferences, whether it's direct physical interaction via an LCD, remote control via a web browser, or technical interaction via a serial console.  
* **Leveraging Modern Microcontroller Capabilities:** The ESP32's dual-core processor, built-in WiFi, and ample GPIOs make it an ideal platform for such a feature-rich device.  
* **Being an Educational Platform:** Building this project offers insights into embedded systems, PWM control, sensor integration, web servers on microcontrollers, and real-time operating system (RTOS) concepts.

## **1.3. Key Goals**

The primary objectives of this project are:

* **Precision:** Implement fine-grained PWM control for 4-pin PC fans, allowing for smooth and accurate speed adjustments from 0% to 100%.  
* **Automation:** Enable automatic fan speed adjustment based on (optional) temperature sensor readings via a user-configurable multi-point fan curve.  
* **Manual Override:** Provide users the ability to manually set and hold specific fan speeds, irrespective of temperature.  
* **Real-time Monitoring:** Display critical system parameters including current temperature (if sensor present), current fan speed (as a percentage), and actual fan RPM (via tachometer feedback).  
* **Multiple Interfaces:**  
  * **LCD & Buttons:** An intuitive onboard menu system for standalone configuration and status display.  
  * **Web UI:** A responsive web interface for remote monitoring and configuration when WiFi is enabled.  
  * **Serial Console:** A command-line interface for debugging, advanced configuration, and diagnostics, activated by a dedicated debug pin.  
* **Flexibility & Modularity:**  
  * Make WiFi connectivity an optional feature, configurable by the user.  
  * Make the temperature sensor an optional component, with graceful fallback behavior if not present.  
* **Persistence:** Store all user settings (WiFi credentials, fan curve, operational states like WiFi enabled/disabled) in the ESP32's non-volatile memory (NVS) to ensure they persist across reboots and power cycles.  
* **Efficiency & Responsiveness:** Leverage the ESP32's dual-core architecture with FreeRTOS to separate network handling from the main control loop, ensuring responsive performance.  
* **Debuggability:** Include comprehensive serial logging (conditionally enabled) and an onboard LED indicator for debug mode status.  
* **Accessibility & Affordability:** Design the system using readily available, relatively inexpensive components without significantly compromising on quality, reliability, or the "cool factor" of the final product.

## **1.4. Features Summary**

* Controls one or more 4-pin PWM PC fans (current software focuses on one, easily expandable).  
* Optional BMP280 temperature sensor for precise thermal readings.  
* Automated fan speed control based on a user-configurable multi-point temperature curve (editable via Web UI or Serial).  
* Manual fan speed control (0-100%) selectable via LCD, Web UI, or Serial.  
* Real-time fan RPM monitoring via tachometer input, displayed on LCD and Web UI.  
* 16x2 I2C LCD display for live status and a comprehensive menu-driven configuration system.  
* Physical button interface (Menu, Up, Down, Select, Back) for intuitive LCD menu navigation.  
* Optional WiFi connectivity:  
  * Web interface served from SPIFFS for remote monitoring and configuration.  
  * Real-time data synchronization using WebSockets.  
* Full WiFi configuration (Enable/Disable, Scan for Networks, Set SSID, Set Password, Connect, Disconnect) manageable via the LCD menu.  
* Persistent storage of all critical settings in NVS.  
* Dual-core operation for enhanced performance:  
  * Core 0: Dedicated to network tasks (WiFi, Web Server, WebSockets).  
  * Core 1: Handles main application logic (sensor polling, LCD updates, button inputs, fan control algorithms, serial commands).  
* Conditional Serial Debug Interface:  
  * Activated by a physical GPIO pin (DEBUG\_ENABLE\_PIN).  
  * Provides extensive runtime logs and a command-line interface for advanced control.  
  * An onboard LED visually indicates if debug mode is active.  
* Web UI features a loading animation for an improved initial page load experience.  
* Modular C++ code structure, separating concerns into logical files for better maintainability and scalability.

[Next Chapter: Hardware](http://docs.google.com/02-hardware.md)