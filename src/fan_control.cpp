#include "fan_control.h"
#include "config.h" // For global variables like numCurvePoints, tempPoints, MAX_CURVE_POINTS

// Static assertion to ensure MAX_CURVE_POINTS (from config.h) is sufficient 
// for the default curve points (DEFAULT_CURVE_POINT_COUNT from fan_control.h).
// This prevents buffer overflows if MAX_CURVE_POINTS is set too low.
static_assert(MAX_CURVE_POINTS >= DEFAULT_CURVE_POINT_COUNT, 
              "MAX_CURVE_POINTS in config.h is too small for the default fan curve. "
              "It must be >= DEFAULT_CURVE_POINT_COUNT defined in fan_control.h. "
              "Please increase MAX_CURVE_POINTS in config.h.");

void setDefaultFanCurve() {
    numCurvePoints = DEFAULT_CURVE_POINT_COUNT; // Use the defined constant for number of points
    
    // These assignments write to indices 0 through 4, requiring 5 points.
    // This is safe because DEFAULT_CURVE_POINT_COUNT is 5, and the static_assert 
    // ensures MAX_CURVE_POINTS is at least 5.
    tempPoints[0] = 25; pwmPercentagePoints[0] = 0;  
    tempPoints[1] = 35; pwmPercentagePoints[1] = 20; 
    tempPoints[2] = 45; pwmPercentagePoints[2] = 50; 
    tempPoints[3] = 55; pwmPercentagePoints[3] = 80; 
    tempPoints[4] = 60; pwmPercentagePoints[4] = 100;
    
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
