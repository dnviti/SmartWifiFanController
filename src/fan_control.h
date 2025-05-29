#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include "config.h" // For MAX_CURVE_POINTS, Arduino.h, etc.

class FanController {
public:
    FanController(); // Default constructor, will use constants from config.h for pins/PWM

    // Call this in setup()
    void begin();

    void setDefaultCurve();
    int computeAutoPwm(float currentTemperature); // Uses internal sensorFound_m status
    void setTargetFanSpeedPercentage(int percentage);

    int getCurrentFanSpeedPercentage() const;
    int getCurrentFanSpeedPWM_Raw() const;

    // Gets the current fan curve. outTempPoints and outPwmPoints must be arrays of at least MAX_CURVE_POINTS size.
    void getFanCurve(int& numPoints, int outTempPoints[], int outPwmPoints[]) const;
    // Sets a new fan curve. Returns true on success, false on validation failure.
    bool setFanCurve(int numPoints, const int newTempPoints[], const int newPwmPoints[]);

    void setSensorStatus(bool found);
    bool getSensorStatus() const;

    unsigned long getAndResetPulseCount(); // Atomically gets and resets pulse count for RPM calculation

    // ISR related
    static void IRAM_ATTR staticCountPulse(); // Static ISR method

private:
    int tempPoints_m[MAX_CURVE_POINTS];
    int pwmPercentagePoints_m[MAX_CURVE_POINTS];
    int numCurvePoints_m;

    int currentFanSpeedPercentage_m;
    int currentFanSpeedPWM_Raw_m;
    bool sensorFound_m;

    volatile unsigned long pulseCountISR_m;

    // Configuration (initialized by constructor/begin)
    int pwmPin_m;
    int tachPin_m;
    int pwmChannel_m;
    int pwmFreq_m;
    int pwmResolutionBits_m;
    int autoModeNoSensorFanPercentage_m;

    static FanController* instance_ptr; // Pointer to the singleton instance for ISR
};

// Global fan controller object, will be defined in main.cpp
extern FanController fanController;

#endif // FAN_CONTROL_H
