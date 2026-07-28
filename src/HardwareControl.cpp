#include "HardwareControl.h"
#include "Config.h"
#include <math.h>

void initHardware() {
    // PIN_PUMP_A/B se manejan vía LEDC (PWM) para soportar velocidad variable - ledcAttach
    // configura el pin como salida internamente, no hace falta pinMode() para estos dos.
    ledcAttach(PIN_PUMP_A, PUMP_PWM_FREQ_HZ, PUMP_PWM_RESOLUTION);
    ledcAttach(PIN_PUMP_B, PUMP_PWM_FREQ_HZ, PUMP_PWM_RESOLUTION);
    ledcWrite(PIN_PUMP_A, 0);
    ledcWrite(PIN_PUMP_B, 0);

    pinMode(PIN_LEVEL_SENS, INPUT_PULLUP);
    pinMode(PIN_BATTERY, INPUT);
    pinMode(PIN_PUMP_A_ISENSE, INPUT);
    pinMode(PIN_PUMP_B_ISENSE, INPUT);
}

static uint8_t lastSpeedA = 0;
static uint8_t lastSpeedB = 0;

void setPumpASpeed(uint8_t speedPercent) {
    speedPercent = constrain(speedPercent, 0, 100);
    uint32_t maxDuty = (1UL << PUMP_PWM_RESOLUTION) - 1;
    ledcWrite(PIN_PUMP_A, (speedPercent * maxDuty) / 100);
    lastSpeedA = speedPercent;
}

void setPumpBSpeed(uint8_t speedPercent) {
    speedPercent = constrain(speedPercent, 0, 100);
    uint32_t maxDuty = (1UL << PUMP_PWM_RESOLUTION) - 1;
    ledcWrite(PIN_PUMP_B, (speedPercent * maxDuty) / 100);
    lastSpeedB = speedPercent;
}

uint8_t getPumpASpeed() { return lastSpeedA; }
uint8_t getPumpBSpeed() { return lastSpeedB; }

void setPumpA(bool state) {
    setPumpASpeed(state ? 100 : 0);
}

void setPumpB(bool state) {
    setPumpBSpeed(state ? 100 : 0);
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

// --- Detección indirecta de tanque vacío por corriente de succión ---
// Ver Config.h para las constantes de calibración y docs/hardware/PUMP_DRY_RUN_DETECTION.md
// para el porqué de la técnica y el cableado del sensor de corriente.

struct PumpDryRunState {
    bool isOn = false;
    unsigned long turnedOnAt = 0;
    unsigned long belowThresholdSinceMs = 0; // 0 = actualmente no está por debajo del umbral
    bool dryDetected = false;
};

static PumpDryRunState pumpAState;
static PumpDryRunState pumpBState;

static float rawToMilliAmps(int adcRaw) {
    // ESP32-H2 ADC de 12 bits, referencia ~3.3V (ajustar si usas atenuación distinta)
    float mv = (adcRaw / 4095.0f) * 3300.0f;
    return (mv - CURRENT_SENSOR_ZERO_MV) / CURRENT_SENSOR_MV_PER_AMP * 1000.0f;
}

float readPumpACurrentMA() {
    return fabsf(rawToMilliAmps(analogRead(PIN_PUMP_A_ISENSE)));
}

float readPumpBCurrentMA() {
    return fabsf(rawToMilliAmps(analogRead(PIN_PUMP_B_ISENSE)));
}

static void updateDryRunState(PumpDryRunState &state, bool pumpCommandedOn, float currentMA) {
    unsigned long now = millis();

    if (pumpCommandedOn && !state.isOn) {
        // Transición OFF -> ON: reiniciar estado
        state.isOn = true;
        state.turnedOnAt = now;
        state.belowThresholdSinceMs = 0;
        state.dryDetected = false;
    } else if (!pumpCommandedOn && state.isOn) {
        // Transición ON -> OFF: sin succión no hay nada que medir
        state.isOn = false;
        state.belowThresholdSinceMs = 0;
        state.dryDetected = false;
    }

    if (!state.isOn) return;
    if ((now - state.turnedOnAt) < PUMP_STARTUP_IGNORE_MS) return; // ignora el pico de arranque

    if (currentMA < PUMP_DRY_CURRENT_THRESHOLD_MA) {
        if (state.belowThresholdSinceMs == 0) {
            state.belowThresholdSinceMs = now;
        } else if ((now - state.belowThresholdSinceMs) > PUMP_DRY_CONFIRM_MS) {
            state.dryDetected = true;
        }
    } else {
        state.belowThresholdSinceMs = 0;
        state.dryDetected = false;
    }
}

void updatePumpDryRunDetection(bool pumpAOn, bool pumpBOn) {
    updateDryRunState(pumpAState, pumpAOn, readPumpACurrentMA());
    updateDryRunState(pumpBState, pumpBOn, readPumpBCurrentMA());
}

bool isPumpADry() { return pumpAState.dryDetected; }
bool isPumpBDry() { return pumpBState.dryDetected; }
