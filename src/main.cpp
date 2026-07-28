#include <Arduino.h>
#include <esp_sleep.h>
#include "Config.h"
#include "HardwareControl.h"
#include "ZigbeeHandler.h"
#include "DisplayHandler.h"
#include "ScheduleManager.h"
#include "zb_lib/zb_scheduler_lib.h"

// Variables Globales
volatile bool nivelBajoDetectado = false;
unsigned long tiempoUltimoCambioNivel = 0;
bool estadoNivelEstable = true; // True = OK, False = VACIO
bool bloqueoSeguridad = false;

// Estado de bombas local (para UI). ScheduleManager las escribe directamente cuando el
// riego programado está activo; handleTouch() (DisplayHandler.cpp) las escribe en toques
// manuales y marca markPumpManualOverride() para que el programador no las pise de inmediato.
bool estadoBombaA = false;
bool estadoBombaB = false;

// Prototipos
void IRAM_ATTR isrSensorNivel();
void verificarNivelAgua();
void gestionarInterfaz();
void procesarComandosSerie();

void setup() {
    Serial.begin(115200);

    initHardware();
    initScheduleManager();
    initZigbee();
    initDisplay();

    showBootScreen();
    delay(2000); // Mostrar logo un momento

    attachInterrupt(digitalPinToInterrupt(PIN_LEVEL_SENS), isrSensorNivel, CHANGE);

    if (readLevelSensor()) {
        bloqueoSeguridad = true;
        reportLevelStatus(true);
    }

    Serial.println("Bomba-Riego-Goteo-Z2M Iniciado");

    zbReportConfigAll(); // Sube a Zigbee lo cargado de NVS (nombres, horarios, timezone)

    // Primer update
    updateDisplayFull(estadoBombaA, estadoBombaB, bloqueoSeguridad, readBatteryVoltage());
}

void loop() {
    updatePumpDryRunDetection(estadoBombaA, estadoBombaB);
    verificarNivelAgua();
    loopZigbee();
    gestionarInterfaz(); // Touch y Display

    // Riego programado (ver ScheduleManager.h/docs/hardware/IRRIGATION_SCHEDULING.md).
    // Comanda las bombas directamente salvo que haya un toque manual reciente (ver
    // markPumpManualOverride) o el interlock de seguridad esté activo (chequeo interno).
    updateScheduleManager();

    // Canal de configuración por USB-Serial (ver docs/hardware/IRRIGATION_SCHEDULING.md) -
    // funciona igual que el transporte Zigbee (zb_lib/zb_scheduler_lib) y es útil como
    // acceso local/depuración incluso una vez confirmado el transporte Zigbee en hardware real.
    procesarComandosSerie();

    if (bloqueoSeguridad) {
        bool huboA = estadoBombaA, huboB = estadoBombaB;
        setPumpA(false);
        setPumpB(false);
        estadoBombaA = false;
        estadoBombaB = false;
        if (huboA) zbReportPumpState(0, false);
        if (huboB) zbReportPumpState(1, false);
    }

    // Light-sleep en vez de delay() puro: baja el consumo de la CPU entre iteraciones del
    // loop (ver Config.h LOOP_SLEEP_MS y docs/hardware/POWER_BUDGET.md).
    // ADVERTENCIA sin verificar: no se ha comprobado que esto sea compatible con el
    // procesamiento de eventos de zb_scheduler_lib (zbSchedulerLoop() -> esp_zb_stack_main_loop_iteration())
    // mientras el dispositivo esta unido a una red. Si notas caidas de conexion, comandos
    // que tardan en llegar, o el dispositivo se desconecta de Z2M, quita este bloque y
    // vuelve a `delay(LOOP_SLEEP_MS);`.
    esp_sleep_enable_timer_wakeup((uint64_t)LOOP_SLEEP_MS * 1000ULL);
    esp_light_sleep_start();
}

