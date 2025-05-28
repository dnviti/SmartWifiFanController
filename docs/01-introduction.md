# **Chapter 1: Introduction**

## **1.1. Project Overview**

This project details the creation of an advanced and highly customizable PC fan controller utilizing the ESP32 microcontroller. It aims to offer superior control, monitoring, and configuration options compared to standard motherboard fan control systems or basic commercial fan hubs. The controller allows for both automated temperature-based fan regulation and precise manual adjustments, accessible through multiple interfaces: an onboard LCD menu with physical buttons, a web interface (when WiFi is active), a conditional serial command interface, MQTT with Home Assistant Discovery for seamless integration into smart home ecosystems, **and Over-the-Air (OTA) firmware updates.**

The core philosophy is to provide a flexible, open, and powerful tool for PC enthusiasts who want granular control over their system's cooling and acoustics, **with the ability to easily update the device firmware remotely.**

## **1.2. Why This Project?**

Many PC users, especially those building custom systems or pushing their hardware, find stock fan control solutions lacking. Motherboard BIOS settings can be limited, and software solutions can sometimes be resource-intensive or platform-dependent. Commercial fan controllers exist, but they may not offer the desired level of customization, open-source flexibility, or specific features an enthusiast might want.

This ESP32-based project addresses these points by:

* **Offering Deep Customization:** Users can define precise fan curves, control multiple aspects of the system, and even extend the functionality due to its open nature.  
* **Providing Multiple Control Interfaces:** Catering to different user preferences, whether it's direct physical interaction via an LCD, remote control via a web browser, technical interaction via a serial console, integration with home automation via MQTT with auto-discovery, **or initiating firmware updates through these interfaces.**  
* **Leveraging Modern Microcontroller Capabilities:** The ESP32's dual-core processor, built-in WiFi, and ample GPIOs make it an ideal platform for such a feature-rich device.  
* **Being an Educational Platform:** Building this project offers insights into embedded systems, PWM control, sensor integration, web servers on microcontrollers, real-time operating system (RTOS) concepts, IoT communication protocols like MQTT (including service discovery), **and implementing secure remote firmware updates.**  
* **Facilitating Easy Updates:** **The OTA update feature allows for seamless deployment of new features and bug fixes without requiring physical access to the device.**

## **1.3. Key Goals**

The primary objectives of this project are:

* **Precision:** Implement fine-grained PWM control for 4-pin PC fans, allowing for smooth and accurate speed adjustments from 0% to 100%.  
* **Automation:** Enable automatic fan speed adjustment based on (optional) temperature sensor readings via a user-configurable multi-point fan curve.  
* **Manual Override:** Provide users the ability to manually set and hold specific fan speeds, irrespective of temperature.  
* **Real-time Monitoring:** Display critical system parameters including current temperature (if sensor present), current fan speed (as a percentage), and actual fan RPM (via tachometer feedback).  
* **Multiple Interfaces:**  
  * **LCD & Buttons:** An intuitive onboard menu system for standalone configuration, status display, **and initiating OTA updates.**  
  * **Web UI:** A responsive web interface for remote monitoring, configuration, **and initiating OTA updates.**  
  * **Serial Console:** A command-line interface for debugging, advanced configuration, diagnostics, **and initiating OTA updates.**  
  * **MQTT:** Integration for publishing status and receiving commands from home automation systems, featuring Home Assistant MQTT Discovery for automatic entity creation.  
* **Remote Firmware Updates (OTA):**  
  * **Implement OTA updates via the ESPAsyncWebServer (ElegantOTA) for manual file uploads.**  
  * **Implement a feature to check for and download firmware and SPIFFS updates directly from GitHub releases.** This includes version checking against the current firmware.  
* **Flexibility & Modularity:**  
  * Make WiFi connectivity an optional feature, configurable by the user.  
  * Make the temperature sensor an optional component, with graceful fallback behavior if not present.  
  * Make MQTT connectivity and Home Assistant Discovery optional features, configurable by the user.  
