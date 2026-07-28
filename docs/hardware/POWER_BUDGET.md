# Presupuesto de Energía: Batería y Panel Solar

## ✅ Hallazgo crítico - ya corregido

`DisplayHandler.cpp`/`main.cpp` mantenían la pantalla TFT **encendida y con backlight al 100%
todo el tiempo** (`updateDisplayFull()` se llamaba cada segundo, sin ningún timeout de
apagado/dimming). El backlight de un panel IPS de 3.5" es, con diferencia, el mayor consumidor
del sistema (~80-150 mA él solo) - ver [`DISPLAY_OPTIONS.md`](DISPLAY_OPTIONS.md) sección 0
para el detalle de la corrección ya implementada (apagado de backlight + sleep del GT911 tras
`BACKLIGHT_TIMEOUT_MS` sin toques). El **Escenario B** de abajo asume esta corrección aplicada;
el **Escenario A** muestra qué pasaba antes, para dimensionar con conocimiento de causa.

## Supuestos (ajusta con tus valores reales - son la variable que más mueve el resultado)

| Consumo | Valor asumido | Nota |
|---|---|---|
| ESP32-H2 (radio Zigbee activa, sin WiFi) | 30 mA promedio | Duty-cycled, valor típico de catálogo |
| Backlight TFT 3.5" IPS | 120 mA | **Dominante** - depende del brillo real configurado |
| Lógica display + controlador táctil GT911 | 30 mA | |
| 2x sensor de corriente ACS712 | 20 mA | Quiescente propio del chip Hall (~10mA c/u) |
| 2x sensor de corriente shunt+INA180 (alternativa) | ~1 mA | INA180 consume ~370µA c/u - mucho mejor para batería |
| Bomba A / Bomba B (cada una, mientras riega) | 600 mA c/u | Asumido - depende del modelo real de bomba |
| Riego por bomba, por día | 15 min/día **cada una** | **Placeholder** - depende de tu programa de riego real. **Corrección: las dos bombas pueden regar simultáneamente** (confirmado, no hay interlock de exclusión mutua) - esto no cambia mucho la energía diaria total (ver abajo) pero sí el **pico de corriente instantáneo**, que ahora se dimensiona como 2x600mA = 1.2A, no 600mA. |

## Escenario A - firmware original, sin el fix de la sección anterior (pantalla siempre encendida)

- Consumo continuo: 30 + 120 + 30 + 20 (ACS712) ≈ **200 mA promedio**
- Energía diaria (continuo): 200 mA × 24h = **4.8 Ah/día**
- + Bombas: 0.6A × (20/60)h ≈ 0.2 Ah/día
- **Total ≈ 5.0 Ah/día**

Un 18650 típico (2500-3500 mAh) se agotaría en **menos de un día**. Este escenario no es viable
con batería+solar en un formato de dispositivo IoT pequeño - se documenta para mostrar por qué
el modo de bajo consumo de la pantalla no es opcional en un diseño solar real.

## Escenario B - con el fix de bajo consumo de pantalla ya aplicado (recomendado)

Backlight apagado salvo interacción táctil reciente (✅ implementado), CPU en light-sleep entre
iteraciones de `loop()` en vez de `delay()` puro (✅ implementado, `esp_light_sleep_start()` en
`main.cpp`), sensores de corriente vía shunt+INA180 (no ACS712, ver
[`PUMP_DRY_RUN_DETECTION.md`](PUMP_DRY_RUN_DETECTION.md), no implementado - es una elección de
hardware, no de firmware):

- Consumo continuo: ~25-35 mA promedio (redondeo: 30 mA)
- Energía diaria (continuo): 30 mA × 24h = **0.72 Ah/día**
- + Bombas: 2 bombas × 0.6A × (15/60)h = **0.3 Ah/día** (la energía total no cambia mucho por
  correr simultáneas o no - es la misma cantidad de "amperio-minutos" acumulados en el día;
  lo que sí cambia es el pico instantáneo, ver más abajo)
- **Total ≈ 1.02 Ah/día ≈ 1 Ah/día** (prácticamente igual al cálculo anterior)

### ⚠️ Corrección: pico de corriente por operación simultánea de bombas

La **capacidad** de batería (Ah/día) apenas cambia por que las bombas corran juntas o no - lo
que sí hay que dimensionar distinto es el **pico de corriente instantáneo**:

- Pico máximo: 2 × 600 mA (bombas) + ~30-200 mA (electrónica) ≈ **hasta 1.4 A**
- **Batería**: un pack de 7000mAh (ver dimensionamiento abajo) entregando 1.4A es apenas
  ~0.2C - muy cómodo para cualquier celda 18650 (la mayoría soporta 1-2C continuo, es decir
  7-14A en este pack). No hace falta más capacidad de batería solo por el pico.
- **Circuito de protección de la batería** (si el pack 18650 trae uno, como suele ser
  habitual): verificar que su corriente máxima de descarga continua supere ~1.5-2A con
  margen - la mayoría de los baratos están clasificados 3-8A, normalmente no es un problema,
  pero es el punto concreto a revisar en la hoja de datos antes de armar el pack.
- **Calibre de cable/pista de PCB**: dimensionar el retorno común a batería (GND compartido
  de ambas bombas, si comparten ese tramo) para el pico combinado (~1.4A), no solo para
  600mA por rama. Cada MOSFET (Q1/Q2) individualmente solo ve la corriente de SU propia
  bomba (≤600mA), muy por debajo del límite del AO3400A (~5.7A continuo) - no hay que
  cambiar el MOSFET, solo el tramo de retorno compartido si existe.
