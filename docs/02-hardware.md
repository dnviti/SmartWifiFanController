# Chapter 2: Hardware

This chapter details the necessary and recommended hardware components for building the ESP32 PC Fan Controller, along with high-level wiring considerations.

## 2.1. Required Components

The following components are essential for a basic functional version of the project:

* **Microcontroller:**
    * An ESP32-based development board. Common choices include generic ESP32 DevKits (often with the ESP32-WROOM-32 module) or specific boards like the uPesy ESP32 Wroom DevKit. Ensure it has sufficient GPIO pins for all planned peripherals.
* **Display:**
    * 16x2 I2C Character LCD Display: Must have an I2C backpack module (commonly using the PCF8574 I/O expander chip) to simplify wiring. The I2C address is typically `0x27` or `0x3F`.
* **Navigation Inputs:**
    * 5x Tactile Push Buttons: For Menu, Up, Down, Select, and Back functions.
* **PC Fan(s):**
    * At least one standard 4-pin PWM PC cooling fan (12V). The project is designed around 4-pin fans for PWM speed control.
* **Power Supply Components:**
    * **12V DC Source:** This will power the fans directly and be stepped down for the ESP32. Options:
        * PC Power Supply Unit (PSU): Using a Molex or SATA power connector (requires appropriate adapter/wiring).
        * Motherboard Fan Header: Can supply 12V, but ensure the total current draw of your controller and connected fan(s) does not exceed the header's limit (typically 1A, but check motherboard specs).
        * External 12V DC Power Adapter: If building a standalone unit.
    * **12V to 5V Buck Converter Module:** Essential for stepping down the 12V input to a stable 5V suitable for the ESP32's VIN pin. Modules based on the MP1584EN, LM2596 (use with caution, quality varies), or similar switching regulators are common. Ensure its current rating is sufficient for the ESP32 and any other 5V peripherals.
* **Resistors:**
    * **Pull-up Resistors for I2C (if not on modules):** Typically 2x 4.7kΩ or 10kΩ resistors (one for SDA, one for SCL, connected to 3.3V). Many sensor and display modules include these.
    * **Pull-up Resistor for Fan Tachometer:** 1x 10kΩ resistor connected between the fan's tachometer signal line and the ESP32's 3.3V supply.
    * *(Optional for Buttons):* If not using the ESP32's internal `INPUT_PULLUP` mode, you'll need pull-up or pull-down resistors (e.g., 5x 10kΩ) for each button.
* **Logic Level Shifter (for Fan PWM - Highly Recommended):**
    * To convert the ESP32's 3.3V logic level PWM signal to the 5V TTL signal expected by most PC fans. This ensures reliable fan speed control across a wider range of fans. A simple N-channel MOSFET (like BSS138) with two resistors (e.g., 10kΩ) per fan channel can be used.
* **Jumper/Switch (for Debug Mode):**
    * A simple SPST switch or a 2-pin header with a jumper to connect the `DEBUG_ENABLE_PIN` to 3.3V (to enable debug) or leave it pulled low (to disable).
* **Connectors:**
    * At least one 4-pin male PC fan header for connecting the fan to your controller.
    * Appropriate connectors for your 12V power input.
* **Prototyping Supplies:**
    * Breadboard(s).
    * Jumper Wires (male-male, male-female).

## 2.2. Recommended Hardware Stack & Component Details

For a higher quality, more robust, and aesthetically pleasing build:

* **Microcontroller:** **ESP32-WROOM-32E** or **ESP32-WROOM-32UE**.
    * *Reasoning:* These are current-generation modules with good performance, reliability, and community support. The "UE" variant offers a U.FL connector for an external antenna, which can be crucial if the controller is housed in a metal PC case that might attenuate the onboard antenna's WiFi signal.
* **Temperature Sensor (Optional):** **BMP280 (I2C)**.
    * *Reasoning:* Provides accurate temperature readings, is digitally interfaced (I2C), low power, and widely available at a low cost. The pressure data it also provides is not used by this project but doesn't hurt.
* **Display:** **0.96" or 1.3" I2C OLED Display (SSD1306 or SH1106 controller)**.
    * *Reasoning:* Offers significantly better readability, contrast, and viewing angles than standard 16x2 character LCDs. They can display more information graphically and contribute to a more "premium" feel. *Note: The current firmware uses the `LiquidCrystal_I2C` library. Switching to an OLED would require using a different display library (e.g., Adafruit SSD1306 or U8g2) and rewriting the display-related functions.*
* **Power Regulation:** **Integrated Buck Converter on Custom PCB** (using ICs like MP2307, MP1584EN, TPS54331) or a **high-quality MP1584EN-based module**.
    * *Reasoning:* Stable and clean power is vital for the ESP32 and sensor accuracy. Integrating the buck converter onto a custom PCB allows for better layout, component selection, and a more compact design. If using a module, avoid the cheapest unbranded LM2596 modules as their performance and ripple can be poor.
* **Fan PWM Logic Level Shifter:** **MOSFET-based (BSS138) or dedicated IC (e.g., 74AHCT125, 74LVC1T45)**.
    * *Reasoning:* Ensures the fan receives a proper 5V PWM signal, maximizing compatibility and control range. A BSS138 with two 10kΩ pull-up resistors (one to 3.3V on the ESP32 side, one to 5V on the fan side) is a common and effective single-channel solution.
