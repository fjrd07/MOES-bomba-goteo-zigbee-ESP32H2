#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =====================================================================================
// MAPA DE PINES - ESP32-H2-MINI-1 (ver docs/hardware/PIN_BREAKOUT.md para el
// diagrama del breakout de terminales usado para el cableado físico)
//
// Todas las asignaciones de GPIO del proyecto viven en ESTE archivo. Para mover
// una función a otro pin, edita SOLO la línea correspondiente aquí abajo - el
// resto del código (HardwareControl, DisplayHandler, main) siempre referencia
// estos nombres, nunca un número de GPIO suelto.
// =====================================================================================

// --- Etapa de Potencia (Bombas, vía MOSFET AO3400A) ---
#define PIN_PUMP_A       0   // GPIO0  - Salida digital/PWM, activa el MOSFET de la Bomba A
#define PIN_PUMP_B       1   // GPIO1  - Salida digital/PWM, activa el MOSFET de la Bomba B

// --- Sensores ---
#define PIN_LEVEL_SENS   11  // GPIO11 - Entrada digital (INPUT_PULLUP), flotador de nivel (cerrado=GND=depósito lleno)
#define PIN_BATTERY      10  // GPIO10 - Entrada analógica (ADC), divisor de voltaje de la batería 18650

// --- Pantalla TFT IPS 3.5" (SPI, driver ST7796) ---
// LIMITACIÓN: la librería TFT_eSPI solo lee estos pines en tiempo de compilación,
// vía build_flags en platformio.ini - no acepta configurarlos desde este header.
// Estos alias solo dan un nombre propio del proyecto a esas macros de la librería
// para que el resto del código no repita literales ni nombres genéricos de TFT_eSPI.
// Si cambias un pin de la pantalla, actualiza AMBOS lugares (platformio.ini y, si
// hace falta, este comentario) para que no queden desincronizados.
// El esquematico oficial del módulo (docs/hardware/esp32h2_supermini_header_pinout.png)
// solo expone 15 GPIO en total - no alcanza para CS+DC+Touch_RST en pines nuevos. Por
// eso TFT_CS se ata permanentemente a GND en la placa (-1, sin GPIO: único dispositivo
// en el bus SPI, no necesita selección activa) y PIN_LCD_DC usa el único pin que sí
// sobra (GPIO24, "TX"/U0TXD) - ver docs/hardware/PIN_BREAKOUT.md sección 1.
#define PIN_LCD_CS       TFT_CS    // -1 (atado a GND en la placa) - sin selección activa, no requiere GPIO
#define PIN_LCD_DC       TFT_DC    // GPIO24 ("TX"/U0TXD) - Data/Command del panel TFT
#define PIN_LCD_RST      TFT_RST   // GPIO5  - Reset del panel TFT
#define PIN_LCD_MOSI     TFT_MOSI  // GPIO13 - Master Out, datos SPI hacia el panel
#define PIN_LCD_SCLK     TFT_SCLK  // GPIO12 - Reloj SPI del panel
#define PIN_LCD_BL       TFT_BL    // GPIO4  - Backlight (brillo, PWM opcional)

// --- Táctil I2C (controlador capacitivo GT911) ---
// PIN_TOUCH_SDA (GPIO8) comparte el pin físico con el LED RGB WS2812B integrado del
// módulo (su DIN también está cableado a GPIO8 internamente) - el firmware no controla
// ese LED, así que en la práctica no hay conflicto funcional, pero ten presente que
// cualquier actividad I2C en ese pin es visible para el LED integrado y viceversa.
#define PIN_TOUCH_SDA    8   // GPIO8  - Línea de datos I2C hacia el controlador táctil (compartido con LED RGB integrado)
#define PIN_TOUCH_SCL    9   // GPIO9  - Línea de reloj I2C hacia el controlador táctil
#define PIN_TOUCH_INT    14  // GPIO14 - Interrupción táctil (activo BAJO)
// PIN_TOUCH_RST usa GPIO23 ("RX"/U0RXD) - el pin que quedó libre al atar TFT_CS a GND.
// GPIO15 (asignación original) y GPIO22 (propuesta anterior) NO existen en el header de
// este módulo según su esquemático oficial - ver docs/hardware/PIN_BREAKOUT.md.
#define PIN_TOUCH_RST    23  // GPIO23 ("RX"/U0RXD) - Reset del controlador táctil

// --- Detección indirecta de tanque vacío por corriente de succión (ver
// docs/hardware/PUMP_DRY_RUN_DETECTION.md para el porqué, el cableado y la calibración) ---
// Requiere añadir un sensor de corriente (p.ej. ACS712 5A) en serie con cada bomba, con su
// salida analógica conectada a estos pines.
#define PIN_PUMP_A_ISENSE  2   // GPIO2 (ADC1_CH1) - salida analógica del sensor de corriente Bomba A
#define PIN_PUMP_B_ISENSE  3   // GPIO3 (ADC1_CH2) - salida analógica del sensor de corriente Bomba B

