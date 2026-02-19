#include "HardwareControl.h"
#include "Config.h"

void initHardware() {
    pinMode(PIN_PUMP_A, OUTPUT);
    pinMode(PIN_PUMP_B, OUTPUT);
    pinMode(PIN_LEVEL_SENS, INPUT_PULLUP);
    pinMode(PIN_BATTERY, INPUT);
    
    // Default safe state
    digitalWrite(PIN_PUMP_A, LOW);
    digitalWrite(PIN_PUMP_B, LOW);
}

void setPumpA(bool state) {
    digitalWrite(PIN_PUMP_A, state ? HIGH : LOW);
}

void setPumpB(bool state) {
    digitalWrite(PIN_PUMP_B, state ? HIGH : LOW);
}

bool readLevelSensor() {
    return digitalRead(PIN_LEVEL_SENS) == NIVEL_VACIO; // Returns true if EMPTY
}

float readBatteryVoltage() {
    int raw = analogRead(PIN_BATTERY);
    // Assuming voltage divider details not fully specified, return raw or calculate roughly for 18650
    // Vout = Vin * (R2 / (R1 + R2)). Revert calculation.
    return (raw / 4095.0) * 3.3 * 2; // Rough estimate assuming 1:1 divider
}
