#include "ScheduleManager.h"
#include "Config.h"
#include "HardwareControl.h"
#include "zb_lib/zb_scheduler_lib.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <time.h>
#include <sys/time.h>
#include <Wire.h>
#include <RTClib.h>

// RTC externo DS3231 (I2C, comparte bus con el táctil - ver docs/hardware/IRRIGATION_SCHEDULING.md
// sección RTC). Es la fuente de verdad persistente: el reloj de sistema del ESP32
// (time()/settimeofday(), usado por el resto de este archivo para no tocar la lógica de
// calendario ya escrita) se siembra desde el DS3231 al arrancar y se re-sincroniza
// periódicamente para corregir la deriva del oscilador interno del ESP32 entre lecturas.
static RTC_DS3231 rtc;
static bool rtcPresente = false;

// Variables de main.cpp que este módulo lee/escribe para comandar las bombas
extern bool estadoBombaA;
extern bool estadoBombaB;
extern bool bloqueoSeguridad;

struct ScheduleSlot {
    bool enabled = false;
    uint8_t startHour = 6;
    uint8_t startMinute = 0;
    uint16_t durationMin = 10;
    uint8_t speedPercent = 100;
    uint8_t daysOfWeekMask = 0x7F;   // bit0=domingo .. bit6=sábado (todos por defecto)
    uint16_t monthsMask = 0x0FFF;    // bit0=enero .. bit11=diciembre (todos por defecto)
    uint32_t dayOfMonthMask = 0;     // bit0=día1 .. bit30=día31; 0 = cualquier día (comodín)
};

struct PumpSchedule {
    char name[MAX_PUMP_NAME_LEN] = "";
    ScheduleSlot slots[MAX_SCHEDULE_SLOTS_PER_PUMP];
};

enum class PumpControlOrigin : uint8_t { NONE = 0, SCHEDULED = 1, MANUAL = 2 };

static Preferences prefs;
static PumpSchedule pumpSchedules[2]; // 0 = A, 1 = B
static PumpControlOrigin pumpOrigin[2] = { PumpControlOrigin::NONE, PumpControlOrigin::NONE };
static bool prevScheduleWantsOn[2] = { false, false };
static bool timeSynced = false;

static bool* const estadoPtr[2] = { &estadoBombaA, &estadoBombaB };
static void (*const setSpeedFn[2])(uint8_t) = { setPumpASpeed, setPumpBSpeed };

static void applyTimezone(const String &tz) {
    setenv("TZ", tz.c_str(), 1);
    tzset();
}

// --- Serialización JSON de un slot: {"en":true,"h":6,"m":0,"dur":10,"spd":100,"dow":127,"mon":4095,"dom":0} ---

static bool applyScheduleJsonToMemory(uint8_t pumpIndex, const char* json) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        Serial.printf("Schedule JSON invalido (bomba %d): %s\n", pumpIndex, err.c_str());
        return false;
    }
    JsonArray arr = doc.as<JsonArray>();
    if (arr.isNull()) {
        Serial.println("Schedule JSON: se esperaba un array");
        return false;
    }

    for (int i = 0; i < MAX_SCHEDULE_SLOTS_PER_PUMP; i++) {
        pumpSchedules[pumpIndex].slots[i] = ScheduleSlot();
        pumpSchedules[pumpIndex].slots[i].enabled = false;
    }

    int i = 0;
    for (JsonObject obj : arr) {
        if (i >= MAX_SCHEDULE_SLOTS_PER_PUMP) break;
        ScheduleSlot &slot = pumpSchedules[pumpIndex].slots[i];
        slot.enabled = obj["en"] | false;
        slot.startHour = obj["h"] | 6;
        slot.startMinute = obj["m"] | 0;
        slot.durationMin = obj["dur"] | 10;
        slot.speedPercent = obj["spd"] | 100;
        slot.daysOfWeekMask = obj["dow"] | 0x7F;
        slot.monthsMask = obj["mon"] | 0x0FFF;
        slot.dayOfMonthMask = obj["dom"] | 0UL;
        i++;
    }
    return true;
}

bool setPumpScheduleJson(uint8_t pumpIndex, const char* json) {
    if (pumpIndex > 1) return false;
    if (!applyScheduleJsonToMemory(pumpIndex, json)) return false;
    String key = String("sched") + pumpIndex;
    prefs.putString(key.c_str(), json);
    return true;
}