void gestionarInterfaz() {
    // Verificar Touch y actualizar
    handleTouch();

    // Ahorro de energía: apaga backlight + GT911 tras BACKLIGHT_TIMEOUT_MS sin toques
    // (ver Config.h y docs/hardware/DISPLAY_OPTIONS.md). handleTouch() ya despierta la
    // pantalla en cuanto detecta un toque.
    updateDisplayPowerSaving();

    // Si la pantalla está dormida no tiene sentido seguir redibujando cada segundo -
    // el contenido no es visible (backlight apagado) y solo gasta SPI/CPU.
    if (!isDisplayAwake()) {
        return;
    }

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
    // LEVEL_DETECTION_MODE (Config.h) decide qué señales de "vacío" se combinan:
    // el flotador físico y/o la detección indirecta por corriente de succión de las bombas
    // (ver docs/hardware/PUMP_DRY_RUN_DETECTION.md). En modo HIBRIDO basta con que
    // cualquiera de las dos detecte vacío para confirmar el bloqueo.
    bool floatDetectaVacio = false;
    bool corrienteDetectaVacio = false;

#if LEVEL_DETECTION_MODE == LEVEL_MODE_FLOAT_ONLY || LEVEL_DETECTION_MODE == LEVEL_MODE_HYBRID
    floatDetectaVacio = readLevelSensor();
#endif
#if LEVEL_DETECTION_MODE == LEVEL_MODE_CURRENT_ONLY || LEVEL_DETECTION_MODE == LEVEL_MODE_HYBRID
    corrienteDetectaVacio = isPumpADry() || isPumpBDry();
#endif

    bool lecturaActualEsVacio = floatDetectaVacio || corrienteDetectaVacio;
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

void actualizarPantalla() {
    // Stub para e-Paper
}

// --- Canal de configuración por USB-Serial ---
// Ver docs/hardware/IRRIGATION_SCHEDULING.md para el detalle de cada comando y el esquema
// JSON del horario. Comandos (uno por línea, terminado en \n, vía Monitor Serie a 115200):
//   NOMBRE <0|1> <texto>     - Renombra la Bomba A(0) o B(1)
//   HORARIO <0|1> <json>     - Fija el horario de esa bomba (array JSON de slots)
//   HORARIO? <0|1>           - Imprime el horario actual de esa bomba en JSON
//   TZ <posix_tz>            - Fija la zona horaria (ver Config.h DEFAULT_TZ_POSIX)
//   HORA <epoch_segundos>    - Sincroniza el reloj interno (necesario para que el horario funcione)
void procesarComandosSerie() {
    if (!Serial.available()) return;
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) return;

    int sp1 = line.indexOf(' ');
    String cmd = (sp1 == -1) ? line : line.substring(0, sp1);
    String rest = (sp1 == -1) ? "" : line.substring(sp1 + 1);
    rest.trim();

    if (cmd == "NOMBRE") {
        int sp2 = rest.indexOf(' ');
        if (sp2 == -1) { Serial.println("Uso: NOMBRE <0|1> <texto>"); return; }
        uint8_t idx = rest.substring(0, sp2).toInt();
        String name = rest.substring(sp2 + 1);
        setPumpName(idx, name.c_str());
        zbReportConfigAll();
        Serial.printf("OK - Bomba %d renombrada a \"%s\"\n", idx, name.c_str());
    } else if (cmd == "HORARIO?") {
        uint8_t idx = rest.toInt();
        Serial.println(getPumpScheduleJson(idx));
    } else if (cmd == "HORARIO") {
        int sp2 = rest.indexOf(' ');
        if (sp2 == -1) { Serial.println("Uso: HORARIO <0|1> <json>"); return; }
        uint8_t idx = rest.substring(0, sp2).toInt();
        String json = rest.substring(sp2 + 1);
        if (setPumpScheduleJson(idx, json.c_str())) {
            zbReportConfigAll();
            Serial.printf("OK - Horario de bomba %d actualizado\n", idx);
        } else {
            Serial.println("ERROR - JSON invalido, revisa el formato (ver IRRIGATION_SCHEDULING.md)");
        }
    } else if (cmd == "TZ") {
        setTimezone(rest.c_str());
        zbReportConfigAll();
        Serial.printf("OK - Zona horaria fijada a \"%s\"\n", rest.c_str());
    } else if (cmd == "HORA") {
        uint32_t epoch = strtoul(rest.c_str(), nullptr, 10);
        setEpochTime(epoch);
        Serial.println("OK - Reloj sincronizado");
    } else {
        Serial.println("Comando desconocido. Ver docs/hardware/IRRIGATION_SCHEDULING.md");
    }
}
