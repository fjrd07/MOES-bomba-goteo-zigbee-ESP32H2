# Guía de Flasheo para Pruebas (Paso a Paso)

Esta guía es para el **primer flasheo de prueba** del firmware actual - verificar que
compila, sube, y que las funciones básicas (pantalla, táctil, bombas, horario, Zigbee)
responden, antes de dar por buena una versión. Para OTA y emparejamiento definitivo en Home
Assistant, ver `README.md` secciones 5 y 6.

## 0. Antes de empezar - qué esperar

Este firmware incluye una librería Zigbee propia (`src/zb_lib/zb_scheduler_lib.cpp`) escrita
directo sobre `esp-zigbee-sdk`, que **no se compiló contra hardware real en esta sesión** (sin
acceso a compilador/placa). Es la parte con más probabilidad de necesitar un ajuste menor la
primera vez que compiles. Si el build falla ahí, ve directo a
[`docs/hardware/IRRIGATION_SCHEDULING.md`](hardware/IRRIGATION_SCHEDULING.md), sección
"Transporte de configuración vía Zigbee", que tiene una lista ordenada de qué revisar primero.
El resto del firmware (pantalla, táctil, bombas, horario, RTC) no depende de esa librería y
debería compilar sin sorpresas.

## 1. Requisitos previos

- **VS Code** con la extensión **PlatformIO IDE** instalada.
- Cable **USB-C** (datos, no solo carga).
- Para probar *todo* de una vez conviene tener ya soldado/conectado:
  - Módulo ESP32-H2 Supermini + pantalla TFT IPS 3.5" + táctil GT911 (ya cableado según
    [`PINOUT.md`](../PINOUT.md)).
  - DS3231 en el bus I2C compartido (GPIO8/GPIO9) - opcional para un primer flasheo, pero
    necesario para probar el horario (sin él, el reloj solo vive en RAM hasta que sincronices
    manualmente por Serial cada vez que reinicies).
  - Sensor(es) de corriente si vas a probar la detección de marcha en seco (no bloqueante
    para un primer flasheo).
- Puedes hacer un primer flasheo solo con el módulo ESP32-H2 (sin pantalla/RTC/sensores
  conectados) únicamente para confirmar que **compila y sube** - varias pruebas de la sección 7
  necesitarán el hardware completo para responder.

## 2. Obtener el proyecto

Si ya tienes la carpeta local, sáltate este paso.

```
git clone https://github.com/fjrd07/Bomba-Goteo-Zigbee-ESP32H2.git
```

Abre la carpeta en VS Code (**File > Open Folder**).

## 3. Compilar (sin flashear todavía)

1. Espera a que PlatformIO termine de indexar (barra de progreso abajo a la izquierda).
2. Icono de **PlatformIO** en la barra lateral > **Project Tasks** >
   `env:esp32-h2-devkitm-1` > **General** > **Build**.
3. La primera vez tardará varios minutos descargando el framework `espressif32` y las
   librerías (`TFT_eSPI`, `TAMC_GT911`, `ArduinoJson`, `RTClib`).

**Si falla la compilación**: copia el error exacto. Si menciona algo dentro de
`zb_scheduler_lib.cpp` o símbolos de `esp_zigbee_core.h`, ve a la sección 0 de esta guía. Si
menciona `TFT_eSPI`, `RTClib` o `ArduinoJson`, revisa que `platformio.ini` haya descargado bien
esas librerías (`pio pkg list` o reintentar el build).

## 4. Conectar la placa

1. Conecta el ESP32-H2 al PC por USB-C.
2. Si PlatformIO no detecta el puerto automáticamente, pon la placa en **modo boot**:
   - Mantén presionado el botón **BOOT**.
   - Pulsa y suelta **RST**.
   - Suelta **BOOT**.

## 5. Flashear

1. En **Project Tasks**, click en **Upload** (o el ícono de flecha `→` en la barra inferior).
2. Cuando termine, pulsa **RST** para reiniciar la placa con el nuevo firmware.

## 6. Monitor Serie - qué deberías ver

1. Click en **Monitor** (o el ícono de enchufe en la barra inferior). Velocidad **115200**.
2. En un arranque sano deberías ver, en este orden aproximado:
   ```
   Display TFT IPS + Touch Initialized
   DS3231 no detectado ...            <- normal si aún no conectaste el RTC
   Zigbee Initialized (End Device, zb_scheduler_lib)
   Bomba-Riego-Goteo-Z2M Iniciado
   ```
3. Si se reinicia en bucle justo después de "Zigbee Initialized", es la señal más probable de
   que algo en `zb_scheduler_lib.cpp` necesita el ajuste de la sección 0.

## 7. Pruebas básicas por USB-Serial

Con el Monitor Serie abierto, uno por línea (ver
[`docs/hardware/IRRIGATION_SCHEDULING.md`](hardware/IRRIGATION_SCHEDULING.md) para el detalle
completo de cada comando):

```
HORA 1735689600              (pon aquí el epoch actual real, no este de ejemplo)
TZ COT5
NOMBRE 0 Prueba A
HORARIO 0 [{"en":true,"h":6,"m":0,"dur":10,"spd":100,"dow":127,"mon":4095,"dom":0}]
HORARIO? 0
```

Deberías ver `OK - ...` tras cada línea, y `HORARIO? 0` debe devolver el mismo JSON que
acabas de fijar.

## 8. Prueba táctil (modo manual)

Con la pantalla encendida (tócala si estaba dormida - el primer toque solo la despierta, hace
falta un segundo toque para que actúe):
- Tocar el botón grande **ON/OFF** de cada tarjeta debe encender/apagar esa bomba y cambiar la
  insignia a **MANUAL**.
- Tocar **"+"/"-"** debe subir/bajar la velocidad en pasos de 10% (visible en el texto `XX%`).
- Simular tanque vacío (float sensor) debe bloquear ambos botones y mostrar el aviso rojo.

## 9. Prueba de emparejamiento Zigbee

Sigue `README.md` sección 6 (instalar el convertidor externo en Z2M, activar "Permit join",
reiniciar la placa). El dispositivo debería aparecer como `Bomba-Riego-Goteo-Z2M` con los
endpoints On/Off (bombas) y el clúster de configuración (nombres/horario/hora) editables desde
Home Assistant.

## 10. Checklist rápido tras el primer flasheo

- [ ] Compila sin errores
- [ ] Sube sin errores
- [ ] Monitor Serie muestra el arranque sano (sección 6)
- [ ] Comandos Serie de la sección 7 responden `OK`
- [ ] Pantalla enciende y el toque responde tras el segundo toque
- [ ] Bombas cambian de estado/velocidad al tocar
- [ ] (Si tienes DS3231) `HORA` no vuelve a hacer falta tras un reinicio
- [ ] (Si tienes coordinador Zigbee a mano) el dispositivo se empareja y aparece en Z2M
