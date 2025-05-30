#include "fan_control.h"
#include "config.h" // For global variables

void setDefaultFanCurve() {
    Config::numCurvePoints = 5;
    Config::tempPoints[0] = 25; Config::pwmPercentagePoints[0] = 0;  
    Config::tempPoints[1] = 35; Config::pwmPercentagePoints[1] = 20; 
    Config::tempPoints[2] = 45; Config::pwmPercentagePoints[2] = 50; 
    Config::tempPoints[3] = 55; Config::pwmPercentagePoints[3] = 80; 
    Config::tempPoints[4] = 60; Config::pwmPercentagePoints[4] = 100;
    if(Config::serialDebugEnabled) Serial.println("[SYSTEM] Default fan curve set.");
}

int calculateAutoFanPWMPercentage(float temp) {
    if (!Config::tempSensorFound) { 
        return Config::AUTO_MODE_NO_SENSOR_FAN_PERCENTAGE;
    }
    if (Config::numCurvePoints == 0) return 0; 
    if (temp <= Config::tempPoints[0]) return Config::pwmPercentagePoints[0];
    if (temp >= Config::tempPoints[Config::numCurvePoints - 1]) return Config::pwmPercentagePoints[Config::numCurvePoints - 1];

    for (int i = 0; i < Config::numCurvePoints - 1; i++) {
        if (temp >= Config::tempPoints[i] && temp < Config::tempPoints[i+1]) {
            float tempRange = Config::tempPoints[i+1] - Config::tempPoints[i];
            float pwmRange = Config::pwmPercentagePoints[i+1] - Config::pwmPercentagePoints[i];
            if (tempRange <= 0) return Config::pwmPercentagePoints[i]; 
            float tempOffset = temp - Config::tempPoints[i];
            int calculatedPwm = Config::pwmPercentagePoints[i] + (tempOffset / tempRange) * pwmRange;
            return calculatedPwm;
        }
    }
    return Config::pwmPercentagePoints[Config::numCurvePoints - 1]; 
}

void setFanSpeed(int percentage) {
    Config::fanSpeedPercentage = constrain(percentage, 0, 100);
    Config::fanSpeedPWM_Raw = map(Config::fanSpeedPercentage, 0, 100, 0, (1 << Config::PWM_RESOLUTION_BITS) - 1);
    ledcWrite(Config::PWM_CHANNEL, Config::fanSpeedPWM_Raw);
    Config::needsImmediateBroadcast = true; // Signal for web update
    // LCD update is handled by mainAppTask or displayMenu
}

void IRAM_ATTR countPulse() {
  Config::pulseCount++;
}
