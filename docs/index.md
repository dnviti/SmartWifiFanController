# **ESP32 PC Fan Controller**

An advanced, highly configurable PC fan controller based on the ESP32 microcontroller, featuring LCD menu control, optional WiFi with a web interface, and a serial debug console.

This project provides a robust solution for managing PC cooling fans with precision, offering both automated temperature-based regulation and manual control. It's designed for hobbyists and enthusiasts looking for a customizable alternative to standard motherboard fan controls.

## **Documentation Chapters**

Please navigate through the documentation using the links below:

1. [**Introduction**](01-introduction.md)  
   * Project Overview  
   * Why This Project?  
   * Key Goals  
   * Features Summary  
2. [**Hardware**](02-hardware.md)  
   * Required Components  
   * Recommended Hardware Stack & Component Details  
   * Wiring Considerations & Block Diagram  
3. [**Software**](03-software.md)  
   * Development Environment  
   * Libraries Required & platformio.ini  
   * Firmware Architecture (Dual-Core, Tasks)  
   * Code Structure (Key Files and Their Roles)  
4. [**Setup and Installation**](04-setup-and-installation.md)  
   * Hardware Assembly Tips  
   * Software Setup (PlatformIO)  
   * Uploading Firmware and Filesystem Data  
5. [**Usage Guide**](05-usage-guide.md)  
   * First Boot and Initial State  
   * LCD Menu Navigation & Options  
   * Serial Command Interface (Debug Mode)  
   * Web Interface (If WiFi Enabled)  
6. [**Technical Details & Protocols**](06-technical-details.md)  
   * PWM Fan Control  
   * I2C Communication  
   * Fan Tachometer (RPM Sensing)  
   * WiFi and Networking (Async Web Server, WebSockets)  
   * SPIFFS Filesystem Usage  
   * NVS (Non-Volatile Storage) for Persistence  
   * Conditional Debug Mode & LED Indicator  
   * Dual-Core Operation (FreeRTOS Tasks)  
7. [**Troubleshooting**](07-troubleshooting.md)  
   * Common Issues and Solutions  
8. [**Future Enhancements**](08-future-enhancements.md)  
   * Potential improvements and new features.

*This documentation is for the ESP32 PC Fan Controller project. Please refer to the specific chapter files for detailed information.*
