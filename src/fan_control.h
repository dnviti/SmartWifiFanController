#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include "config.h"

// Define the number of points in the default fan curve set by setDefaultFanCurve()
// This value is used for static assertion against MAX_CURVE_POINTS.
constexpr int DEFAULT_CURVE_POINT_COUNT = 5;

void setDefaultFanCurve();
int calculateAutoFanPWMPercentage(float temp);
void setFanSpeed(int percentage);
void IRAM_ATTR countPulse(); // ISR for tachometer

#endif // FAN_CONTROL_H
