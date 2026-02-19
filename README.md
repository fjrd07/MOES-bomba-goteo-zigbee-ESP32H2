# MOES Bomba Goteo Zigbee ESP32H2

## Descripción del Proyecto
Este proyecto implementa el firmware para el **Apollo Goteo H2**, un controlador de riego dual inteligente basado en el microcontrolador ESP32-H2 (Zigbee 3.0). El sistema gestiona dos bombas de agua y monitorea el nivel de un depósito mediante un sensor de flotador, integrándose con Home Assistant a través de Zigbee2MQTT.

## Características Principales
- **MCU**: ESP32-H2-MINI-1 (Zigbee 3.0 Nativo).
- **Control de Bombas**: 2 Canales PWM independientes (GPIO 0, GPIO 1).
- **Protección de Nivel**: Bloqueo automático de bombas si el depósito está vacío (GPIO 11).
- **Histéresis**: Confirmación de estado estable del sensor de nivel (3 segundos) para evitar falsos positivos por movimiento del agua.
- **Gestión de Energía**: Monitoreo de batería 18650 vía ADC (GPIO 10).
- **Interfaz Visual**: Soporte preliminar para pantalla e-Paper (SPI).
- **Integración**: Zigbee2MQTT (Z2M) mediante convertidor externo personalizado.

## Estructura del Proyecto
- `src/`: Código fuente del firmware (C++ / Arduino).
  - `main.cpp`: Lógica principal y configuración inicial.
  - `Config.h`: Definición de pines y constantes globales.
  - `HardwareControl.cpp/h`: Abstracción del control de hardware (Bombas, Sensores).
  - `ZigbeeHandler.cpp/h`: Manejo de la pila Zigbee (Stub para implementación futura con SDK).
- `Z2M/`: Archivos para integración con Zigbee2MQTT.
  - `Apollo_Goteo_H2.js`: Convertidor externo para Z2M.
- `platformio.ini`: Configuración del entorno de desarrollo y dependencias.

## Instalación y Compilación
1. Instalar Visual Studio Code y la extensión PlatformIO.
2. Abrir la carpeta del proyecto en VS Code.
3. PlatformIO descargará automáticamente las herramientas y librerías necesarias.
4. Compilar el proyecto usando el botón "Check" o ejecutando `pio run` en la terminal.
5. Subir el firmware al ESP32-H2 conectado por USB.

## Notas de Desarrollo
- El soporte para Zigbee en Arduino para ESP32-H2 está en desarrollo activo. Se recomienda usar la versión más reciente de la plataforma `espressif32`.
- La lógica de Zigbee actual es un "Stub" (marcador de posición). Se debe integrar la librería específica (ej. `esp-zigbee-sdk` o `arduino-esp32-zigbee`) según la preferencia del desarrollador final.

---
**Autor**: FSD (Generado por Asistente AI)
**Versión**: 1.0
