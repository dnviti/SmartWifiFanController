#include "fan_control.h"
#include "config.h" // For global variables

/**
 * @brief Apply the default temperature-to-PWM fan curve.
 *
 * This function initializes the fan control curve with a predefined set of
 * temperature and corresponding PWM percentage points. It is typically called
 * during setup or when a user wants to revert to default settings.
 *
 * Side-effects:
 * - Modifies the global `numCurvePoints` variable to reflect the number of points in the default curve.
 * - Populates the global `tempPoints[]` array with default temperature setpoints.
 * - Populates the global `pwmPercentagePoints[]` array with corresponding default PWM percentages.
 * - If `serialDebugEnabled` is true, it prints a confirmation message to the Serial console.
 *
 * Global dependencies:
 * - `numCurvePoints` (writable global variable)
 * - `tempPoints[]` (writable global array)
 * - `pwmPercentagePoints[]` (writable global array)
 * - `serialDebugEnabled` (readable global variable)
 * - `Serial` (global object, used for logging if `serialDebugEnabled` is true)
 */
void setDefaultFanCurve() {
    numCurvePoints = 5;
    tempPoints[0] = 25; pwmPercentagePoints[0] = 0;  
    tempPoints[1] = 35; pwmPercentagePoints[1] = 20; 
    tempPoints[2] = 45; pwmPercentagePoints[2] = 50; 
    tempPoints[3] = 55; pwmPercentagePoints[3] = 80; 
    tempPoints[4] = 60; pwmPercentagePoints[4] = 100;
    if(serialDebugEnabled) Serial.println("[SYSTEM] Default fan curve set.");
}

/**
 * @brief Calculates the fan PWM percentage based on the current temperature and the defined fan curve.
 *
 * This function determines the appropriate fan speed (as a PWM percentage) by
 * interpolating between points defined in the `tempPoints[]` and `pwmPercentagePoints[]` arrays.
 *
 * @param temp The current temperature reading in Celsius.
 *
 * @return int The calculated fan PWM percentage (0-100).
 *   - Returns `AUTO_MODE_NO_SENSOR_FAN_PERCENTAGE` if the temperature sensor is not found (`tempSensorFound` is false).
 *   - Returns 0 if no fan curve points are defined (`numCurvePoints` is 0).
 *   - Returns the PWM value corresponding to the first curve point if `temp` is at or below the first temperature point.
 *   - Returns the PWM value corresponding to the last curve point if `temp` is at or above the last temperature point.
 *   - Otherwise, returns a linearly interpolated PWM value based on the segment of the fan curve that `temp` falls into.
 *
 * Global dependencies:
 * - `tempSensorFound` (readable global variable)
 * - `AUTO_MODE_NO_SENSOR_FAN_PERCENTAGE` (readable global constant)
 * - `numCurvePoints` (readable global variable)
 * - `tempPoints[]` (readable global array containing temperature setpoints for the fan curve)
 * - `pwmPercentagePoints[]` (readable global array containing PWM percentages corresponding to `tempPoints[]`)
 */
int calculateAutoFanPWMPercentage(float temp) {
    if (!tempSensorFound) { 
        return AUTO_MODE_NO_SENSOR_FAN_PERCENTAGE;
    }
    if (numCurvePoints == 0) return 0; 
    if (temp <= tempPoints[0]) return pwmPercentagePoints[0];
    if (temp >= tempPoints[numCurvePoints - 1]) return pwmPercentagePoints[numCurvePoints - 1];

    for (int i = 0; i < numCurvePoints - 1; i++) {
        if (temp >= tempPoints[i] && temp < tempPoints[i+1]) {
            float tempRange = tempPoints[i+1] - tempPoints[i];
            float pwmRange = pwmPercentagePoints[i+1] - pwmPercentagePoints[i];
            if (tempRange <= 0) return pwmPercentagePoints[i]; // Avoid division by zero or negative range
            float tempOffset = temp - tempPoints[i];
            int calculatedPwm = pwmPercentagePoints[i] + (tempOffset / tempRange) * pwmRange;
            return calculatedPwm;
        }
    }
    return pwmPercentagePoints[numCurvePoints - 1]; // Should be covered by earlier checks, but as a fallback
}

/**
 * @brief Sets the fan speed to a given percentage.
 *
 * This function takes a desired fan speed percentage, constrains it to the valid
 * range (0-100), maps it to a raw PWM value based on the configured PWM resolution,
 * and then applies this value to the fan's PWM channel. It also signals that
 * a state change has occurred, which might trigger updates elsewhere (e.g., web UI).
 *
 * @param percentage The desired fan speed as a percentage (0-100). Values outside this range will be constrained.
 *
 * Side-effects:
 * - Updates the global `fanSpeedPercentage` variable with the constrained percentage.
 * - Calculates and updates the global `fanSpeedPWM_Raw` variable.
 * - Sets the fan's PWM duty cycle by calling `ledcWrite` on `PWM_CHANNEL`.
 * - Sets the global `needsImmediateBroadcast` flag to `true`.
 *
 * Global dependencies:
 * - `fanSpeedPercentage` (writable global variable)
 * - `fanSpeedPWM_Raw` (writable global variable)
 * - `PWM_RESOLUTION_BITS` (readable global constant, defines the bit resolution for PWM)
 * - `PWM_CHANNEL` (readable global constant, defines the LEDC channel used for fan PWM)
 * - `needsImmediateBroadcast` (writable global variable)
 * - `constrain()` (Arduino core function, used to limit the percentage)
 * - `map()` (Arduino core function, used to scale percentage to PWM raw value)
 * - `ledcWrite()` (ESP-IDF/Arduino LEDC API function, used to set PWM duty cycle)
 */
void setFanSpeed(int percentage) {
    fanSpeedPercentage = constrain(percentage, 0, 100);
    fanSpeedPWM_Raw = map(fanSpeedPercentage, 0, 100, 0, (1 << PWM_RESOLUTION_BITS) - 1);
    ledcWrite(PWM_CHANNEL, fanSpeedPWM_Raw);
    needsImmediateBroadcast = true; // Signal for web update
    // LCD update is handled by mainAppTask or displayMenu
}

/**
 * @brief Interrupt Service Routine (ISR) for the fan tachometer.
 *
 * This function is called on each pulse detected from the fan's tachometer output.
 * It increments a counter used to calculate the fan's RPM.
 *
 * @note This function is an ISR and is marked with `IRAM_ATTR` to ensure it is
 *       loaded into IRAM for faster execution. ISRs should be kept as short and
 *       simple as possible. Avoid calling functions that are not ISR-safe
 *       (e.g., most `Serial` operations or functions involving delays or complex memory allocation).
 *
 * Side-effects:
 * - Increments the global `pulseCount` variable. This variable should be declared `volatile`.
 *
 * Global dependencies:
 * - `pulseCount` (writable global volatile variable)
 */
void IRAM_ATTR countPulse() {
  pulseCount++;
}
