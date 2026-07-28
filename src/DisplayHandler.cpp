#include "DisplayHandler.h"
#include "Config.h"
#include "HardwareControl.h"
#include "ScheduleManager.h"
#include "zb_lib/zb_scheduler_lib.h"
#include <Wire.h>

// Instancias globales
TFT_eSPI tft = TFT_eSPI();
TAMC_GT911 tp = TAMC_GT911(PIN_TOUCH_SDA, PIN_TOUCH_SCL, PIN_TOUCH_INT, PIN_TOUCH_RST, 320, 480);

// --- Layout del panel de control manual (pantalla 480x320 tras tft.setRotation(1)) ---
// Dos tarjetas (Bomba A / Bomba B), cada una con: nombre, botón ON/OFF grande, fila de
// velocidad ("-" / % / "+") y una insignia MANUAL/AUTO/SIN HORA. Ver
// docs/hardware/IRRIGATION_SCHEDULING.md para cómo interactúa esto con el programador.
#define CARD_Y        38
#define CARD_W        225
#define CARD_H        175
#define CARD_A_X      10
#define CARD_B_X      245
#define ONOFF_Y       60
#define ONOFF_H       65
#define SPEED_Y       132
#define SPEED_H       45
#define SPEED_MINUS_W 55
#define SPEED_PLUS_W  55
#define BADGE_Y       182
#define BADGE_H       22
#define STATUS_Y      215
#define STATUS_H      70
#define SPEED_STEP    10

// Variables de estado local para evitar redibujado innecesario
bool lastPumpA = false;
bool lastPumpB = false;
static uint8_t lastSpeedA = 255; // valor imposible -> fuerza el primer dibujo
static uint8_t lastSpeedB = 255;
static bool lastManualA = true;
static bool lastManualB = true;
bool lastLevelLow = false;
float lastBattery = 0.0;
bool firstDraw = true;

// Ahorro de energía: backlight + GT911 en reposo tras BACKLIGHT_TIMEOUT_MS sin toques
static unsigned long lastTouchActivityMs = 0;
static bool displayAwake = true;

// Variables externas (main.cpp)
extern bool estadoBombaA;
extern bool estadoBombaB;
extern bool bloqueoSeguridad;

// Prototipos de UI
static void drawPumpCard(int x0, const char* name, bool state, uint8_t speed, bool isManual);
static void drawStatusArea(bool levelLow);
static void drawBattery(float voltage);
static void wakeDisplay();
static void applyPumpChange(uint8_t pumpIndex, bool newState, uint8_t newSpeed);

void initDisplay() {
    // Inicializar TFT
    tft.init();
    tft.setRotation(1); // Paisaje (Landscape) - Ajustar si es necesario (1 o 3)
    tft.fillScreen(TFT_BLACK);

    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH);

    // Inicializar Touch
    tp.begin();
    tp.setRotation(ROTATION_NORMAL); // Ajustar mapeo con tft.setRotation

    lastTouchActivityMs = millis();
    displayAwake = true;

    Serial.println("Display TFT IPS + Touch Initialized");
}

void showBootScreen() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(MC_DATUM); // Middle Center
    tft.drawString("Bomba-Riego-Goteo-Z2M", tft.width() / 2, tft.height() / 2 - 20, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Iniciando Sistema...", tft.width() / 2, tft.height() / 2 + 20, 2);
}

void updateDisplayFull(bool pumpA, bool pumpB, bool levelLow, float batteryVolts) {
    if (firstDraw) {
        tft.fillScreen(TFT_BLACK);
        // Header
        tft.fillRect(0, 0, tft.width(), 30, TFT_DARKGREY);
        tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
        tft.setTextDatum(ML_DATUM); // Middle Left
        tft.drawString("Bomba-Riego-Goteo-Z2M", 10, 15, 2);
    }

    uint8_t speedA = getPumpASpeed();
    uint8_t speedB = getPumpBSpeed();
    bool manualA = isPumpManualOverride(0);
    bool manualB = isPumpManualOverride(1);

    // Solo redibujar la tarjeta si algo de su contenido cambió
    if (firstDraw || lastPumpA != pumpA || lastSpeedA != speedA || lastManualA != manualA) {
        drawPumpCard(CARD_A_X, getPumpName(0), pumpA, speedA, manualA);
        lastPumpA = pumpA;
        lastSpeedA = speedA;
        lastManualA = manualA;
    }

    if (firstDraw || lastPumpB != pumpB || lastSpeedB != speedB || lastManualB != manualB) {
        drawPumpCard(CARD_B_X, getPumpName(1), pumpB, speedB, manualB);
        lastPumpB = pumpB;
        lastSpeedB = speedB;
        lastManualB = manualB;
    }

    if (firstDraw || lastLevelLow != levelLow) {
        drawStatusArea(levelLow);
        lastLevelLow = levelLow;
    }

    // Actualizar Batería (con un poco de umbral para evitar flickering)
    if (firstDraw || abs(lastBattery - batteryVolts) > 0.1) {
        drawBattery(batteryVolts);
        lastBattery = batteryVolts;
    }

    firstDraw = false;
}

