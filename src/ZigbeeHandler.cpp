#include "ZigbeeHandler.h"
#include "HardwareControl.h"
#include "zb_lib/zb_scheduler_lib.h"

// ---------------------------------------------------------------------------------
// Implementación sobre zb_scheduler_lib (librería Zigbee propia de este proyecto,
// construida directamente sobre esp-zigbee-sdk - ver src/zb_lib/zb_scheduler_lib.h para
// el porqué de no usar el wrapper de alto nivel Zigbee.h de arduino-esp32). Este archivo
// es ahora una capa fina que conecta esa librería con el resto del firmware.
//
// TODO pendiente (sin resolver, no relacionado con el transporte de configuración):
// sensor endpoint (nivel + batería) y cliente OTA - ver notas en zb_scheduler_lib.cpp para
// dónde añadirlos siguiendo el mismo patrón (esp_zb_cluster_list_add_binary_input_cluster /
// esp_zb_cluster_list_add_analog_input_cluster para el endpoint 3, esp_zb_cluster_list_add_ota_cluster
// para el cliente OTA - no implementado en este cambio).
// ---------------------------------------------------------------------------------

void initZigbee() {
    if (!zbSchedulerInit()) {
        Serial.println("Error al iniciar Zigbee - reiniciando...");
        delay(3000);
        ESP.restart();
    }
    Serial.println("Zigbee Initialized (End Device, zb_scheduler_lib)");
}

void loopZigbee() {
    zbSchedulerLoop();
}

void sendAlert(const char* message) {
    Serial.printf("Sending Zigbee Alert: %s\n", message);
    // TODO: reportar vía el atributo/cluster del endpoint de sensores una vez implementado
    // (ver comentario de arriba) - zb_scheduler_lib ya tiene el patrón para añadirlo.
}

void reportLevelStatus(bool isLow) {
    Serial.printf("Reporting Level Status: %s\n", isLow ? "LOW" : "OK");
    // TODO: igual que sendAlert() - pendiente el endpoint de sensores.
}
