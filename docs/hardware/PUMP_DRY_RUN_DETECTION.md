# Detección Indirecta de Tanque Vacío por Corriente de Succión

## El problema

El sensor de nivel actual (flotador en `PIN_LEVEL_SENS`) es la fuente de verdad principal para
saber si el depósito está vacío. Pero un flotador puede fallar mecánicamente, ensuciarse, o
simplemente no estar disponible/instalado en una unidad concreta. La pregunta: ¿se puede saber
que el tanque está vacío **solo observando cómo trabajan las bombas**, sin un sensor de nivel?

## La técnica: corriente de succión, no velocidad

No hay forma de leer la velocidad real de estos motores DC de bomba sin un sensor dedicado
(no tienen encoder ni salida de tacómetro), así que "por velocidad" no es viable sin hardware
adicional. **Por corriente sí es viable** y es la técnica estándar en electrodomésticos con
bombas (lavavajillas, lavadoras) para detectar "marcha en seco":

- Una bomba empujando agua contra la resistencia hidráulica de la tubería/altura tiene una
  carga mecánica real sobre el motor → corriente estable y más alta.
- Una bomba girando en seco (sin agua, succionando aire) casi no tiene carga mecánica →
  corriente más baja y, en muchos motores DC de escobillas, más "temblorosa" (menos estable)
  porque el motor gira más rápido de lo normal sin resistencia de fluido.

Esta diferencia es medible con un sensor de corriente barato en serie con cada bomba - **no es
posible medirla con el hardware actual del proyecto**, porque `PIN_PUMP_A`/`PIN_PUMP_B` son
salidas digitales de control (encienden el MOSFET) y no dan ninguna señal de vuelta sobre cuánto
está trabajando el motor.

## Hardware necesario: dos opciones válidas

Ambas opciones entregan lo mismo al firmware - un voltaje analógico proporcional a la
corriente - y usan exactamente el mismo código (`CURRENT_SENSOR_MV_PER_AMP`/
`CURRENT_SENSOR_ZERO_MV` en `Config.h`, ver más abajo). Solo cambia el hardware.

### Opción A - ACS712 (Hall, plug-and-play)

Un módulo **ACS712 de 5A** por bomba - más que suficiente para bombas de goteo pequeñas
(normalmente <1A). Viene en una placa ya soldada (solo VCC/GND/señal, sin SMD), es la opción
más simple si no quieres soldar componentes pequeños.

```
Batería (+) ---[ACS712 IP+]---[ACS712 IP-]--- Bomba (+)
                    |
                 (salida analógica, VCC, GND)
                    |
              GPIO2 (Bomba A) / GPIO3 (Bomba B)
```

Sensibilidad ~185mV/A, salida centrada en ~Vcc/2 (1.65V con alimentación a 3.3V).

### Opción B - Shunt + INA180/INA181 (mi recomendación para este proyecto)

**Esta es la que recomendaría** para este caso concreto: son bombas pequeñas de bajo consumo
en un sistema alimentado por batería+solar (ver sección de energía en `PINOUT.md`), donde
importan el costo por unidad, el consumo en reposo y sobre todo la **resolución** - la
diferencia de corriente entre "bombeando agua" y "en seco" en una bomba pequeña puede ser de
solo unos cientos de mA, y el ACS712 (185mV/A) deja poco margen en un ADC de 12 bits. Diseño
concreto:

- **Shunt**: resistencia de precisión de **0.1 Ω, 1% tolerancia, paquete 2512** (potencia:
  a 1A disipa `I²R` = 0.1W, un 2512 aguanta 1W con margen de sobra).
- **Amplificador**: **INA180A1** (ganancia fija 20V/V), lado bajo (entre el Source del MOSFET y
  GND) - sensado *low-side*, referenciado a GND, sin necesidad de centrar en Vcc/2.
- **Sensibilidad resultante**: `0.1 Ω × 20 V/V = 2 V/A` = **2000 mV/A** - ~10x más sensible que
  el ACS712, y con corriente=0 el offset es ~0V (no 1.65V), así que se aprovecha todo el rango
  del ADC (0-3.3V) para medir corriente en vez de "gastar" la mitad del rango en el offset.
- Con este par (0.1Ω + ganancia 20), 1A de corriente da 2V de salida - dentro del rango del ADC
  de 3.3V con margen para arrancadas (inrush) más altas.

```
MOSFET Source ---[Shunt 0.1Ω]--- GND
       |                  |
      IN+ (INA180)      IN- (INA180)
                |
           OUT (0-3.3V, ~2V/A) --- GPIO2 (Bomba A) / GPIO3 (Bomba B)
```

El INA180 viene en SOT23-5 (requiere soldadura SMD, más trabajo manual que un módulo ACS712 ya
armado) pero es más barato por unidad en volumen y da mejor resolución para este caso de uso.
Si no tienes experiencia soldando SMD, la Opción A (ACS712) sigue siendo válida - solo pierdes
algo de resolución, no funcionalidad.

