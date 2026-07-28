#ifndef SCHEDULE_MANAGER_H
#define SCHEDULE_MANAGER_H

#include <Arduino.h>

// Programador de riego: calendario combinado diario+semanal+mensual+anual por bomba, con
// nombres de bomba personalizables y persistencia en NVS. Ver
// docs/hardware/IRRIGATION_SCHEDULING.md para el modelo de datos, el esquema JSON expuesto
// vía Zigbee, y las limitaciones de sincronización horaria (el ESP32-H2 no tiene RTC con
// batería - ver esa misma nota en Config.h DEFAULT_TZ_POSIX).

void initScheduleManager();    // Llamar una vez en setup(), después de initHardware()
void updateScheduleManager();  // Llamar periódicamente en loop() (no hace falta cada 50ms)

// pumpIndex: 0 = Bomba A, 1 = Bomba B
void setPumpName(uint8_t pumpIndex, const char* name);
const char* getPumpName(uint8_t pumpIndex);

// Configuración de horario vía JSON (array de hasta MAX_SCHEDULE_SLOTS_PER_PUMP objetos,
// esquema documentado en IRRIGATION_SCHEDULING.md). Devuelve false si el JSON es inválido.
bool setPumpScheduleJson(uint8_t pumpIndex, const char* json);
String getPumpScheduleJson(uint8_t pumpIndex);

// Marca que el toque en pantalla tomó control manual de esa bomba - el programador no la
// tocará hasta el próximo cambio real de ventana horaria (inicio o fin de un slot).
void markPumpManualOverride(uint8_t pumpIndex);

// Para la UI (DisplayHandler): true si esa bomba está actualmente bajo control manual
// (toque/Zigbee ON-OFF), false si el programador tiene el control (o no hay hora
// sincronizada, en cuyo caso el programador no actúa y todo es efectivamente manual).
bool isPumpManualOverride(uint8_t pumpIndex);

// Zona horaria POSIX (persistente) - ver Config.h DEFAULT_TZ_POSIX para el formato.
void setTimezone(const char* posixTz);
String getTimezone();

// Sincronización horaria manual. Sin esto, el reloj interno del ESP32-H2 arranca en
// 1970-01-01 y el calendario no se evalúa (updateScheduleManager() no hace nada hasta que
// isTimeSynced() sea true).
void setEpochTime(uint32_t epochSeconds);
bool isTimeSynced();

#endif