String getPumpScheduleJson(uint8_t pumpIndex) {
    if (pumpIndex > 1) return "[]";
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < MAX_SCHEDULE_SLOTS_PER_PUMP; i++) {
        ScheduleSlot &slot = pumpSchedules[pumpIndex].slots[i];
        JsonObject obj = arr.add<JsonObject>();
        obj["en"] = slot.enabled;
        obj["h"] = slot.startHour;
        obj["m"] = slot.startMinute;
        obj["dur"] = slot.durationMin;
        obj["spd"] = slot.speedPercent;
        obj["dow"] = slot.daysOfWeekMask;
        obj["mon"] = slot.monthsMask;
        obj["dom"] = slot.dayOfMonthMask;
    }
    String out;
    serializeJson(doc, out);
    return out;
}

void setPumpName(uint8_t pumpIndex, const char* name) {
    if (pumpIndex > 1) return;
    strlcpy(pumpSchedules[pumpIndex].name, name, MAX_PUMP_NAME_LEN);
    String key = String("name") + pumpIndex;
    prefs.putString(key.c_str(), name);
}

const char* getPumpName(uint8_t pumpIndex) {
    if (pumpIndex > 1) return "";
    return pumpSchedules[pumpIndex].name;
}

void setTimezone(const char* posixTz) {
    prefs.putString("tz", posixTz);
    applyTimezone(String(posixTz));
}

String getTimezone() {
    return prefs.getString("tz", DEFAULT_TZ_POSIX);
}

void setEpochTime(uint32_t epochSeconds) {
    struct timeval tv = { (time_t)epochSeconds, 0 };
    settimeofday(&tv, nullptr);
    timeSynced = true;
    prefs.putBool("timeSynced", true);
    if (rtcPresente) {
        rtc.adjust(DateTime((uint32_t)epochSeconds)); // el DS3231 también queda correcto y
                                                       // sobrevive a un reinicio/corte total
    }
    Serial.printf("Hora sincronizada: epoch=%lu%s\n", (unsigned long)epochSeconds,
        rtcPresente ? " (DS3231 actualizado)" : " (sin DS3231 - solo memoria)");
}

bool isTimeSynced() {
    return timeSynced;
}

void markPumpManualOverride(uint8_t pumpIndex) {
    if (pumpIndex > 1) return;
    pumpOrigin[pumpIndex] = PumpControlOrigin::MANUAL;
}

bool isPumpManualOverride(uint8_t pumpIndex) {
    if (pumpIndex > 1) return true;
    return pumpOrigin[pumpIndex] == PumpControlOrigin::MANUAL || !timeSynced;
}

void initScheduleManager() {
    prefs.begin("riego", false);

    applyTimezone(prefs.getString("tz", DEFAULT_TZ_POSIX));
    timeSynced = prefs.getBool("timeSynced", false);

    // El DS3231 comparte el bus I2C ya inicializado por el táctil (SDA=PIN_TOUCH_SDA,
    // SCL=PIN_TOUCH_SCL) - Wire.begin() es seguro llamarlo de nuevo aunque ya esté
    // inicializado. rtc.begin() detecta si el chip realmente responde en el bus.
    Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL);
    rtcPresente = rtc.begin();
    if (!rtcPresente) {
        Serial.println("DS3231 no detectado en el bus I2C - el reloj solo vivira en RAM (se "
            "pierde en cada reinicio hasta sincronizar de nuevo). Revisa el cableado si esto "
            "es inesperado.");
    } else if (rtc.lostPower()) {
        // El DS3231 perdió su respaldo (pila nueva/agotada) - su hora no es de fiar todavía.
        Serial.println("DS3231 detectado pero sin hora valida (lostPower) - esperando "
            "sincronizacion por Serial o Zigbee antes de activar el programador.");
        timeSynced = false;
    } else {
        // DS3231 con hora válida: siembra el reloj de sistema del ESP32 con ella. A partir de
        // aquí toda la lógica de calendario (localtime_r, etc.) sigue igual sin cambios.
        DateTime ahora = rtc.now();
        struct timeval tv = { (time_t)ahora.unixtime(), 0 };
        settimeofday(&tv, nullptr);
        timeSynced = true;
        Serial.printf("Hora restaurada desde DS3231: %04d-%02d-%02d %02d:%02d:%02d\n",
            ahora.year(), ahora.month(), ahora.day(), ahora.hour(), ahora.minute(), ahora.second());
    }

    const char* defaultNames[2] = { "Bomba A", "Bomba B" };
    for (uint8_t p = 0; p < 2; p++) {
        String nameKey = String("name") + p;
        strlcpy(pumpSchedules[p].name, prefs.getString(nameKey.c_str(), defaultNames[p]).c_str(), MAX_PUMP_NAME_LEN);

        String schedKey = String("sched") + p;
        String json = prefs.getString(schedKey.c_str(), "");
        if (json.length() > 0) {
            applyScheduleJsonToMemory(p, json.c_str());
        }
    }
}

