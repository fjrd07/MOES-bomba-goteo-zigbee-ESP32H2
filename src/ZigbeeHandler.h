#ifndef ZIGBEE_HANDLER_H
#define ZIGBEE_HANDLER_H

#include <Arduino.h>

void initZigbee();
void loopZigbee();
void sendAlert(const char* message);
void reportLevelStatus(bool isLow);

#endif
