#include "fan_control.h"
#include "config.h" // For global variables

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
 * @brief Calculates the fan PWM percentage based on the current temperature using linear interpolation.
 * 
 * This function assumes that the `tempPoints` array, which defines the fan curve,
 * is strictly increasing. This precondition is enforced by all functions that
 * modify the fan curve (e.g., `setDefaultFanCurve`, `loadFanCurveFromNVS`,
 * `handleSerialCommands` for staging, and `webSocketEvent` for web UI input).
 * 
 * If the temperature falls below the first point, the PWM of the first point is returned.
 * If the temperature falls above the last point, the PWM of the last point is returned.
 * For temperatures between points, linear interpolation is used.
 * 
 * @param temp The current temperature in Celsius.
 * @return The calculated fan PWM percentage (0-100).
 */
int calculateAutoFanPWMPercentage(float temp) {
    if (!tempSensorFound) { 
        return AUTO_MODE_NO_SENSOR_FAN_PERCENTAGE;
    }
    if (numCurvePoints == 0) return 0; 
    if (temp <= tempPoints[0]) return pwmPercentagePoints[0];
    if (temp >= tempPoints[numCurvePoints - 1]) return pwmPercentagePoints[numCurvePoints - 1];

    for (int i = 0; i < numCurvePoints - 1; i++) {
        // Find the segment where temp falls: [tempPoints[i], tempPoints[i+1])
        // The fan curve setting functions ensure tempPoints is strictly increasing,
        // so tempPoints[i+1] > tempPoints[i] should always hold.
        if (temp >= tempPoints[i] && temp < tempPoints[i+1]) {
            float tempRange = tempPoints[i+1] - tempPoints[i];
            float pwmRange = pwmPercentagePoints[i+1] - pwmPercentagePoints[i];
            
            // This check acts as a safeguard against invalid or identical temperature points,
            // though ideally, it should not be triggered if the curve is properly validated on input.
            if (tempRange <= 0) return pwmPercentagePoints[i]; 
            
            float tempOffset = temp - tempPoints[i];
            int calculatedPwm = pwmPercentagePoints[i] + (tempOffset / tempRange) * pwmRange;
            return calculatedPwm;
        }
    }
    // If temp is exactly on the last point, or slightly above but not caught by the `temp >= tempPoints[numCurvePoints - 1]` check
    // (e.g., due to float precision, or if the loop condition `temp < tempPoints[i+1]` was not met for the last segment),
    // return the last point's PWM. This acts as a final fallback.
    return pwmPercentagePoints[numCurvePoints - 1]; 
}

void setFanSpeed(int percentage) {
    fanSpeedPercentage = constrain(percentage, 0, 100);
    fanSpeedPWM_Raw = map(fanSpeedPercentage, 0, 100, 0, (1 << PWM_RESOLUTION_BITS) - 1);
    ledcWrite(PWM_CHANNEL, fanSpeedPWM_Raw);
    needsImmediateBroadcast = true; // Signal for web update
    // LCD update is handled by mainAppTask or displayMenu
}

void IRAM_ATTR countPulse() {
  pulseCount++;
}
