#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include "config.h" // For constants like PWM_CHANNEL, MAX_CURVE_POINTS, etc.

// Forward declaration of Adafruit_BMP280 if needed for a method signature,
// but it's better if FanController doesn't directly interact with the sensor hardware.
// Instead, the main application task should read the sensor and pass the data to FanController.

class FanController {
private:
    // Singleton instance
    static FanController* _instance;

    // Fan Control State Variables
    volatile bool _isAutoMode;
    volatile int _manualFanSpeedPercentage;
    volatile float _currentTemperature;
    volatile bool _tempSensorFound;
    volatile int _fanRpm;
    volatile int _fanSpeedPercentage;
    volatile int _fanSpeedPWM_Raw;
    volatile unsigned long _pulseCount;
    unsigned long _lastRpmReadTime_Task;

    // Fan Curve
    int _tempPoints[MAX_CURVE_POINTS];
    int _pwmPercentagePoints[MAX_CURVE_POINTS];
    int _numCurvePoints;
    volatile bool _fanCurveChanged;

    // Communication/Broadcast Flag
    volatile bool _needsImmediateBroadcast;

    // Private constructor for singleton pattern
    FanController();

public:
    // Delete copy constructor and assignment operator to prevent cloning
    FanController(const FanController&) = delete;
    FanController& operator=(const FanController&) = delete;

    // Public static method to get the singleton instance
    static FanController& getInstance();

    // Public methods for fan control logic
    void setDefaultCurve();
    int calculateAutoPwm(float temp);
    void setFanSpeed(int percentage); // Applies PWM, updates _fanSpeedPercentage, _fanSpeedPWM_Raw
    void setManualSpeedTarget(int percentage); // Sets _manualFanSpeedPercentage and _isAutoMode
    void updateRpm(unsigned long currentTime); // Calculates RPM based on pulseCount

    // ISR for tachometer (static to be attachable to interrupt)
    static void IRAM_ATTR countPulseISR();
    void incrementPulseCount(); // Non-static method called by ISR

    // Methods to update sensor data from external sources (e.g., mainAppTask)
    void setTemperature(float temp, bool sensorFound);

    // Methods to get/set fan curve data
    const int* getTempPoints() const { return _tempPoints; }
    const int* getPwmPercentagePoints() const { return _pwmPercentagePoints; }
    int getNumCurvePoints() const { return _numCurvePoints; }
    void setFanCurve(int newNumPoints, const int newTempPoints[], const int newPwmPercentagePoints[]);

    // Getters for state variables
    bool isAutoMode() const { return _isAutoMode; }
    int getManualFanSpeedPercentage() const { return _manualFanSpeedPercentage; }
    float getCurrentTemperature() const { return _currentTemperature; }
    bool isTempSensorFound() const { return _tempSensorFound; }
    int getFanRpm() const { return _fanRpm; }
    int getFanSpeedPercentage() const { return _fanSpeedPercentage; }
    bool getNeedsImmediateBroadcast() const { return _needsImmediateBroadcast; }
    void setNeedsImmediateBroadcast(bool state) { _needsImmediateBroadcast = state; }
    bool getFanCurveChanged() const { return _fanCurveChanged; }
    void setFanCurveChanged(bool state) { _fanCurveChanged = state; }
};

#endif // FAN_CONTROL_H
