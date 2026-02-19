#include <Arduino.h>
#include "Config.h"
#include "HardwareControl.h"
#include "ZigbeeHandler.h"
#include "DisplayHandler.h"

// Variables Globales
volatile bool nivelBajoDetectado = false;
unsigned long tiempoUltimoCambioNivel = 0;
bool estadoNivelEstable = true; // True = OK, False = VACIO
bool bloqueoSeguridad = false;

// Estado de bombas local (para UI)
bool estadoBombaA = false;
bool estadoBombaB = false;

// Prototipos
void IRAM_ATTR isrSensorNivel();
void verificarNivelAgua();
void gestionarBombas();
void gestionarInterfaz();

void setup() {
    Serial.begin(115200);
    
    initHardware();
    initZigbee();
    initDisplay();
    
    showBootScreen();
    delay(2000); // Mostrar logo un momento
    
    attachInterrupt(digitalPinToInterrupt(PIN_LEVEL_SENS), isrSensorNivel, CHANGE);
    
    if (readLevelSensor()) {
        bloqueoSeguridad = true;
        reportLevelStatus(true);
    }
    
    Serial.println("MOES Bomba Goteo Zigbee ESP32H2 Iniciado");
    
    // Primer update
    updateDisplayFull(estadoBombaA, estadoBombaB, bloqueoSeguridad, readBatteryVoltage());
}

void loop() {
    verificarNivelAgua();
    loopZigbee();
    gestionarInterfaz(); // Touch y Display
    
    if (!bloqueoSeguridad) {
        gestionarBombas();
    } else {
        setPumpA(false);
        setPumpB(false);
        estadoBombaA = false;
        estadoBombaB = false;
    }
    
    delay(50);
}

void gestionarInterfaz() {
    // Verificar Touch y actualizar
    handleTouch();
    
    // Si queremos actualizar el nivel de batería u otros datos
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 1000) {
        updateDisplayFull(estadoBombaA, estadoBombaB, bloqueoSeguridad, readBatteryVoltage());
        lastUpdate = millis();
    }
}

void IRAM_ATTR isrSensorNivel() {
    // Solo marcamos que hubo un cambio o simplemente dejamos que el polling en loop lo maneje con debounce
    // La interrupción es útil para despertar, pero el debounce por software en loop es más robusto para agua
}

void verificarNivelAgua() {
    bool lecturaActualEsVacio = readLevelSensor();
    static bool ultimoEstadoLecturaVacio = !lecturaActualEsVacio;
    
    // Detectar cambio -> reiniciar timer
    if (lecturaActualEsVacio != ultimoEstadoLecturaVacio) {
        tiempoUltimoCambioNivel = millis();
        ultimoEstadoLecturaVacio = lecturaActualEsVacio;
    }
    
    // Si ha pasado el tiempo de histéresis
    if ((millis() - tiempoUltimoCambioNivel) > HYSTERESIS_MS) {
        // El estado es estable
        if (lecturaActualEsVacio) { 
            // Nivel BAJO confirmado
            if (!bloqueoSeguridad) {
                bloqueoSeguridad = true;
                Serial.println("Nivel Bajo Confirmado -> BLOQUEO ACTIVADO");
                reportLevelStatus(true);
                sendAlert("Deposito Vacio");
            }
        } else {
            // Nivel OK confirmado
            if (bloqueoSeguridad) {
                bloqueoSeguridad = false;
                Serial.println("Nivel Recuperado -> SISTEMA OPERATIVO");
                reportLevelStatus(false);
            }
        }
    }
}

void gestionarBombas() {
    // Aquí iría la lógica de recepción de comandos Zigbee
    // Por ahora, solo confirmamos que no se enciendan si hay bloqueo (ya manejado en loop)
}

void actualizarPantalla() {
    // Stub para e-Paper
}