static void drawPumpCard(int x0, const char* name, bool state, uint8_t speed, bool isManual) {
    // Fondo de toda la tarjeta primero, para no dejar restos de un redibujado anterior
    tft.fillRect(x0, CARD_Y, CARD_W, CARD_H, TFT_BLACK);

    // Nombre
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(name, x0 + CARD_W / 2, CARD_Y + 12, 2);

    // Botón ON/OFF grande
    uint16_t color = state ? TFT_GREEN : TFT_RED;
    uint16_t bgcolor = state ? TFT_DARKGREEN : TFT_MAROON;
    tft.fillRoundRect(x0, ONOFF_Y, CARD_W, ONOFF_H, 10, bgcolor);
    tft.drawRoundRect(x0, ONOFF_Y, CARD_W, ONOFF_H, 10, color);
    tft.setTextColor(TFT_WHITE, bgcolor);
    tft.drawString(state ? "ON" : "OFF", x0 + CARD_W / 2, ONOFF_Y + ONOFF_H / 2, 4);

    // Fila de velocidad: "-" | XX% | "+"
    int minusX = x0;
    int plusX = x0 + CARD_W - SPEED_PLUS_W;
    int textX = minusX + SPEED_MINUS_W;
    int textW = CARD_W - SPEED_MINUS_W - SPEED_PLUS_W;

    tft.fillRoundRect(minusX, SPEED_Y, SPEED_MINUS_W, SPEED_H, 6, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.drawString("-", minusX + SPEED_MINUS_W / 2, SPEED_Y + SPEED_H / 2, 4);

    tft.fillRect(textX, SPEED_Y, textW, SPEED_H, TFT_BLACK);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(String(speed) + "%", textX + textW / 2, SPEED_Y + SPEED_H / 2, 4);

    tft.fillRoundRect(plusX, SPEED_Y, SPEED_PLUS_W, SPEED_H, 6, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.drawString("+", plusX + SPEED_PLUS_W / 2, SPEED_Y + SPEED_H / 2, 4);

    // Insignia de origen: quién tiene el control de esta bomba ahora mismo
    uint16_t badgeColor = isManual ? TFT_ORANGE : TFT_BLUE;
    const char* badgeText = isManual ? "MANUAL" : "AUTO (PROGRAMADO)";
    tft.fillRect(x0, BADGE_Y, CARD_W, BADGE_H, badgeColor);
    tft.setTextColor(TFT_BLACK, badgeColor);
    tft.drawString(badgeText, x0 + CARD_W / 2, BADGE_Y + BADGE_H / 2, 1);
}

static void drawStatusArea(bool levelLow) {
    int y = STATUS_Y;
    int h = STATUS_H;

    if (levelLow) {
        tft.fillRect(0, y, tft.width(), h, TFT_RED);
        tft.setTextColor(TFT_YELLOW, TFT_RED);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("! DEPOSITO VACIO !", tft.width() / 2, y + h/2 - 12, 4);
        tft.drawString("SISTEMA BLOQUEADO", tft.width() / 2, y + h/2 + 16, 2);
    } else {
        tft.fillRect(0, y, tft.width(), h, TFT_BLACK); // Limpiar zona
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Nivel de Agua: OK", tft.width() / 2, y + h/2 - 12, 4);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString("Sistema Listo", tft.width() / 2, y + h/2 + 16, 2);
    }
}

static void drawBattery(float voltage) {
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setTextDatum(MR_DATUM); // Middle Right
    String voltStr = String(voltage, 1) + "V";
    tft.drawString(voltStr, tft.width() - 10, 15, 2);
}

bool checkTouch(uint16_t *x, uint16_t *y) {
    tp.read();
    if (tp.isTouched) {
        for (int i=0; i<tp.touches; i++){
            // Usar el primer punto de toque válido
            // Nota: GT911 suele devolver coordenadas crudas 0-Xmax, 0-Ymax
            // Quizás necesitemos mapearlas a la rotación de pantalla
            // Asumimos mapeo directo por ahora o simple inversión si rotation=1
            *x = tp.points[i].x; // Mapeo simple
            *y = tp.points[i].y;

            // Prevención de toques accidentales: si la pantalla estaba dormida, ESTE toque
            // solo la despierta (backlight + saca al GT911 de reposo) - no se reporta como
            // toque "accionable". Hace falta un segundo toque, ya con la pantalla despierta,
            // para que handleTouch() procese botones/scheduler. Evita que el mismo toque que
            // enciende la pantalla también, por accidente, encienda/apague una bomba si caía
            // sobre esa zona de la UI.
            bool estabaDormida = !displayAwake;
            wakeDisplay(); // enciende backlight, reinicia el timer de inactividad
            if (estabaDormida) {
                Serial.println("Touch: solo despertar (se ignora como accion)");
                return false;
            }
            return true;
        }
    }
    return false;
}

// --- Ahorro de energía: backlight + GT911 en reposo tras inactividad ---
// El GT911 sigue detectando toques en su propio modo de bajo consumo ("Green mode") y
// reporta normalmente por I2C en cuanto se le toca - por eso basta con seguir llamando
// checkTouch()/tp.read() en el loop como siempre; no hace falta "despertarlo" a mano.
static void gt911EnterSleep() {
    Wire.beginTransmission(GT911_I2C_ADDR);
    Wire.write(0x80);
    Wire.write(0x40);
    Wire.write(0x05); // Comando de sleep del GT911 (registro 0x8040)
    Wire.endTransmission();
}

static void wakeDisplay() {
    if (!displayAwake) {
        digitalWrite(PIN_LCD_BL, HIGH);
        firstDraw = true; // Fuerza un redibujado completo - el estado pudo cambiar mientras dormía
        displayAwake = true;
        Serial.println("Pantalla: despertada por toque");
    }
    lastTouchActivityMs = millis();
}

static void sleepDisplayIfIdle() {
    if (displayAwake && (millis() - lastTouchActivityMs > BACKLIGHT_TIMEOUT_MS)) {
        digitalWrite(PIN_LCD_BL, LOW);
        gt911EnterSleep();
        displayAwake = false;
        Serial.println("Pantalla: en reposo por inactividad (backlight apagado, tactil en sleep)");
    }
}

void updateDisplayPowerSaving() {
    sleepDisplayIfIdle();
}

bool isDisplayAwake() {
    return displayAwake;
}

// Aplica un cambio de estado/velocidad de bomba disparado por el panel manual: actualiza el
// pin real, la variable global de UI, marca override manual y reporta a Zigbee - los tres
// pasos que antes se repetían sueltos en cada rama de handleTouch().
static void applyPumpChange(uint8_t pumpIndex, bool newState, uint8_t newSpeed) {
    if (pumpIndex == 0) {
        estadoBombaA = newState;
        setPumpASpeed(newState ? newSpeed : 0);
    } else {
        estadoBombaB = newState;
        setPumpBSpeed(newState ? newSpeed : 0);
    }
    markPumpManualOverride(pumpIndex); // El programador no la pisa hasta el próximo cambio de ventana
    zbReportPumpState(pumpIndex, newState); // Refleja el toque manual hacia Z2M/HA
}

void handleTouch() {
    uint16_t x, y;
    if (checkTouch(&x, &y)) {
        Serial.printf("Touch: X=%d Y=%d\n", x, y);

        if (bloqueoSeguridad) return; // el interlock de seguridad no permite control manual

        for (uint8_t p = 0; p < 2; p++) {
            int x0 = (p == 0) ? CARD_A_X : CARD_B_X;
            bool &estado = (p == 0) ? estadoBombaA : estadoBombaB;
            uint8_t speedActual = (p == 0) ? getPumpASpeed() : getPumpBSpeed();

            // Botón ON/OFF: enciende al último % recordado (o 100% si nunca se fijó uno), apaga a 0%.
            if (x >= x0 && x <= x0 + CARD_W && y >= ONOFF_Y && y <= ONOFF_Y + ONOFF_H) {
                bool nuevoEstado = !estado;
                uint8_t nuevaVelocidad = nuevoEstado ? (speedActual > 0 ? speedActual : 100) : 0;
                applyPumpChange(p, nuevoEstado, nuevaVelocidad);
                delay(200); // Debounce visual
                return;
            }

            // Fila de velocidad ("-"/"+"), solo dentro de la banda vertical de esos botones
            if (y >= SPEED_Y && y <= SPEED_Y + SPEED_H) {
                if (x >= x0 && x <= x0 + SPEED_MINUS_W) {
                    // "-": baja 10%; si llega a 0, apaga la bomba también
                    int nueva = (int)speedActual - SPEED_STEP;
                    if (nueva <= 0) { applyPumpChange(p, false, 0); }
                    else { applyPumpChange(p, true, (uint8_t)nueva); }
                    delay(200);
                    return;
                }
                if (x >= x0 + CARD_W - SPEED_PLUS_W && x <= x0 + CARD_W) {
                    // "+": si estaba apagada, la enciende al mínimo escalón; si no, sube 10% (máx 100)
                    int nueva = estado ? min(100, (int)speedActual + SPEED_STEP) : SPEED_STEP;
                    applyPumpChange(p, true, (uint8_t)nueva);
                    delay(200);
                    return;
                }
            }
        }
    }
}
