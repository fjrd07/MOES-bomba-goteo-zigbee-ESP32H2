#ifndef HARDWARE_CONTROL_H
#define HARDWARE_CONTROL_H

#include <Arduino.h>

void initHardware();
void setPumpA(bool state);
void setPumpB(bool state);
bool readLevelSensor();
float readBatteryVoltage();

#endif
