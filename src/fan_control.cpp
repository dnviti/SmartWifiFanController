#include "fan_control.h"
#include "config.h" // For global constants like PWM_CHANNEL, PWM_RESOLUTION_BITS, etc.

// Initialize the static instance pointer
FanController* FanController::_instance = nullptr;

// Private constructor implementation
FanController::FanController()
    : _isAutoMode(true),
      _manualFanSpeedPercentage(50),
      _currentTemperature(-999.0),
      _tempSensorFound(false),
      _fanRpm(0),
      _fanSpeedPercentage(0),
      _fanSpeedPWM_Raw(0),
      _pulseCount(0),
      _lastRpmReadTime_Task(0),
      _numCurvePoints(0),
      _fanCurveChanged(false),
      _needsImmediateBroadcast(false) {
    // Initialize curve arrays to 0 or default values
    for (int i = 0; i < MAX_CURVE_POINTS; ++i) {
        _tempPoints[i] = 0;
        _pwmPercentagePoints[i] = 0;
    }
    if(serialDebugEnabled) Serial.println("[FanController] Instance created.");
}

// Public static method to get the singleton instance
FanController& FanController::getInstance() {
    if (_instance == nullptr) {
        _instance = new FanController();
    }
    return *_instance;
}

void FanController::setDefaultCurve() {
    _numCurvePoints = 5;
    _tempPoints[0] = 25; _pwmPercentagePoints[0] = 0;  
    _tempPoints[1] = 35; _pwmPercentagePoints[1] = 20; 
    _tempPoints[2] = 45; _pwmPercentagePoints[2] = 50; 
    _tempPoints[3] = 55; _pwmPercentagePoints[3] = 80; 
    _tempPoints[4] = 60; _pwmPercentagePoints[4] = 100;
    _fanCurveChanged = true; // Signal that curve has changed
    if(serialDebugEnabled) Serial.println("[FanController] Default fan curve set.");
}

int FanController::calculateAutoPwm(float temp) {
    if (!_tempSensorFound) { 
        return AUTO_MODE_NO_SENSOR_FAN_PERCENTAGE;
    }
    if (_numCurvePoints == 0) return 0; 
    if (temp <= _tempPoints[0]) return _pwmPercentagePoints[0];
    if (temp >= _tempPoints[_numCurvePoints - 1]) return _pwmPercentagePoints[_numCurvePoints - 1];

    for (int i = 0; i < _numCurvePoints - 1; i++) {
        if (temp >= _tempPoints[i] && temp < _tempPoints[i+1]) {
            float tempRange = _tempPoints[i+1] - _tempPoints[i];
            float pwmRange = _pwmPercentagePoints[i+1] - _pwmPercentagePoints[i];
            if (tempRange <= 0) return _pwmPercentagePoints[i]; 
            float tempOffset = temp - _tempPoints[i];
            int calculatedPwm = _pwmPercentagePoints[i] + (tempOffset / tempRange) * pwmRange;
            return calculatedPwm;
        }
    }
    return _pwmPercentagePoints[_numCurvePoints - 1]; 
}

void FanController::setFanSpeed(int percentage) {
    int constrainedPercentage = constrain(percentage, 0, 100);
    if (_fanSpeedPercentage != constrainedPercentage) { // Only update if value changes
        _fanSpeedPercentage = constrainedPercentage;
        _fanSpeedPWM_Raw = map(_fanSpeedPercentage, 0, 100, 0, (1 << PWM_RESOLUTION_BITS) - 1);
        ledcWrite(PWM_CHANNEL, _fanSpeedPWM_Raw);
        _needsImmediateBroadcast = true; // Signal for web update
        // LCD update is handled by mainAppTask or displayMenu
    }
}

void FanController::setManualSpeedTarget(int percentage) {
    _isAutoMode = false;
    int constrainedPercentage = constrain(percentage, 0, 100);
    if (_manualFanSpeedPercentage != constrainedPercentage) {
        _manualFanSpeedPercentage = constrainedPercentage;
        _needsImmediateBroadcast = true; // Signal for web update
    }
}

void FanController::updateRpm(unsigned long currentTime) {
    noInterrupts(); 
    unsigned long currentPulses = _pulseCount;
    _pulseCount = 0; 
    interrupts(); 
    
    unsigned long elapsedMillis = currentTime - _lastRpmReadTime_Task;
    _lastRpmReadTime_Task = currentTime; 

    int newRpm = 0;
    if (elapsedMillis > 0 && PULSES_PER_REVOLUTION > 0) {
        newRpm = (currentPulses / (float)PULSES_PER_REVOLUTION) * (60000.0f / elapsedMillis);
    }
    if (newRpm != _fanRpm) {
        _fanRpm = newRpm;
        _needsImmediateBroadcast = true; // RPM changed
    }
}

void FanController::setTemperature(float temp, bool sensorFound) {
    if (_currentTemperature != temp || _tempSensorFound != sensorFound) {
        _currentTemperature = temp;
        _tempSensorFound = sensorFound;
        _needsImmediateBroadcast = true; // Temperature or sensor status changed
    }
}

void FanController::setFanCurve(int newNumPoints, const int newTempPoints[], const int newPwmPercentagePoints[]) {
    if (newNumPoints >= 0 && newNumPoints <= MAX_CURVE_POINTS) {
        _numCurvePoints = newNumPoints;
        for (int i = 0; i < _numCurvePoints; ++i) {
            _tempPoints[i] = newTempPoints[i];
            _pwmPercentagePoints[i] = newPwmPercentagePoints[i];
        }
        _fanCurveChanged = true; // Signal that curve has changed
        _needsImmediateBroadcast = true; // Signal for web update
    }
}

// ISR implementation (static method)
void IRAM_ATTR FanController::countPulseISR() {
    // Check if the singleton instance has been created
    if (FanController::_instance) {
        FanController::_instance->incrementPulseCount();
    }
}

// Non-static method to increment pulse count
void FanController::incrementPulseCount() {
    _pulseCount++;
}
