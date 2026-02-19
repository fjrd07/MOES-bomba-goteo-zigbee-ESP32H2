#ifndef DISPLAY_HANDLER_H
#define DISPLAY_HANDLER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "TAMC_GT911.h"

// Configuración de pantalla táctil
#define TOUCH_ROTATION 1 // Ajustar según orientación física
#define TOUCH_MAP_X1 0
#define TOUCH_MAP_Y1 0
#define TOUCH_MAP_X2 320
#define TOUCH_MAP_Y2 480

void initDisplay();
void updateDisplayFull(bool pumpA, bool pumpB, bool levelLow, float batteryVolts);
void showBootScreen();
bool checkTouch(uint16_t *x, uint16_t *y); // Devuelve true si hay toque y las coordenadas
void handleTouch(); // Procesa la acción táctil

#endif
