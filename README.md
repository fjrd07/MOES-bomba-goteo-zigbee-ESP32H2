#Bomba Goteo Zigbee ESP32H2

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
  - `main.cpp`: Lógica principal, integración del programador de riego y canal de comandos Serie.
  - `Config.h`: Definición de pines y constantes globales.
  - `HardwareControl.cpp/h`: Abstracción del control de hardware (Bombas con PWM, sensores).
  - `ScheduleManager.cpp/h`: Programador de riego (calendario diario/semanal/mensual/anual) y RTC (DS3231).
  - `ZigbeeHandler.cpp/h`: Capa fina que conecta el firmware con `zb_lib/zb_scheduler_lib`.
  - `zb_lib/zb_scheduler_lib.cpp/h`: Librería Zigbee propia del proyecto, sobre `esp-zigbee-sdk`.
- `Z2M/`: Archivos para integración con Zigbee2MQTT.
  - `Bomba-Riego-Goteo-Z2M.js`: Convertidor externo para Z2M.
- `docs/FLASHEO_PRUEBAS.md`: Guía paso a paso para compilar, flashear y probar el firmware.
- `docs/hardware/`: Documentación de hardware (módulo ESP32-H2, breakout de terminales, detección de marcha en seco por corriente, presupuesto de energía batería/solar, programador de riego, y `BOM.md`/`BOM.csv` - lista de materiales, ver `CLAUDE.md` para la directiva de mantenerla actualizada).
- `platformio.ini`: Configuración del entorno de desarrollo y dependencias.

## Instalación y Compilación
Ver [`docs/FLASHEO_PRUEBAS.md`](docs/FLASHEO_PRUEBAS.md) para la guía paso a paso completa
(compilar, flashear, monitor serie, pruebas básicas). Resumen rápido:
1. Instalar Visual Studio Code y la extensión PlatformIO.
2. Abrir la carpeta del proyecto en VS Code.
3. PlatformIO descargará automáticamente las herramientas y librerías necesarias.
4. Compilar el proyecto usando el botón "Check" o ejecutando `pio run` en la terminal.
5. Subir el firmware al ESP32-H2 conectado por USB.

## Notas de Desarrollo
- El soporte para Zigbee en Arduino para ESP32-H2 está en desarrollo activo. Se recomienda usar la versión más reciente de la plataforma `espressif32`.
- La lógica de Zigbee usa la librería **Zigbee nativa de arduino-esp32** (`Zigbee.h`, incluida en el core desde arduino-esp32 3.x - ver `platformio.ini`, que pinea `platform = espressif32 @ ^6.9.0` y fuerza `board_build.partitions = zigbee.csv` + `-D ZIGBEE_MODE_ED=1`). Las bombas ya están cableadas como endpoints Zigbee controlables; el endpoint de sensores (nivel + batería) y el cliente OTA quedan marcados con `TODO` en `src/ZigbeeHandler.cpp` porque su API exacta depende de la versión del core instalada - antes de compilar, compáralos contra los ejemplos oficiales `Zigbee_On_Off_Light` y `Zigbee_OTA` que vienen junto al framework.

## Guía de Compilación y Carga (Desde Casa)

Como la primera compilación requiere descargar herramientas de internet, sigue estos pasos en tu casa:

### 1. Preparación
1.  Instala **Visual Studio Code**.
2.  Instala la extensión **PlatformIO IDE** dentro de VS Code (icono de cabeza de alien).
3.  Clona este repositorio o copia la carpeta `Bomba-Riego-Goteo-Z2M`.
4.  Abre la carpeta del proyecto en VS Code (**File > Open Folder**).

### 2. Compilación
1.  Espera a que PlatformIO termine de indexar (verás una barra de progreso abajo).
2.  Haz clic en el icono de **PlatformIO** en la barra lateral izquierda.
3.  En "Project Tasks", despliega `env:esp32-h2-devkitm-1` > **General**.
4.  Haz clic en **Build**.
    *   *Nota*: La primera vez tardará varios minutos descargando el framework `espressif32`.

### 3. Cagar el Programa (Flash)
1.  Conecta tu **ESP32-H2** al PC por USB.
2.  (Opcional) Si no detecta el puerto, pon la placa en **Modo Boot**:
    *   Mantén presionado el botón **BOOT** (o IO9).
    *   Pulsa y suelta el botón **RST** (Reset).
    *   Suelta el botón **BOOT**.
3.  Haz clic en **Upload** en el menú de PlatformIO.
4.  Una vez subido, pulsa **RST** para reiniciar la placa.

### 4. Monitor Serie (Depuración)
Para ver los mensajes de inicio y estado de Zigbee:
*   Haz clic en **Monitor** en las tareas de PlatformIO.
*   Asegúrate de que la velocidad sea **115200**.

### 5. Actualizar Firmware por OTA (vía Z2M)

Una vez que el dispositivo ya está emparejado (ver sección 6) y funcionando, las siguientes
versiones de firmware se pueden subir por Zigbee en vez de por USB.