// Un slot aplica "ahora" si: está habilitado, el día de la semana/mes/día-del-mes coinciden
// (día del mes es comodín si dayOfMonthMask==0), y la hora actual cae dentro de
// [inicio, inicio+duración). Soporta ventanas que cruzan medianoche.
static bool slotMatchesNow(const ScheduleSlot &slot, const struct tm &tmNow, int nowMinutesOfDay) {
    if (!slot.enabled) return false;
    if (!(slot.daysOfWeekMask & (1 << tmNow.tm_wday))) return false;
    if (!(slot.monthsMask & (1 << tmNow.tm_mon))) return false;
    if (slot.dayOfMonthMask != 0 && !(slot.dayOfMonthMask & (1UL << (tmNow.tm_mday - 1)))) return false;

    int startMin = slot.startHour * 60 + slot.startMinute;
    int endMin = startMin + slot.durationMin;
    if (endMin <= 1440) {
        return nowMinutesOfDay >= startMin && nowMinutesOfDay < endMin;
    }
    // Ventana cruza medianoche (p.ej. empieza 23:30, dura 60min -> termina 00:30)
    return nowMinutesOfDay >= startMin || nowMinutesOfDay < (endMin - 1440);
}

// Cada cuánto se corrige el reloj de sistema del ESP32 contra el DS3231 - el oscilador
// interno del ESP32 (sin este RTC) puede desviarse varios segundos/minutos por día,
// especialmente con cambios de temperatura a la intemperie; el DS3231 no.
#define RTC_RESYNC_INTERVAL_MS (60UL * 60UL * 1000UL) // 1 hora

static void resyncFromRtcIfDue() {
    if (!rtcPresente) return;
    static unsigned long lastResyncMs = 0;
    unsigned long now = millis();
    if (lastResyncMs != 0 && (now - lastResyncMs) < RTC_RESYNC_INTERVAL_MS) return;
    lastResyncMs = now;

    if (rtc.lostPower()) return; // no fiarse de una hora que el propio DS3231 marca como inválida
    DateTime ahora = rtc.now();
    struct timeval tv = { (time_t)ahora.unixtime(), 0 };
    settimeofday(&tv, nullptr);
    if (!timeSynced) {
        timeSynced = true;
        prefs.putBool("timeSynced", true);
    }
}

void updateScheduleManager() {
    resyncFromRtcIfDue();
    if (!timeSynced) return; // sin hora válida no se puede evaluar el calendario con seguridad

    time_t now = time(nullptr);
    struct tm tmNow;
    localtime_r(&now, &tmNow);
    int nowMinutesOfDay = tmNow.tm_hour * 60 + tmNow.tm_min;

    for (uint8_t p = 0; p < 2; p++) {
        bool anyActive = false;
        uint8_t activeSpeed = 0;
        for (int i = 0; i < MAX_SCHEDULE_SLOTS_PER_PUMP; i++) {
            if (slotMatchesNow(pumpSchedules[p].slots[i], tmNow, nowMinutesOfDay)) {
                anyActive = true;
                activeSpeed = pumpSchedules[p].slots[i].speedPercent;
                break; // primer slot que coincide manda
            }
        }

        // Un cambio real de lo que pide el horario (inicio o fin de ventana) siempre
        // retoma el control automático, aunque haya habido un toque manual de por medio.
        if (anyActive != prevScheduleWantsOn[p]) {
            pumpOrigin[p] = PumpControlOrigin::NONE;
        }
        prevScheduleWantsOn[p] = anyActive;

        if (bloqueoSeguridad) continue;               // el interlock de seguridad manda siempre
        if (pumpOrigin[p] == PumpControlOrigin::MANUAL) continue; // dejar el toque manual como está

        bool changed = (*estadoPtr[p] != anyActive);
        setSpeedFn[p](anyActive ? activeSpeed : 0);
        *estadoPtr[p] = anyActive;
        pumpOrigin[p] = PumpControlOrigin::SCHEDULED;
        if (changed) zbReportPumpState(p, anyActive); // avisa a Z2M/HA del cambio disparado por horario
    }
}
