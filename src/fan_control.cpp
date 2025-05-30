#include "fan_control.h"
#include "config.h" // For global variables

/**
 * @brief Applies the default temperature-to-PWM fan curve.
 *
 * This function initializes the global `tempPoints` and `pwmPercentagePoints` arrays
 * with a predefined set of temperature-to-PWM mappings, and sets `numCurvePoints`
 * to reflect the number of points defined.
 *
 * @note This function does not save the curve to NVS; `saveFanCurveToNVS()` must be called separately.
 * @sideeffect Modifies global arrays `tempPoints[]` and `pwmPercentagePoints[]`.
 * @sideeffect Sets global variable `numCurvePoints`.
 * @sideeffect Prints debug information to Serial if `serialDebugEnabled` is true.
 * @global `numCurvePoints`: Updated to 5.
 * @global `tempPoints[]`: Initialized with default temperature values.
 * @global `pwmPercentagePoints[]`: Initialized with default PWM percentage values.
 * @global `serialDebugEnabled`: Used to conditionally enable debug output.
 * @global `Serial`: Used for debug output.
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
 * @brief Calculates the target fan PWM percentage based on current temperature and the defined fan curve.
 *
 * This function interpolates the fan speed percentage from the `tempPoints` and `pwmPercentagePoints`
 * arrays based on the provided `temp`.
 *
 * If `tempSensorFound` is false, it returns a fixed default percentage.
 * If `numCurvePoints` is zero, it returns 0.
 * If the temperature is below the first point, it returns the PWM of the first point.
 * If the temperature is above the last point, it returns the PWM of the last point.
 * For temperatures between points, linear interpolation is used.
 *
 * @param temp The current temperature in Celsius.
 * @return int The calculated fan speed as a percentage (0-100).
 * @global `tempSensorFound`: Checked to determine if a temperature sensor is available.
 * @global `AUTO_MODE_NO_SENSOR_FAN_PERCENTAGE`: Returned if no temperature sensor is found.
 * @global `numCurvePoints`: Used to determine the number of points in the fan curve.
 * @global `tempPoints[]`: Array of temperature points defining the fan curve.
 * @global `pwmPercentagePoints[]`: Array of PWM percentage points defining the fan curve.
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
            if (tempRange <= 0) return pwmPercentagePoints[i]; 
            float tempOffset = temp - tempPoints[i];
            int calculatedPwm = pwmPercentagePoints[i] + (tempOffset / tempRange) * pwmRange;
            return calculatedPwm;
        }
    }
    return pwmPercentagePoints[numCurvePoints - 1]; 
}

/**
 * @brief Sets the fan speed to a specified percentage.
 *
 * This function constrains the input `percentage` to be between 0 and 100,
 * converts it to a raw PWM duty cycle value based on `PWM_RESOLUTION_BITS`,
 * and applies it to the fan's PWM pin using `ledcWrite`.
 * It also signals that a broadcast update is needed for web/MQTT clients.
 *
 * @param percentage The desired fan speed as a percentage (0-100).
 * @sideeffect Modifies global variable `fanSpeedPercentage` to the constrained value.
 * @sideeffect Modifies global variable `fanSpeedPWM_Raw` to the calculated raw PWM duty cycle.
 * @sideeffect Controls the fan hardware via `ledcWrite` on `PWM_CHANNEL`.
 * @sideeffect Sets global boolean `needsImmediateBroadcast` to true.
 * @global `fanSpeedPercentage`: Stores the currently set fan speed percentage.
 * @global `fanSpeedPWM_Raw`: Stores the raw PWM duty cycle value.
 * @global `PWM_CHANNEL`: The LEDC channel used for fan PWM output.
 * @global `PWM_RESOLUTION_BITS`: The resolution of the PWM signal, used for mapping.
 * @global `needsImmediateBroadcast`: Flag to signal web/MQTT updates.
 * @dependency `constrain()`: Arduino function to limit a value to a range.
 * @dependency `map()`: Arduino function to remap a number from one range to another.
 * @dependency `ledcWrite()`: ESP32 LEDC API to set PWM duty cycle.
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
 * This function is called on each falling edge detected on the fan tachometer pin.
 * It increments a global counter (`pulseCount`) which is later used to calculate fan RPM.
 *
 * @note This function is marked `IRAM_ATTR` to ensure it resides in IRAM for fast execution,
 *       which is critical for ISRs.
 * @param None (ISR functions do not take parameters).
 * @return None (ISR functions do not return values).
 * @sideeffect Increments global variable `pulseCount`.
 * @global `pulseCount`: Counter for tachometer pulses.
 */
void IRAM_ATTR countPulse() {
  pulseCount++;
}