- **Regulador 3.3V / panel solar / IC de carga**: sin cambios - alimentan la lógica y cargan
  la batería respectivamente, ninguno de los dos está en la ruta de descarga hacia las
  bombas, así que el pico de corriente de las bombas no los afecta.

**⚠️ Sin verificar en hardware real**: el light-sleep de la CPU (`main.cpp`, `loop()`) no se ha
probado contra el scheduling interno de la librería Zigbee de arduino-esp32 mientras el
dispositivo está unido a una red. Si notas caídas de conexión, comandos que tardan en llegar, o
desconexiones de Z2M, quita ese bloque (`esp_sleep_enable_timer_wakeup`/`esp_light_sleep_start`
en `loop()`) y vuelve a `delay(LOOP_SLEEP_MS);` - el resto de esta sección (backlight/GT911) no
se ve afectado por ese cambio en absoluto.

## Dimensionamiento de batería (autonomía de 3 días nublados, CERO sol)

Regla estándar de diseño solar off-grid: la batería sola debe cubrir los días de autonomía sin
ninguna contribución solar, dejando un margen de profundidad de descarga (DoD) del 80% para no
degradar la celda de Li-ion prematuramente:

```
Batería (Ah) = consumo_diario × días_autonomía / DoD
             = 1.02 Ah/día × 3 días / 0.8
             ≈ 3.83 Ah
```

**Recomendación (sin cambios respecto a la versión anterior): 2x 18650 de alta capacidad
(3500 mAh c/u) en paralelo = 7000 mAh.** La corrección de que ambas bombas pueden regar
simultáneamente apenas mueve este número (3.75→3.83 Ah, la energía diaria casi no cambia por
el traslape) - lo que sí cambia es el pico de corriente a soportar (~1.4A, ver sección
anterior), y 7000mAh sigue dando margen de sobra para eso. Con una sola 18650 (3500mAh)
quedarías casi exactamente en el mínimo calculado, sin margen de seguridad.

**Voltaje**: mantener **1S (una celda, 3.0-4.2V, nominal 3.7V)**, coherente con el `TP4054`/
`CN3163` y el escalado de `readBatteryVoltage()` ya usados en el proyecto. No hay necesidad de
subir a 2S/7.4V para esta carga - complicaría el regulador y el sensado de batería sin
beneficio real aquí.

## Dimensionamiento del panel solar

Un día nublado entrega aproximadamente **10-25% de la irradiancia de un día despejado** (se usa
15% como estimado conservador). El panel debe:
1. Aportar algo incluso en los días nublados (reduce cuánto se vacía la batería).
2. Recuperar el déficit de los 3 días nublados en 1-2 días de sol normal después.

Con ~4.5 horas-sol-pico/día equivalentes (razonable para Colombia) y ~75% de eficiencia de
carga (pérdidas del regulador + no-MPPT real):

```
Potencia mínima (romper el equilibrio en un día normal):
  1.0 Ah/día × 3.7V / (4.5h × 0.75) ≈ 1.1 W

Con el factor de sobredimensionamiento 2-3x que se usa siempre en diseño solar
(para cubrir ángulo de incidencia, suciedad, días parcialmente nublados):
  ≈ 3-5 W mínimo recomendado
```

**Recomendación: panel de 6V, 5-10W** (tamaño común y económico). Con un panel de 10W:
- Día normal: recarga completa en pocas horas de sol con margen de sobra.
- Día nublado (~15% de salida): ~1.5W ≈ 0.3-0.4A a 4-5V - no cubre el consumo del Escenario B
  por completo, pero reduce bastante cuánto se consume de la reserva de 3 días.
- Tras los 3 días nublados, un panel de 10W recupera el déficit de ~3 Ah en un par de horas de
  sol normal.

**Controlador de carga**: no uses el `TP4054`/`CN3163` actual directamente desde el panel - esos
son cargadores pensados para una fuente USB estable de 5V, no para un panel solar (voltaje/
corriente variables con la luz). Usa un **IC de carga solar para Li-ion de una celda** (p.ej.
`CN3065` o similar, con entrada tolerante a paneles de 4.5-6V) entre el panel y la batería;
mantén el `TP4054` como respaldo para cargar por USB-C cuando haya poco sol.

## Resumen de recomendación

| Ítem | Valor |
|---|---|
| Batería | 2x 18650 3500mAh en paralelo (7000mAh), 1S / 3.7V nominal (sin cambio - ver nota de pico de corriente) |
| Panel solar | 6V, 5-10W (monocristalino preferido sobre policristalino por mejor rendimiento con luz difusa/nublado) - sin cambio |
| Controlador de carga | IC solar Li-ion 1S (p.ej. CN3065) en vez del TP4054 actual, conservando el TP4054 como carga de respaldo por USB-C |
| Pico de corriente a verificar (nuevo, por bombas simultáneas) | ~1.4A - confirmar que el circuito de protección del pack 18650 soporta esa descarga continua, y dimensionar el retorno común a GND para ese pico, no solo 600mA |
| Cambio de firmware recomendado (no implementado) | Apagar backlight tras inactividad táctil - **✅ ya implementado**, ver `DISPLAY_OPTIONS.md` |

Todo lo anterior depende de los supuestos de la tabla al inicio (sobre todo brillo real del
backlight y minutos de riego/día por bomba) - si me das esos números medidos con tu hardware
real, puedo recalcular con precisión en vez de con placeholders.
