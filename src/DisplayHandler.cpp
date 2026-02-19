#include "DisplayHandler.h"
#include "Config.h"
#include "HardwareControl.h"
#include <Wire.h>

// Instancias globales
TFT_eSPI tft = TFT_eSPI(); 
TAMC_GT911 tp = TAMC_GT911(PIN_TOUCH_SDA, PIN_TOUCH_SCL, PIN_TOUCH_INT, PIN_TOUCH_RST, 320, 480);

// Variables de estado local para evitar redibujado innecesario
bool lastPumpA = false;
bool lastPumpB = false;
bool lastLevelLow = false;
float lastBattery = 0.0;
bool firstDraw = true;

// Prototipos de UI
void drawPumpButton(int x, int y, int w, int h, const char* label, bool state);
void drawStatusArea(bool levelLow);
void drawBattery(float voltage);

void initDisplay() {
    // Inicializar TFT
    tft.init();
    tft.setRotation(1); // Paisaje (Landscape) - Ajustar si es necesario (1 o 3)
    tft.fillScreen(TFT_BLACK);
    
    // Inicializar Touch
    tp.begin();
    tp.setRotation(ROTATION_NORMAL); // Ajustar mapeo con tft.setRotation
    
    Serial.println("Display TFT IPS + Touch Initialized");
}

void showBootScreen() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(MC_DATUM); // Middle Center
    tft.drawString("MOES Zigbee", tft.width() / 2, tft.height() / 2 - 20, 4);
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
        tft.drawString("MOES Controller", 10, 15, 2);
    }
    
    // Solo redibujar lo que cambió
    if (firstDraw || lastPumpA != pumpA) {
        drawPumpButton(20, 60, 130, 100, "Bomba A", pumpA);
        lastPumpA = pumpA;
    }
    
    if (firstDraw || lastPumpB != pumpB) {
        drawPumpButton(170, 60, 130, 100, "Bomba B", pumpB);
        lastPumpB = pumpB;
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

void drawPumpButton(int x, int y, int w, int h, const char* label, bool state) {
    uint16_t color = state ? TFT_GREEN : TFT_RED;
    uint16_t bgcolor = state ? TFT_DARKGREEN : TFT_MAROON;
    
    tft.fillRoundRect(x, y, w, h, 10, bgcolor);
    tft.drawRoundRect(x, y, w, h, 10, color);
    
    tft.setTextColor(TFT_WHITE, bgcolor);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(label, x + w/2, y + h/2 - 15, 2);
    tft.drawString(state ? "ON" : "OFF", x + w/2, y + h/2 + 15, 4);
}

void drawStatusArea(bool levelLow) {
    int y = 180;
    int h = 100; // Ajustar según altura total (320px en landscape)
    
    if (levelLow) {
        tft.fillRect(0, y, tft.width(), h, TFT_RED);
        tft.setTextColor(TFT_YELLOW, TFT_RED);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("! DEPOSITO VACIO !", tft.width() / 2, y + h/2, 4);
        tft.drawString("SISTEMA BLOQUEADO", tft.width() / 2, y + h/2 + 30, 2);
    } else {
        tft.fillRect(0, y, tft.width(), h, TFT_BLACK); // Limpiar zona
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Nivel de Agua: OK", tft.width() / 2, y + h/2, 4);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString("Sistema Listo", tft.width() / 2, y + h/2 + 30, 2);
    }
}

void drawBattery(float voltage) {
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
            return true; 
        }
    }
    return false;
}

// Variables externas para controlar estado (main.cpp tiene las flags)
extern bool estadoBombaA;
extern bool estadoBombaB;
extern bool bloqueoSeguridad;

void handleTouch() {
    uint16_t x, y;
    if (checkTouch(&x, &y)) {
        Serial.printf("Touch: X=%d Y=%d\n", x, y);
        
        // Coordenadas deben ser ajustadas a la rotación de la pantalla si no coinciden
        // Si la pantalla está rotada (landscape), el touch x/y puede estar intercambiado
        // Asumiendo Landscape (320x480 -> 480x320)
        
        // Lógica simple de zonas (Hardcoded para las posiciones de botones arriba)
        // Botón A: x=20, y=60, w=130, h=100
        // Botón B: x=170, y=60, w=130, h=100
        
        // Ajuste de coordenadas si es necesario (depende del driver touch axis)
        // Simulando que funciona directo:
        
        if (!bloqueoSeguridad) {
            if (x >= 20 && x <= 150 && y >= 60 && y <= 160) {
                // Toggle A
                estadoBombaA = !estadoBombaA;
                setPumpA(estadoBombaA);
                // Actualizar inmediatamente la UI
                drawPumpButton(20, 60, 130, 100, "Bomba A", estadoBombaA);
                lastPumpA = estadoBombaA; // Sincronizar estado local de display
                delay(200); // Debounce visual
            } else if (x >= 170 && x <= 300 && y >= 60 && y <= 160) {
                // Toggle B
                estadoBombaB = !estadoBombaB;
                setPumpB(estadoBombaB);
                drawPumpButton(170, 60, 130, 100, "Bomba B", estadoBombaB);
                lastPumpB = estadoBombaB;
                delay(200);
            }
        }
    }
}