### Dónde conectarlos en el módulo ESP32-H2 Supermini

`PIN_PUMP_A_ISENSE` (GPIO2) y `PIN_PUMP_B_ISENSE` (GPIO3) están en el **header izquierdo** del
módulo, justo debajo de los pines de las bombas: de arriba a abajo el header izquierdo es
`TX, RX, 0 (Bomba A), 1 (Bomba B), 2 (I-sense A), 3 (I-sense B), 4, 5, 26, 27, 8` (ver
[`PIN_BREAKOUT.md`](PIN_BREAKOUT.md), sección 1) - físicamente son los dos pines inmediatamente
después de los de control de las bombas, lo que hace el cableado corto y directo: MOSFET de
Bomba A → shunt/ACS712 → GPIO2, mismo patrón para Bomba B → GPIO3. Ambos son ADC1
(`ADC1_CH1`/`ADC1_CH2` en el diagrama de catálogo), sin conflicto con ninguna otra función del
proyecto.

## Cómo funciona la detección en el firmware

Implementado en `HardwareControl.cpp` (`updatePumpDryRunDetection`, `isPumpADry`/`isPumpBDry`)
y conectado en `main.cpp` (`verificarNivelAgua`):

1. Al encender una bomba, se ignoran los primeros `PUMP_STARTUP_IGNORE_MS` (400 ms) - el pico de
   arranque (inrush) del motor no es representativo de la carga real en régimen.
2. A partir de ahí, cada ciclo de `loop()` compara la corriente instantánea contra
   `PUMP_DRY_CURRENT_THRESHOLD_MA`.
3. Si la corriente se mantiene **por debajo** del umbral durante `PUMP_DRY_CONFIRM_MS` (2s)
   seguidos, esa bomba se marca como "en seco" (`isPumpADry()`/`isPumpBDry()` devuelven `true`).
4. `verificarNivelAgua()` combina esta señal con el flotador según `LEVEL_DETECTION_MODE`
   (`Config.h`):
   - `LEVEL_MODE_FLOAT_ONLY`: solo flotador (comportamiento original).
   - `LEVEL_MODE_CURRENT_ONLY`: solo corriente (para unidades sin flotador instalado).
   - `LEVEL_MODE_HYBRID` (**por defecto**): basta con que cualquiera de las dos señales
     detecte vacío para activar el bloqueo de seguridad - es la opción más segura porque
     nunca depende de un único sensor.

## ⚠️ Calibración obligatoria antes de confiar en esto

**Los valores en `Config.h` (`PUMP_DRY_CURRENT_THRESHOLD_MA`, `CURRENT_SENSOR_ZERO_MV`,
`CURRENT_SENSOR_MV_PER_AMP`) son placeholders** - no se han medido contra tus bombas reales,
porque eso depende del modelo de bomba, el voltaje y el sensor elegido (Opción A o B arriba).
Procedimiento:

1. Con el sensor cableado y la bomba apagada, mide `readPumpACurrentMA()`/`readPumpBCurrentMA()`
   por el Monitor Serie - debería estar cerca de 0. Si no, ajusta `CURRENT_SENSOR_ZERO_MV` al
   voltaje real de offset que reporta tu sensor en reposo (será ~1650mV para el ACS712 alimentado
   a 3.3V, o ~0mV para el shunt+INA180 de la Opción B).
2. Sumerge la bomba en agua, enciéndela, y anota la corriente estable que reporta (ignora los
   primeros ~1-2s de arranque).
3. Saca la bomba del agua (o tápale la entrada) y enciéndela brevemente en seco - anota esa
   corriente. **No la dejes girar en seco más de unos segundos**, muchas bombas de diafragma se
   dañan por marcha en seco prolongada.
4. Fija `PUMP_DRY_CURRENT_THRESHOLD_MA` a medio camino entre ambas lecturas (con margen de
   seguridad hacia el lado "seco" para evitar falsos positivos por variaciones de tensión de
   batería).

## Alternativa sin hardware adicional (no implementada)

Si no quieres añadir un sensor de corriente, existe una técnica más burda: observar la caída de
voltaje de la batería (`PIN_BATTERY`, ya existente) justo al encender cada bomba - una bomba
cargada (bombeando agua) provoca una caída de tensión mayor que una bomba en seco, por la
resistencia interna de la batería. **No se implementó** porque es mucho menos fiable (se
confunde con el estado de carga de la batería, su temperatura, y con ambas bombas encendidas a
la vez) y este proyecto ya usa esa medición para reportar el nivel de batería a Home Assistant -
mezclar ambos usos degradaría la precisión de los dos. Se documenta aquí solo como idea, no como
código funcional.
