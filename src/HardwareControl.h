#ifndef HARDWARE_CONTROL_H
#define HARDWARE_CONTROL_H

#include <Arduino.h>

void initHardware();
void setPumpA(bool state);
void setPumpB(bool state);
// Control de velocidad vía PWM (0-100%, ver Config.h PUMP_PWM_*). setPumpA/B(bool) internamente
// llaman a estas con 100/0 - usar estas directamente para riego a velocidad parcial.
void setPumpASpeed(uint8_t speedPercent);
void setPumpBSpeed(uint8_t speedPercent);
// Último duty comandado (0-100), para que la UI de control manual muestre/ajuste la
// velocidad actual sin necesitar su propio estado duplicado.
uint8_t getPumpASpeed();
uint8_t getPumpBSpeed();
bool readLevelSensor();
float readBatteryVoltage();

// Detección indirecta de tanque vacío por corriente de succión (ver Config.h y
// docs/hardware/PUMP_DRY_RUN_DETECTION.md). Llamar updatePumpDryRunDetection() en cada
// loop() con el estado comandado de cada bomba; isPumpADry()/isPumpBDry() reflejan si esa
// bomba lleva PUMP_DRY_CONFIRM_MS por debajo del umbral de corriente (bombeando en seco).
float readPumpACurrentMA();
float readPumpBCurrentMA();
void updatePumpDryRunDetection(bool pumpAOn, bool pumpBOn);
bool isPumpADry();
bool isPumpBDry();

#endif
