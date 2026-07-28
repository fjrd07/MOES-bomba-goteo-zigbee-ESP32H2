# Programador de Riego: Calendario, Velocidad y Nombres de Bomba

## Qué se implementó

Cada bomba (A y B) tiene su propio horario independiente, con hasta
`MAX_SCHEDULE_SLOTS_PER_PUMP` (4 por defecto) "slots" de riego. Cada slot combina en una sola
regla las cuatro dimensiones pedidas - diaria, semanal, mensual y anual - en vez de tener
cuatro sistemas de calendario separados:

| Campo | Qué controla | Rango |
|---|---|---|
| `h`, `m` | Hora de inicio (diario) | 0-23, 0-59 |
| `dur` | Duración en minutos (tiempo de encendido) | 1-1440 |
| `spd` | Velocidad (PWM del MOSFET, ver sección Velocidad) | 0-100% |
| `dow` | Días de la semana en que aplica (semanal) | bitmask, bit0=domingo .. bit6=sábado |
| `mon` | Meses en que aplica (anual/estacional) | bitmask, bit0=enero .. bit11=diciembre |
| `dom` | Días del mes en que aplica (mensual); `0` = cualquier día | bitmask de 31 bits, bit0=día1 |

Un slot se activa "ahora" si está habilitado **y** el día de la semana, el mes **y** el día
del mes coinciden **y** la hora actual cae dentro de `[inicio, inicio+duración)`. Con esto,
"todos los días a las 6:00 por 10 min" es tan válido como "solo los martes y jueves de
mayo a septiembre, los días pares del mes, a las 18:30 por 5 min a media velocidad" - un
único modelo cubre los cuatro ciclos pedidos sin necesitar cuatro motores de calendario
distintos.

Implementado en `src/ScheduleManager.h/.cpp`, con persistencia en NVS (flash) vía
`Preferences` - el horario, los nombres y la zona horaria sobreviven a un reinicio.

## Velocidad (PWM)

`PIN_PUMP_A`/`PIN_PUMP_B` pasaron de `digitalWrite` a PWM real vía LEDC
(`ledcAttach`/`ledcWrite`, `HardwareControl.cpp`), a `PUMP_PWM_FREQ_HZ` (5kHz por defecto,
`Config.h`). `setPumpA(bool)`/`setPumpB(bool)` siguen existiendo (100%/0%) para no romper el
código existente (touch, Zigbee); `setPumpASpeed()`/`setPumpBSpeed()` son las nuevas
funciones para velocidad parcial.

**Advertencia sin verificar contra hardware real**: muchas bombas pequeñas de diafragma no
arrancan de forma fiable por debajo de un ~30-40% de duty (par insuficiente a tensión
promedio baja) - calibra el rango útil de `spd` con tu bomba concreta, igual que se calibra
`PUMP_DRY_CURRENT_THRESHOLD_MA` en `PUMP_DRY_RUN_DETECTION.md`.

## Hora y zona horaria - RTC externo DS3231 (revisado)

**Revisión de una decisión anterior**: se evaluó usar el cristal de 32.768kHz nativo del
ESP32-H2 (pines `XTAL_32K_P`/`XTAL_32K_N`, que según el esquemático oficial del módulo -
`docs/hardware/esp32h2_supermini_pinout.png` - son **GPIO13 y GPIO14**, no GPIO12/13 como se
mencionó en un principio). Ese camino se descartó: este módulo solo tiene 15 GPIO en total y
GPIO13/14 ya están asignados a `LCD_MOSI`/`TOUCH_INT` - liberarlos habría exigido sacrificar
otra función (batería, sensado de corriente, o el táctil) sin necesidad real.

**Se usa en su lugar un DS3231** (RTC I2C, ~USD 1, cristal compensado en temperatura ±2ppm) -
comparte el bus I2C que ya usa el táctil (`PIN_TOUCH_SDA`=GPIO8, `PIN_TOUCH_SCL`=GPIO9;
dirección del DS3231 = `0x68`, no colisiona con el GT911 en `0x5D`/`0x14`). **Cero pines GPIO
nuevos**, mejor precisión real a la intemperie que un cristal desnudo en el ESP32 (la
compensación de temperatura del DS3231 es justo lo que mitiga la deriva térmica mencionada
como objetivo), y sobrevive a un corte de energía total con su propia pila de respaldo -
algo que el reloj de software del ESP32 no puede hacer por sí solo.

