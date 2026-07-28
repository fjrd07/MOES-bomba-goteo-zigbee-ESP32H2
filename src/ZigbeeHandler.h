#ifndef ZIGBEE_HANDLER_H
#define ZIGBEE_HANDLER_H

#include <Arduino.h>

// Endpoints Zigbee del dispositivo (deben coincidir con Z2M/Bomba-Riego-Goteo-Z2M.js
// -> deviceEndpoints({ '1': 1, '2': 2, '3': 3, '4': 4 })). Los valores reales viven en
// zb_lib/zb_scheduler_lib.h (ZB_EP_*) - este archivo los re-expone con el nombre histórico
// para no romper el resto del proyecto.
#define ZB_ENDPOINT_PUMP_A   1
#define ZB_ENDPOINT_PUMP_B   2
#define ZB_ENDPOINT_SENSORS  3
#define ZB_ENDPOINT_CONFIG   4

void initZigbee();
void loopZigbee();
void sendAlert(const char* message);
void reportLevelStatus(bool isLow);

#endif
