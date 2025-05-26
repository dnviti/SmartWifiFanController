#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include "config.h"

void setDefaultFanCurve();
int calculateAutoFanPWMPercentage(float temp);
void setFanSpeed(int percentage);
void IRAM_ATTR countPulse(); // ISR for tachometer

#endif // FAN_CONTROL_H