Diseño (`ScheduleManager.cpp`):
- **Arranque**: `initScheduleManager()` inicializa el DS3231 y, si tiene una hora válida
  (`!rtc.lostPower()`), siembra el reloj de sistema del ESP32 (`settimeofday()`) con ella -
  toda la lógica de calendario (`localtime_r()`, zona horaria vía
  `setenv("TZ",...)`/`tzset()`, `DEFAULT_TZ_POSIX` en `Config.h`) sigue funcionando sin
  cambios sobre ese reloj de sistema.
- **Re-sincronización periódica**: cada `RTC_RESYNC_INTERVAL_MS` (1 hora) se vuelve a leer el
  DS3231 y se corrige el reloj de sistema (`resyncFromRtcIfDue()`), para que cualquier deriva
  del oscilador interno del ESP32 entre lecturas no se acumule.
- **Escritura**: `setEpochTime()` (llamada desde Serial o desde el atributo Zigbee
  `epochTime`) actualiza tanto el reloj de sistema como el propio DS3231 (`rtc.adjust()`) -
  así el DS3231 también queda correcto y sigue sirviendo la hora tras un reinicio, sin
  depender de que Zigbee/HA estén disponibles en ese momento.
- **Detección de fallos**: si el DS3231 no responde en el bus (`!rtc.begin()`) o nunca tuvo
  una hora válida (`rtc.lostPower()`), el sistema se degrada exactamente al comportamiento
  original (reloj solo en RAM, se pierde en cada reinicio) - no rompe nada, solo pierde la
  persistencia extra.

### Sincronización periódica vía Zigbee/HA (complementaria, no crítica ahora)

Con el DS3231 en su lugar, el dispositivo ya mantiene hora precisa por sí mismo sin depender
de la red Zigbee. Aun así, para corregir cualquier desviación de muy largo plazo (o si el
DS3231 nunca se sincronizó la primera vez), el atributo Zigbee `epochTime` (clúster de
configuración, ver sección siguiente) ya acepta escrituras en cualquier momento - la forma
más simple de lograr "sincronizado cada cierto tiempo, aun en sleep, vía HA" es una
**automatización de Home Assistant** que publique la hora actual a ese atributo cada 24h
(o al detectar que el dispositivo se reconectó tras un corte). No se implementó un
mecanismo de lectura activa del Cluster Time de Zigbee (0x000A) *desde* el propio
dispositivo hacia el coordinador - habría añadido una llamada ZCL cliente adicional en
`zb_scheduler_lib.cpp` con el mismo nivel de incertidumbre ya señalado para esa librería, sin
aportar nada que la automatización de HA (mucho más simple y ya soportada) no resuelva igual.

## Nombres de bomba (renombrar identidades)

`setPumpName(idx, "Tomates")`/`getPumpName(idx)` en `ScheduleManager`, persistente en NVS.
Editable como texto directamente desde Home Assistant/Z2M (ver sección siguiente) o por
Serial con `NOMBRE <0|1> <texto>`.

## Transporte de configuración vía Zigbee - librería propia (`zb_lib/zb_scheduler_lib`)

La librería Zigbee de arduino-esp32 (`Zigbee.h`) no tiene un tipo de dispositivo "programador
de riego", y no hay forma de verificar sin su código fuente si expone una manera segura de
añadir atributos custom (string/JSON) a un endpoint sin arriesgar el despacho interno de
eventos que ya usan los endpoints On/Off. En vez de mezclar esa capa de alto nivel con
llamadas de bajo nivel (lo que podía romper el manejador de eventos compartido), se construyó
**una librería Zigbee propia para este proyecto** (`src/zb_lib/zb_scheduler_lib.h/.cpp`)
directamente sobre `esp-zigbee-sdk` - el SDK real de Espressif que `Zigbee.h` envuelve -, con
una única implementación consistente para todo el dispositivo:

- Endpoints 1 y 2: On/Off estándar para Bomba A/B (mismo comportamiento que antes).
- Endpoint 4: clúster manufacturer-specific `0xFC00` (rango reservado por la especificación
  ZCL para uso no estándar - esto es un hecho del estándar, no una suposición sobre esta
  librería) con 6 atributos: `pumpAName`, `pumpBName` (Character String), `pumpASchedule`,
  `pumpBSchedule` (Long Character String, para el JSON), `timezone` (Character String) y
  `epochTime` (Unsigned 32-bit Integer, escribible para sincronizar el reloj).
- Un único manejador de acciones ZCL (`zbActionHandler`) para todo - sin el riesgo de que un
  segundo manejador registrado por separado pisara al de los endpoints On/Off.