1.  **Generar la imagen OTA**: compila normalmente con `pio run` (o el botón **Build**).
    El hook `extra_script.py` genera automáticamente `firmware.zigbee` junto a `firmware.bin`
    en la carpeta `.pio/build/esp32-h2-devkitm-1/`, con la cabecera OTA de Zigbee ya incluida.
2.  **Incrementar la versión**: sube `FILE_VERSION` en `extra_script.py` (por ejemplo de
    `0x00000001` a `0x00000002`) antes de compilar - si la versión no sube, Z2M no ofrecerá
    la actualización porque la considera igual o más vieja que la instalada.
3.  **Copiar la imagen a donde Z2M pueda leerla**: mueve `firmware.zigbee` a una carpeta
    accesible por el proceso de Zigbee2MQTT, por ejemplo `data/ota/` dentro de tu instalación
    de Z2M.
4.  **Crear/editar el índice OTA local**: Z2M soporta un índice de imágenes propio vía la
    opción `ota.zigbee_ota_override_index_location` en `configuration.yaml` de Z2M. Crea un
    archivo (por ejemplo `data/ota/index.json`) con una entrada para este dispositivo:
    ```json
    [
      {
        "fileVersion": 2,
        "fileSize": 123456,
        "manufacturerCode": 4097,
        "imageType": 0,
        "url": "./ota/firmware.zigbee"
      }
    ]
    ```
    `manufacturerCode` (4097 = `0x1001`) e `imageType` (`0x0000`) deben coincidir
    **exactamente** con `MANUFACTURER_CODE`/`IMAGE_TYPE` de `extra_script.py`, y `fileSize`
    con el tamaño real en bytes de `firmware.zigbee`. En `configuration.yaml` de Z2M:
    ```yaml
    ota:
      zigbee_ota_override_index_location: data/ota/index.json
    ```
5.  **Reiniciar Zigbee2MQTT** para que recargue el índice.
6.  **Lanzar la actualización**: en el frontend de Z2M, abre el dispositivo
    `Bomba-Riego-Goteo-Z2M` → pestaña **OTA** → **Check for update** → **Update**. Sigue el
    progreso desde ahí; no desconectes la alimentación del ESP32-H2 durante el proceso.

### 6. Emparejar el Dispositivo y Darlo de Alta en Home Assistant (vía Z2M)

1.  **Instalar el convertidor externo**: copia `Z2M/Bomba-Riego-Goteo-Z2M.js` a la carpeta
    `data/external_converters/` de tu instalación de Zigbee2MQTT, y en `configuration.yaml`
    de Z2M agrega:
    ```yaml
    external_converters:
      - Bomba-Riego-Goteo-Z2M.js
    ```
    Reinicia Zigbee2MQTT para que lo cargue (revisa el log: debe indicar que el convertidor
    externo se cargó sin errores).
2.  **Activar el modo de emparejamiento**: en el frontend de Z2M ve a la pantalla principal
    y activa **Permit join (All)**.
3.  **Emparejar el ESP32-H2**: enciende o resetea la placa (botón **RST**). Al iniciar, la
    librería Zigbee entra en modo de unión automáticamente si no tiene una red guardada.
    Revisa el **Monitor Serie** (paso 4) para confirmar que imprime `Zigbee Initialized
    (End Device)` sin errores.
4.  **Confirmar en Z2M**: el dispositivo debería aparecer en la lista con el modelo
    `Bomba-Riego-Goteo-Z2M` (definido en el convertidor). Si aparece como `Unsupported`,
    revisa que el convertidor externo se haya cargado (paso 1) y que el `zigbeeModel`
    reportado por el firmware coincida con el declarado en el `.js`.
5.  **Renombrar el dispositivo (opcional)**: en la página del dispositivo dentro de Z2M,
    cambia el **Friendly Name** a algo descriptivo (por ejemplo `riego_patio`) - ese nombre
    es el que usará MQTT y, por lo tanto, Home Assistant.
6.  **Verificar en Home Assistant**: con la integración **MQTT** habilitada y Z2M publicando
    con *discovery* (opción por defecto en Z2M), el dispositivo aparece solo, sin pasos
    extra, bajo **Configuración → Dispositivos y Servicios → MQTT**. Deberías ver:
    - `switch.<friendly_name>_bomba_a` / `..._bomba_b` - controlan las bombas.
    - `binary_sensor.<friendly_name>_water_level` - estado del nivel de agua (`ON` = bajo).
    - `sensor.<friendly_name>_battery` - porcentaje de batería.
    - `binary_sensor.<friendly_name>_lock_safety` - bloqueo de seguridad activo.
7.  **Añadir al dashboard**: desde la página del dispositivo en Home Assistant, usa
    **Añadir a Lovelace** para generar automáticamente una tarjeta con todas las entidades.

---
**Autor**: FSD (Generado por Asistente AI)
**Versión**: 1.0
