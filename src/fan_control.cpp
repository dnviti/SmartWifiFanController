#include "fan_control.h"
#include "config.h" // For global variables and MAX_CURVE_POINTS, DEFAULT_CURVE_POINT_COUNT

// Define the default curve points
// These arrays are sized using DEFAULT_CURVE_POINT_COUNT defined in config.h
static const int DEFAULT_TEMP_POINTS[DEFAULT_CURVE_POINT_COUNT] = {25, 35, 45, 55, 60};
static const int DEFAULT_PWM_POINTS[DEFAULT_CURVE_POINT_COUNT] = {0, 20, 50, 80, 100};

void setDefaultFanCurve() {
    // The static_assert in config.h already ensures DEFAULT_CURVE_POINT_COUNT <= MAX_CURVE_POINTS at compile time.
    // This runtime check is included as a defensive measure, as suggested in the issue,
    // although it should ideally be unreachable if the static_assert passes.
    if (DEFAULT_CURVE_POINT_COUNT > MAX_CURVE_POINTS) {
        if(serialDebugEnabled) Serial.println("[SYSTEM_ERR] Default fan curve definition exceeds MAX_CURVE_POINTS! Truncating.");
        numCurvePoints = MAX_CURVE_POINTS; // Clamp to max capacity to prevent overflow
    } else {
        numCurvePoints = DEFAULT_CURVE_POINT_COUNT;
    }

    // Populate the fan curve arrays using the default points
    for (int i = 0; i < numCurvePoints; ++i) {
        tempPoints[i] = DEFAULT_TEMP_POINTS[i];
        pwmPercentagePoints[i] = DEFAULT_PWM_POINTS[i];
    }
    if(serialDebugEnabled) Serial.println("[SYSTEM] Default fan curve set.");
}

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
