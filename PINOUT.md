# Documentación de Hardware y Pinout - Bomba-Riego-Goteo-Z2M

Este documento detalla la conexión física de todos los componentes al ESP32-H2-MINI-1.

## 1. Pinout del Microcontrolador (ESP32-H2)

| Componente | Función | GPIO ESP32-H2 | Notas |
| :--- | :--- | :--- | :--- |
| **Etapa de Potencia** | | | |
| Bomba A | Salida PWM/Digital | **GPIO 0** | Control MOSFET (Canal N) |
| Bomba B | Salida PWM/Digital | **GPIO 1** | Control MOSFET (Canal N) |
| **Sensores** | | | |
| Sensor Nivel | Entrada Digital | **GPIO 11** | Flotador (Configurado INPUT_PULLUP). Cerrado=Tierra (Lleno). |
| Batería | Entrada Analógica | **GPIO 10** | Divisor de voltaje (ej. 100k/100k para Li-Ion). |
| **Pantalla (SPI)** | | | Controlador ST7796 (3.5" IPS) |
| LCD CS | Chip Select | **Atado a GND** | GPIO7 no disponible en el módulo; único dispositivo SPI, no necesita selección activa (`TFT_CS=-1`). |
| LCD DC | Data/Command | **GPIO 24** | Pin físico "TX"/U0TXD del módulo Supermini (GPIO6 no disponible en ese módulo). |
| LCD RST | Reset | **GPIO 5** | |
| LCD MOSI | Master Out | **GPIO 13** | |
| LCD SCLK | Clock | **GPIO 12** | |
| LCD BL | Backlight | **GPIO 4** | Control de brillo (PWM opcional). |
| **Táctil (I2C)** | | | Controlador GT911 |
| Touch SDA | Data I2C | **GPIO 8** | Comparte el pin con el LED RGB WS2812B integrado del módulo (sin conflicto funcional, el firmware no lo controla). |
| Touch SCL | Clock I2C | **GPIO 9** | |
| Touch INT | Interrupción | **GPIO 14** | Activo BAJO. |
| Touch RST | Reset | **GPIO 23** | Pin físico "RX"/U0RXD del módulo Supermini (GPIO15 no disponible en ese módulo). |
| **Detección de Tanque Vacío (opcional)** | | | Ver [`docs/hardware/PUMP_DRY_RUN_DETECTION.md`](docs/hardware/PUMP_DRY_RUN_DETECTION.md) |
| Bomba A - Sensor Corriente | Entrada Analógica | **GPIO 2** | Salida analógica de un ACS712 en serie con la Bomba A |
| Bomba B - Sensor Corriente | Entrada Analógica | **GPIO 3** | Salida analógica de un ACS712 en serie con la Bomba B |

## 2. Diagrama de Conexiones

### Etapa de Potencia (Bombas)
*   **MOSFET**: AO3400A (o compatible Logic Level).
*   **Gate**: Al GPIO (0 o 1) con resistencia _pull-down_ de 10k a GND.
*   **Drain**: Al negativo de la Bomba.
*   **Source**: A GND.
*   **Diodo**: Diodo Flyback (1N4007 o SS14) en paralelo con la bomba (Cátodo a V+, Ánodo a Drain del MOSFET).

### Sensor de Nivel
*   **Tipo**: Interruptor de flotador (Reed Switch).
*   **Conexión**:
    *   Pin 1 del Sensor -> GPIO 11.
    *   Pin 2 del Sensor -> GND.
*   **Protección**: Condensador de 100nF en paralelo cerca del pin del MCU para filtrar ruido (debouncing hardware).

### Alimentación
*   **Cargador**: Módulo CN3163 o TP4056 para batería 18650.
*   **Regulador**: LDO de 3.3V (ej. AMS1117-3.3 o HT7333) para alimentar el ESP32-H2 y la Pantalla.

## 3. Guía de Pruebas Zigbee

### 3.1. Emparejamiento (Pairing)
1.  Al encender el dispositivo por primera vez, entrará automáticamente en modo emparejamiento.
2.  En **Zigbee2MQTT**, activar "Permit join (All)".
3.  El dispositivo debería aparecer como `Bomba-Riego-Goteo-Z2M`.

### 3.2. Pruebas Funcionales
1.  **Switch A/B**: Enviar payload `{"state_l1": "ON"}` o usar el dashboard para encender Bomba A. Verificar salida en GPIO 0.
2.  **Sensor de Nivel**: Simular vacío (desconectar flotador si es NC, o cerrar si es NO según lógica).
    *   Verificar que `binary_sensor_water_level` cambia a `LOW` (o `PROBLEM`).
    *   Verificar que las bombas se apagan automáticamente (Interlock).
3.  **Reporte de Batería**: Verificar que el voltaje se actualiza periódicamente en el clúster `genPowerCfg`.

### 3.3. Actualización OTA
1.  Generar firmware: `pio run`.
2.  El script `extra_script.py` generará `firmware.zigbee`.
3.  Copiar a `data/images` en Z2M y actualizar `index.json`.
4.  Iniciar OTA desde el frontend de Z2M.