* **Buttons:** **Good quality, PCB-mounted tactile switches**.
    * *Reasoning:* Provide reliable input and good tactile feedback. When paired with a custom enclosure and button caps, they contribute to a professional product feel.
* **Final Build:** **Custom PCB and a well-designed Enclosure**.
    * *Reasoning:* A PCB is non-negotiable for a reliable and presentable final product. An enclosure (e.g., 3D printed with PETG/ASA for temperature resistance, or a custom-machined/lasercut box) protects the electronics and greatly enhances the overall aesthetic and perceived value. Consider ventilation in the enclosure design.

## 2.3. Wiring Considerations (High-Level)

*(A detailed Fritzing diagram or schematic would be ideal here in a full project repository, but for text-based documentation, a description is provided.)*

* **ESP32 Power:**
    * 12V DC input connects to the VIN of your 12V-to-5V buck converter.
    * GND of the 12V input connects to the GND of the buck converter.
    * 5V output from the buck converter connects to the ESP32's VIN (or 5V) pin.
    * GND output from the buck converter connects to an ESP32 GND pin.
* **Fan(s):**
    * **GND (Pin 1):** Connect to the common ground of your circuit (shared with ESP32 GND).
    * **+12V (Pin 2):** Connect directly to your 12V power source (the same source feeding the buck converter).
    * **Tachometer/Sense (Pin 3):** Connect to the designated ESP32 GPIO (`FAN_TACH_PIN_ACTUAL`). A 10kΩ pull-up resistor should be connected between this GPIO pin and the ESP32's 3.3V line.
    * **Control/PWM (Pin 4):** Connect to the output of your 3.3V-to-5V logic level shifter. The input of the logic level shifter connects to the ESP32's `FAN_PWM_PIN` GPIO.
* **I2C Devices (LCD & BMP280):**
    * **SDA:** Connect ESP32's SDA pin (typically GPIO21) to the SDA pins of both the LCD and BMP280.
    * **SCL:** Connect ESP32's SCL pin (typically GPIO22) to the SCL pins of both the LCD and BMP280.
    * **VCC:** LCD VCC to 5V (from buck converter). BMP280 VCC to 3.3V (from ESP32's 3V3 pin).
    * **GND:** Connect to common ground.
    * **Pull-up Resistors:** If not already present on the modules, add 4.7kΩ-10kΩ pull-up resistors from SDA to 3.3V and SCL to 3.3V.
* **Buttons:**
    * Connect one terminal of each button to a dedicated ESP32 GPIO pin (e.g., `BTN_MENU_PIN`, `BTN_UP_PIN`, etc.).
    * Connect the other terminal of each button to GND.
    * The firmware will use the ESP32's internal pull-up resistors (`INPUT_PULLUP`).
* **Debug Enable Pin:**
    * Connect `DEBUG_ENABLE_PIN` to one terminal of a switch or jumper header.
    * Connect the other terminal of the switch/jumper header to 3.3V.
    * The pin is configured with `INPUT_PULLDOWN` in the firmware, so leaving it open or connected to GND will disable debug mode. Connecting it to 3.3V will enable debug mode.
* **Debug LED:**
    * Connect the anode (longer leg) of an LED to `LED_DEBUG_PIN` (GPIO2).
    * Connect the cathode (shorter leg) of the LED to a current-limiting resistor (e.g., 220Ω - 330Ω).
    * Connect the other end of the resistor to GND.

**Simplified Block Diagram Concept:**

                                     +-----------+
                                     |   12V DC  |
                                     |   Input   |
                                     +-----+-----+
                                           | (12V)
                                           |
                                 +---------v-------+     +--------+
                                 |  12V to 5V Buck |---->| PC Fan | (12V Power)
                                 |   Converter     |     | (12V)  |
                                 +---------+-------+     +----+---+
                                           | (5V)             | (Tach)
                                           |                  |
    +--------------------------------------v------------------v------------------------------------------+
    |                                                                                                     |
    |  +-----------+   (I2C)    +---------+      (I2C)     +-----------------+                            |
    |  |   LCD     |<---------->|  ESP32  |<-------------->| BMP280 Sensor   |                            |
    |  | (5V/3.3V) |            | (3.3V)  |<------(3.3V)---| (Optional)      |                            |
    |  +-----------+            +----+----+                +-----------------+                            |
    |                              | | | |                                                                |
    |  +-----------+  (GPIOs)      | | | `----(3.3V PWM)--->[Logic Shifter]--->(5V PWM)--+--------------+ |
    |  | Buttons   |<--------------' | `----(GPIO for Tach)-->(Pulled up to 3.3V)<-------| Fan Tach Pin | |
    |  | (5x)      |                 |                                                   +--------------+ |
    |  +-----------+                 `----(GPIO for Debug LED)------------------------->[Debug LED]       |
    |                                                                                                     |
    |  +---------------------+  (GPIO)                                                                    |
    |  | Debug Enable Switch |<---------------------------------------------------------------------------+
    |  +---------------------+
    +-----------------------------------------------------------------------------------------------------+

This detailed hardware setup provides a solid foundation for a reliable and feature-rich fan controller. Always double-check your specific ESP32 board's pinout diagram.

[Previous Chapter: Introduction](./01-introduction.md) | [Next Chapter: Software](./03-software.md)