* **Persistence:** Store all user settings (WiFi credentials, MQTT configuration, MQTT Discovery settings, fan curve, operational states) in the ESP32's non-volatile memory (NVS) to ensure they persist across reboots and power cycles.  
* **Efficiency & Responsiveness:** Leverage the ESP32's dual-core architecture with FreeRTOS to separate network handling from the main control loop, ensuring responsive performance.  
* **Debuggability:** Include comprehensive serial logging (conditionally enabled) and an onboard LED indicator for debug mode status.  
* **Accessibility & Affordability:** Design the system using readily available, relatively inexpensive components without significantly compromising on quality, reliability, or the "cool factor" of the final product.

## **1.4. Features Summary**

* Controls one or more 4-pin PWM PC fans (current software focuses on one, easily expandable).  
* Optional BMP280 temperature sensor for precise thermal readings.  
* Automated fan speed control based on a user-configurable multi-point temperature curve (editable via Web UI, MQTT, or Serial).  
* Manual fan speed control (0-100%) selectable via LCD, Web UI, Serial, or MQTT.  
* Real-time fan RPM monitoring via tachometer input, displayed on LCD, Web UI, and published via MQTT.  
* 16x2 I2C LCD display for live status and a comprehensive menu-driven configuration system.  
* Physical button interface (Menu, Up, Down, Select, Back) for intuitive LCD menu navigation.  
* Optional WiFi connectivity:  
  * Web interface served from SPIFFS for remote monitoring and configuration (including MQTT and Discovery settings).  
  * Real-time data synchronization using WebSockets.  
* **Over-the-Air (OTA) Firmware Updates:**  
  * **ElegantOTA Integration:** Provides a web endpoint (/update) for manually uploading firmware.bin or spiffs.bin files.  
  * **GitHub Release Updater:** Feature to check the project's GitHub repository (dnviti/SmartWifiFanController) for the latest release.  
    * Compares the latest release version with the current running firmware version.  
    * If a newer version is found, allows the user to initiate the download and update of both firmware and SPIFFS image directly from the GitHub release assets.  
    * Can be triggered via LCD Menu, Web UI, or Serial Command.  
    * Uses a Root CA certificate loaded from SPIFFS (/github\_root\_ca.pem) for secure HTTPS connections to GitHub.  
* Optional MQTT Connectivity with Home Assistant Discovery:  
  * Publishes device status (temperature, fan speed, RPM, mode, settings states, etc.) to configurable topics.  
  * Subscribes to command topics for remote control (mode, manual speed, fan curve, select configurations).  
  * Automatically creates entities in Home Assistant for fan control, sensors (temp, RPM, diagnostics), and configuration (fan curve, discovery settings, reboot).  
  * Supports MQTT Last Will and Testament (LWT) for availability status.  
* Full WiFi, MQTT, and MQTT Discovery configuration manageable via all interfaces.  
* Persistent storage of all critical settings in NVS.  
* Dual-core operation for enhanced performance:  
  * Core 0: Dedicated to network tasks (WiFi, Web Server, WebSockets, MQTT Client, **ElegantOTA handler**).  
  * Core 1: Handles main application logic (sensor polling, LCD updates, button inputs, fan control algorithms, serial commands, **GitHub OTA check logic**).  
* Conditional Serial Debug Interface:  
  * Activated by a physical GPIO pin (DEBUG\_ENABLE\_PIN).  
  * Provides extensive runtime logs and a command-line interface for advanced control.  
  * An onboard LED visually indicates if debug mode is active.  
* Web UI features a loading animation for an improved initial page load experience.  
* Modular C++ code structure, separating concerns into logical files for better maintainability and scalability.  
* Firmware version tracking.  
* **Dynamic Hostname:** Sets hostname to fancontrol-\[macaddress\] for easier network identification.  
* **Automated GitHub Release Pipeline:** Builds firmware and SPIFFS (including the latest CA certificate) upon pushes to release/v\* branches, creating versioned and "latest" GitHub releases.

[Next Chapter: Hardware](02-hardware.md)