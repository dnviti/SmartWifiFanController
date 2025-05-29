# **Chapter 5: Usage Guide**

This chapter explains how to interact with and use the ESP32 PC Fan Controller after setup and installation, including the new MQTT Discovery and OTA update features.

## **5.1. First Boot and Initial State**

*(No significant changes, but firmware version is now more prominent)*

* **Power On:** After uploading the firmware and filesystem, power on your ESP32 fan controller.  
* **Debug Mode Check:** (As before)  
* **Initial WiFi & MQTT State:** (As before)  
* **LCD Display:** The LCD will initialize and show "Fan Controller" and the **current FIRMWARE\_VERSION** briefly. Once the mainAppTask starts, it will switch to the normal status display.  
* **Fan State:** (As before)  
* **Settings:** (As before)  
* **Root CA for OTA:** The device attempts to load a Root CA certificate from /github\_api\_ca.pem (or as defined by GITHUB\_ROOT\_CA\_FILENAME) on SPIFFS. If not found or empty, secure OTA updates from GitHub will fail.

## **5.2. LCD Menu Navigation**

The primary way to configure the device standalone is through the LCD menu system.

* **Button Functions:** (Menu, Up, Down, Select, Back)  
* **Main Menu Flow:**  
  1. **Normal Status Display**  
     * Press BTN\_MENU\_PIN \-\> **Main Menu**  
       * \>WiFi Settings  
       * MQTT Settings  
       * **OTA Update** (New Menu Item)  
       * View Status (Exits menu)  
  2. **WiFi Settings Menu:** (As before)  
  3. **WiFi Scan Menu (WIFI\_SCAN):** (As before)  
  4. **Password Entry Menu (WIFI\_PASSWORD\_ENTRY):** (As before)  
  5. **MQTT Settings Menu:** (As before)  
  6. **MQTT Entry Screens:** (As before)  
  7. **MQTT Discovery Settings Menu:** (As before)  
  8. **MQTT Discovery Prefix Entry Menu:** (As before)  
  9. **OTA Update Screen (OTA\_UPDATE\_SCREEN) (New):**  
     * Displays "Firmware Update".  
     * Options:  
       * \>Check & Update: (If WiFi is connected) Initiates a check with GitHub for the latest release. If a newer version is found, it will attempt to download and install firmware (and SPIFFS if available in the release). The LCD will show status messages like "Fetching...", "Updating FW...", "Updating FS...", "Rebooting...".  
       * \>Back to Main: Returns to the main menu.  
     * If an OTA update is already in progress, this screen will show "OTA In Progress:" and the current status message.  
  10. **Confirm Reboot Menu (CONFIRM\_REBOOT):** (As before)

## **5.3. Serial Command Interface (Debug Mode)**

* **Activation:** DEBUG\_ENABLE\_PIN HIGH at boot.  
* **Connection:** Serial Monitor at 115200 baud.  
* **Key Serial Commands (type help for full list):**  
  * status: Displays current operational status including WiFi, MQTT state, Discovery settings, **and current OTA status/firmware version.**  
  * set\_mode auto / set\_mode manual \<percentage\>  
  * WiFi commands (as before)  
  * MQTT commands (as before)  
  * MQTT Discovery commands (as before)  
  * **ota\_update (New):** If WiFi is connected, triggers a check for new firmware/SPIFFS releases on GitHub. If a newer version is found, it attempts to download and apply the update. Progress and status messages are printed to the serial console.  
  * Fan curve commands (as before)  
  * reboot

## **5.4. Web Interface (If WiFi Enabled)**

* Access via ESP32's IP address.  
* Real-time status display (Temp, Fan Speed/RPM, Mode, **Firmware Version**).  
* Mode control (Auto/Manual buttons).  
* Manual speed slider.  
* Fan Curve Editor.  
* MQTT Configuration Section.  
* MQTT Discovery Configuration Section.  
* **OTA Update Section (New):**  
  * Displays current firmware version.  
  * Shows current OTA status message (e.g., "Idle", "Checking...", "Updating FW...", "Error...").  
  * A button "Check for Updates & Install". Clicking this (after confirmation) triggers the GitHub OTA update check process. The button will be disabled while an update is in progress.  
* **ElegantOTA Manual Update (New):**  
  * Navigate to http://\<ESP32\_IP\_ADDRESS\>/update.  
  * This page allows manual upload of firmware.bin or spiffs.bin files for OTA update. This is separate from the GitHub-based OTA.

## **5.5. MQTT Usage for Home Automation (with Home Assistant Discovery)**

*(No direct changes to MQTT usage itself, but the status\_json payload now includes firmware version and OTA status)*

* **Status Topic (status\_json):** Now includes firmwareVersion, otaInProgress, and otaStatusMessage fields.

## **5.6. Over-the-Air (OTA) Updates**

The device supports two methods for OTA updates:

1. **ElegantOTA (Manual Web Upload):**  
   * **Access:** http://\<ESP32\_IP\_ADDRESS\>/update  
   * **Functionality:** Allows you to manually upload a compiled firmware.bin file or a spiffs.bin (filesystem image) directly to the device through your web browser.  
   * **Use Case:** Useful for deploying specific builds, testing, or if the GitHub OTA mechanism is unavailable.  
2. **GitHub Release Updater (Automated Check & Install):**  
   * **Trigger:** Can be initiated from the LCD Menu, Web UI button, or Serial Command (ota\_update).  
   * **Process:**  
     1. The ESP32 connects to the GitHub API (securely, using the CA certificate from SPIFFS) to fetch information about the latest release from the dnviti/SmartWifiFanController repository.  
     2. It compares the tag name (version) of the latest release with the FIRMWARE\_VERSION compiled into the currently running firmware.  
     3. If a newer version is found:  
        * It downloads the firmware\_PIO\_BUILD\_ENV\_NAME\_vX.Y.Z.bin asset.  
        * It applies the firmware update using HTTPUpdate. The device will reboot after a successful firmware update.  
        * If a spiffs\_PIO\_BUILD\_ENV\_NAME\_vX.Y.Z.bin asset is also found in the release, and the firmware update was successful (leading to a reboot), the intention is for the system to handle this. *Currently, the SPIFFS update is attempted sequentially after firmware in the same function; if firmware reboots, a more robust two-stage mechanism (e.g., using NVS flag) would be needed for automatic SPIFFS update post-reboot.* For now, if firmware updates and reboots, you might need to trigger the OTA again to get the SPIFFS, or update SPIFFS manually via ElegantOTA or serial.  
     4. If the firmware is already up-to-date, or if an error occurs (e.g., no WiFi, GitHub unreachable, no valid assets), an appropriate status message is shown.  
   * **Root CA Certificate:** This process relies on a valid Root CA certificate named github\_api\_ca.pem (or as defined by GITHUB\_ROOT\_CA\_FILENAME in config.h) being present in the ESP32's SPIFFS filesystem. This file is automatically included in the spiffs.bin generated by the GitHub Actions release pipeline.

[Previous Chapter: Setup and Installation](04-setup-and-installation.md) | [Next Chapter: Technical Details & Protocols](06-technical-details.md)