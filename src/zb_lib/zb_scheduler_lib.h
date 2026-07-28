#ifndef ZB_SCHEDULER_LIB_H
#define ZB_SCHEDULER_LIB_H

#include <Arduino.h>

// =====================================================================================
// zb_scheduler_lib - librería Zigbee propia para este proyecto, construida directamente
// sobre esp-zigbee-sdk (el SDK real de Espressif que arduino-esp32/Zigbee.h envuelve),
// en vez de sobre el wrapper de alto nivel de Arduino.
//
// POR QUÉ EXISTE: Zigbee.h (arduino-esp32) no tiene un tipo de dispositivo "programador de
// riego", y no hay forma de verificar sin el código fuente de esa librería si expone una
// manera de añadir atributos custom (string/JSON) a un endpoint existente sin arriesgar
// conflictos con el despachador interno de eventos que ya usan los endpoints On/Off de las
// bombas. Esta librería resuelve eso construyendo TODO el dispositivo Zigbee (bombas +
// clúster de configuración custom) con una única implementación consistente, con un único
// manejador de acciones ZCL, sin mezclar dos capas de abstracción distintas.
//
// QUÉ EXPONE:
//   - Dos endpoints On/Off estándar (1 y 2) para Bomba A y Bomba B - mismo comportamiento
//     que antes (ZigbeeLight), pero implementado directamente.
//   - Un endpoint de configuración (4) con un clúster manufacturer-specific (0xFC00,
//     rango reservado por la especificación ZCL para uso no estándar) con atributos de
//     texto/JSON para nombres de bomba, horarios, zona horaria y sincronización de hora -
//     la pieza que Zigbee.h no podía dar con confianza.
//
// MODELADO A PARTIR DE (para verificar/depurar contra la fuente oficial si algo no
// compila en tu versión exacta del SDK):
//   esp-zigbee-sdk/examples/esp_zigbee_HA_sample/HA_on_off_light   (bring-up del stack,
//     clusters Basic/Identify/OnOff, endpoint list, device_register, action handler)
//   esp-zigbee-sdk/examples/esp_zigbee_customized_devices          (clústeres/atributos
//     custom vía esp_zb_zcl_attr_list_create + esp_zb_custom_cluster_add_custom_attr)
//
// ⚠️ HONESTIDAD: esto no se compiló ni se probó contra hardware real (sin acceso a
// compilador/placa en esta sesión). Se escribió con el mayor cuidado posible usando la API
// pública documentada de esp-zigbee-sdk. Ver docs/hardware/IRRIGATION_SCHEDULING.md,
// sección "Transporte Zigbee", para la guía de depuración si algo no compila o no reporta
// bien - los nombres de struct/función exactos pueden variar levemente entre versiones del
// SDK, y esos son los primeros puntos a revisar.
// =====================================================================================

#define ZB_EP_PUMP_A     1
#define ZB_EP_PUMP_B     2
#define ZB_EP_SENSORS    3   // reservado - ver ZigbeeHandler.cpp TODO existente
#define ZB_EP_CONFIG     4

// Arranca el stack Zigbee completo (bombas + endpoint de configuración) y lo une a la red.
// Reemplaza a Zigbee.begin() de arduino-esp32 - no llamar a ambos.
bool zbSchedulerInit();

// Debe llamarse periódicamente (p.ej. en loop()) para procesar la cola de eventos ZCL.
void zbSchedulerLoop();

// Actualiza el atributo On/Off reportado de cada bomba hacia el coordinador (Z2M/HA lo ve
// reflejado sin esperar a que ellos pregunten).
void zbReportPumpState(uint8_t pumpIndex, bool state);

// Sube al clúster de configuración los valores actuales (desde ScheduleManager) - llamar
// tras cualquier cambio hecho por Serial, para que Z2M/HA vea el estado real.
void zbReportConfigAll();

#endif