// Constantes
#define NIVEL_VACIO     LOW
#define HYSTERESIS_MS   3000 // 3 segundos de histéresis

// --- Modo de detección de tanque vacío: elige uno según el hardware disponible ---
#define LEVEL_MODE_FLOAT_ONLY    0  // Solo sensor de flotador (comportamiento original)
#define LEVEL_MODE_CURRENT_ONLY  1  // Solo detección por corriente de succión (sin flotador)
#define LEVEL_MODE_HYBRID        2  // Ambos - bloquea si CUALQUIERA detecta vacío (más seguro)
#define LEVEL_DETECTION_MODE     LEVEL_MODE_HYBRID

// --- Calibración de detección por corriente (AJUSTAR CON TU BOMBA REAL) ---
// Válido para CUALQUIERA de las dos opciones de hardware de docs/hardware/PUMP_DRY_RUN_DETECTION.md
// - un ACS712 (Hall, aislado) o una shunt + amplificador INA180/181 (más barato y sensible,
// recomendado si no te importa soldar SMD). Ambos entregan "voltaje analógico proporcional a
// corriente" - solo cambian los dos números de abajo:
//   ACS712 5A:            CURRENT_SENSOR_MV_PER_AMP=185.0,  CURRENT_SENSOR_ZERO_MV=1650.0 (~Vcc/2)
//   Shunt 0.1Ω + INA180A1: CURRENT_SENSOR_MV_PER_AMP=2000.0, CURRENT_SENSOR_ZERO_MV=0.0 (low-side, referenciado a GND)
// Estos valores son PLACEHOLDERS - no se han medido contra hardware real. Calibra con el
// Monitor Serie: enciende cada bomba sumergida en agua y anota la corriente estable que
// reporta readPumpACurrentMA()/readPumpBCurrentMA(); luego enciéndela brevemente en seco
// (fuera del agua) y anota esa corriente. PUMP_DRY_CURRENT_THRESHOLD_MA debe quedar a medio
// camino entre ambas lecturas.
#define CURRENT_SENSOR_MV_PER_AMP      185.0f  // mV/A - ajustar según el sensor elegido (ver arriba)
#define CURRENT_SENSOR_ZERO_MV        1650.0f  // Offset en reposo - medir con la bomba apagada
#define PUMP_STARTUP_IGNORE_MS          400    // Ignora el pico de arranque (inrush) de cada bomba
#define PUMP_DRY_CONFIRM_MS            2000    // Tiempo de corriente baja sostenida antes de confirmar "seco"
#define PUMP_DRY_CURRENT_THRESHOLD_MA  150.0f  // PLACEHOLDER - calibrar con tu bomba real

// --- Ahorro de energía de pantalla (ver docs/hardware/DISPLAY_OPTIONS.md sección 0) ---
// Backlight y controlador táctil se apagan tras este tiempo sin toques; el GT911 sigue
// detectando toques en su propio modo de bajo consumo y despierta la pantalla automáticamente.
#define BACKLIGHT_TIMEOUT_MS   20000  // 20s sin actividad táctil -> apaga backlight + duerme el GT911
// Dirección I2C del GT911. 0x5D es la más común (INT en LOW durante el reset de fábrica del
// panel). Si tras este cambio el toque deja de despertar la pantalla, prueba con 0x14.
#define GT911_I2C_ADDR         0x5D

// Duración del light-sleep de la CPU entre iteraciones de loop() (reemplaza el delay() fijo
// que había antes). Ver la advertencia sobre compatibilidad con Zigbee en main.cpp/loop().
#define LOOP_SLEEP_MS          50

// --- Control de velocidad de bombas (PWM vía LEDC) - ver docs/hardware/IRRIGATION_SCHEDULING.md ---
// Frecuencia moderada para minimizar ruido audible del motor sin acercarse a los límites de
// conmutación del MOSFET AO3400A. AJUSTAR si tu bomba concreta responde mejor a otra frecuencia -
// no se ha probado contra un motor real, y muchas bombas pequeñas de diafragma no arrancan de
// forma fiable por debajo de un ~30-40% de duty (par insuficiente a baja tensión promedio).
#define PUMP_PWM_FREQ_HZ       5000
#define PUMP_PWM_RESOLUTION    8    // 8 bits -> duty 0-255

// --- Programador de riego (calendario diario+semanal+mensual+anual combinado, ver
// docs/hardware/IRRIGATION_SCHEDULING.md) ---
#define MAX_SCHEDULE_SLOTS_PER_PUMP  4
#define MAX_PUMP_NAME_LEN            24

// Zona horaria POSIX por defecto (formato TZ estándar, ej. "COT5" = Colombia UTC-5 sin DST;
// "CET-1CEST,M3.5.0,M10.5.0/3" = Europa Central con DST). Se puede sobreescribir en caliente
// vía Zigbee (ver ZigbeeHandler) sin reflashear. Sin sincronización horaria (ver más abajo) el
// reloj interno arranca en 1970-01-01 y el calendario no puede evaluarse correctamente.
#define DEFAULT_TZ_POSIX       "COT5"

#endif