- El clúster de configuración correspondiente ya está declarado en
  `Z2M/Bomba-Riego-Goteo-Z2M.js` (`deviceAddCustomCluster` + `text()`/`numeric()`), con los
  mismos IDs de clúster/atributo - editable como texto/número normal desde Home Assistant.

### ⚠️ Honestidad sobre el nivel de confianza de esta implementación

Se escribió con el mayor cuidado posible, modelada explícitamente sobre los ejemplos
oficiales de Espressif (`esp-zigbee-sdk/examples/esp_zigbee_HA_sample/HA_on_off_light` para
el arranque del stack y los clústeres estándar; `esp_zigbee_customized_devices` para el
clúster manufacturer-specific), pero **no se compiló ni se probó contra hardware real** en
esta sesión (sin acceso a compilador ni placa). Los nombres de función/struct de
`esp-zigbee-sdk` pueden variar levemente entre versiones del SDK. Si algo no compila, en este
orden de probabilidad:

1. `esp_zb_stack_main_loop()` (la tarea dedicada en `zbSchedulerInit()`) - el nombre exacto
   de la función de bucle principal es lo menos seguro de esta implementación. Si no existe,
   busca en tu SDK instalado (`esp_zigbee_core.h`) el equivalente - suele llamarse algo como
   `esp_zb_main_loop_iteration()` para una sola iteración no bloqueante, en cuyo caso hay que
   envolverla en un `while(true)` dentro de la tarea en vez de llamarla una sola vez.
2. `esp_zb_custom_cluster_add_custom_attr()` / `esp_zb_zcl_attr_list_create()` - nombres de
   las funciones para crear el clúster custom y añadirle atributos - ver
   `esp_zigbee_customized_devices` en tu SDK para el nombre exacto en tu versión.
3. Los campos de `esp_zb_zcl_set_attr_value_message_t` (usados en `zbActionHandler` para leer
   qué atributo cambió) - la estructura general (`info.cluster`, `info.dst_endpoint`,
   `attribute.id`, `attribute.data.value`) es estable entre versiones recientes, pero
   verifica contra el struct real si el compilador se queja de un campo.
4. Los IDs de tipo de dato ZCL (`0x42`=Character String, `0x44`=Long Character String,
   `0x23`=Unsigned 32-bit) son del estándar ZCL, no de esta librería - no deberían cambiar.

**El canal por USB-Serial (sección siguiente) sigue disponible siempre**, tanto como
respaldo si algo de lo anterior no compila en tu versión exacta del SDK, como para
configuración/depuración local incluso una vez el transporte Zigbee esté confirmado.

## Canal de configuración por USB-Serial

Por el Monitor Serie (115200 baudios), un comando por línea:

```
NOMBRE <0|1> <texto>       Renombra la Bomba A(0) o B(1)
HORARIO <0|1> <json>       Fija el horario de esa bomba (array JSON de slots, ver esquema abajo)
HORARIO? <0|1>             Imprime el horario actual de esa bomba en JSON
TZ <posix_tz>              Fija la zona horaria (formato POSIX, ver DEFAULT_TZ_POSIX)
HORA <epoch_segundos>      Sincroniza el reloj interno (obligatorio para que el horario funcione)
```

### Ejemplo: Bomba A todos los días a las 6:00, 10 minutos, 100% de velocidad

```
HORA 1735689600
TZ COT5
HORARIO 0 [{"en":true,"h":6,"m":0,"dur":10,"spd":100,"dow":127,"mon":4095,"dom":0}]
```

### Ejemplo: Bomba B solo martes/jueves, mayo-septiembre, a las 18:30, 5 min, 60% de velocidad

```
HORARIO 1 [{"en":true,"h":18,"m":30,"dur":5,"spd":60,"dow":20,"mon":496,"dom":0}]
```
(`dow`=20 -> bits 2 y 4 -> martes y jueves; `mon`=496 -> bits 4-8 -> mayo a septiembre)

## Interacción con el toque manual y el interlock de seguridad

- Un toque en pantalla (o un comando Zigbee ON/OFF, ver `ZigbeeHandler.cpp`) marca esa bomba
  como "override manual" - el programador no la toca hasta el **próximo cambio real** de lo
  que pide el horario (inicio o fin de una ventana), momento en el que retoma el control
  automático. Así un riego manual de emergencia no queda peleando con el horario en el mismo
  ciclo, pero tampoco bloquea el horario indefinidamente.
- El interlock de seguridad (`bloqueoSeguridad`, tanque vacío) manda siempre - el programador
  ni el comando Zigbee pueden encender una bomba mientras está activo.
