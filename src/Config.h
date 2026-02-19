#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Definiciones de Pines
#define PIN_PUMP_A      0
#define PIN_PUMP_B      1
#define PIN_LEVEL_SENS  11
#define PIN_BATTERY     10

// Pines TFT LCD (SPI) - Definidos en platformio.ini, pero aquí para referencia
// SCK=12, MOSI=13, CS=7, DC=6, RST=5, BL=4

// Pines Touch I2C (Capacitivo GT911)
#define PIN_TOUCH_SDA   8
#define PIN_TOUCH_SCL   9
#define PIN_TOUCH_INT   14
#define PIN_TOUCH_RST   15

// Constantes
#define NIVEL_VACIO     LOW  
#define HYSTERESIS_MS   3000 // 3 segundos de histéresis

#endif
