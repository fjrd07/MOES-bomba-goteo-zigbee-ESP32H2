# Lista de Materiales (BOM) - Bomba-Riego-Goteo-Z2M

Este documento y su contraparte máquina-legible [`BOM.csv`](BOM.csv) son la fuente de verdad
de componentes del proyecto. **Se actualizan en cada cambio de hardware** (ver la directiva en
`CLAUDE.md` en la raíz del repo) - si agregas, quitas o cambias un componente, actualiza ambos
archivos en el mismo cambio, no como una tarea aparte.

`BOM.csv` usa las columnas estándar que reconoce el plugin de BOM de KiCad
(`Designator, Value, Footprint, Quantity, Description, Datasheet, Manufacturer, MPN, Notes`) -
es el formato más portable para importar a cualquier herramienta de diseño de esquemáticos/PCB
(incluyendo proflow.ai), precisamente porque es un estándar de facto y no un formato propietario
que dependa de adivinar la API de una herramienta externa.

## ⚠️ Nota de fidelidad (leer antes de diseñar el PCB)

- **Los designadores (`U1`, `Q1`, `R1`, etc.) son propuestos por este proyecto, no provienen de
  un esquemático/PCB real todavía** - no existe un archivo KiCad en este repo aún. Úsalos como
  punto de partida al crear el esquemático, no como referencia a un diseño ya existente.
- **Marcado `[PLACEHOLDER]`**: valores que los propios documentos de este proyecto (`PINOUT.md`,
  `PUMP_DRY_RUN_DETECTION.md`, `POWER_BUDGET.md`) ya señalan como estimados/sin calibrar contra
  hardware real - no los tomes como definitivos sin verificar.
- **Marcado `[ALTERNATIVA]`**: dos opciones válidas documentadas para la misma función (p.ej.
  ACS712 vs shunt+INA180 para sensado de corriente) - elige una, no ambas, antes de fabricar.
- Todo lo demás refleja lo ya implementado/documentado en el firmware y los docs de hardware de
  este proyecto - no se inventó ningún componente nuevo solo para completar la tabla.

## Tabla de materiales

| Designador | Cant. | Componente | Valor/Parte | Encapsulado | Función | Estado | Fuente |
|---|---|---|---|---|---|---|---|
| U1 | 1 | Módulo MCU | ESP32-H2 "Supermini" (ESP32-H2-MINI-1) | Módulo SMD, 23.6x18mm | Zigbee 3.0 End Device, control general | Confirmado | `PIN_BREAKOUT.md` |
| U2 | 1 | Panel TFT IPS 3.5" | ST7796, 320x480, SPI | Módulo combo TFT+táctil | Interfaz visual | Confirmado | `platformio.ini`, `PINOUT.md` |
| U3 | 1 | Controlador táctil capacitivo | GT911 (integrado en U2) | Módulo combo TFT+táctil | Entrada táctil | Confirmado | `Config.h`, `DisplayHandler.cpp` |
| U4 | 1 | RTC externo | DS3231 (I2C, ±2ppm compensado en temperatura) | Módulo I2C, dir. 0x68 | Hora persistente/precisa (comparte bus con U3) | Confirmado | `ScheduleManager.cpp`, `IRRIGATION_SCHEDULING.md` |
| U5 | 1 | Regulador LDO 3.3V | AMS1117-3.3 **[ALTERNATIVA: HT7333]** | SOT-223 / SOT-89 | Alimenta ESP32-H2 + pantalla | Confirmado (marca genérica) | `PINOUT.md` |
| U6 | 1 | Cargador Li-ion (respaldo USB) | TP4054 | SOT23-5 | Carga por USB-C cuando hay poco sol | Confirmado | `PINOUT.md`, `POWER_BUDGET.md` |
| U7 | 1 | Controlador de carga solar Li-ion | CN3065 **[recomendado, no instalado aún]** | SOP-8 o similar | Carga desde el panel solar (no usar TP4054 directo con el panel) | Recomendado, pendiente de compra | `POWER_BUDGET.md` |
| U8 | 2 | Sensor de corriente Hall **[ALTERNATIVA A]** | ACS712-05B (5A) | Módulo | Detección de marcha en seco - uno por bomba, obligatorio (las dos bombas corren simultáneamente, no admite sensor compartido) | Alternativa A | `PUMP_DRY_RUN_DETECTION.md` |
| U9 | 2 | Amplificador de corriente **[ALTERNATIVA B, recomendado]** | INA180A1 (ganancia 20V/V) | SOT23-5 | Junto a R5/R6 (shunt), detección de marcha en seco - uno por bomba, obligatorio | Alternativa B | `PUMP_DRY_RUN_DETECTION.md` |
| Q1, Q2 | 2 | MOSFET N logic-level | AO3400A | SOT-23 | Interruptor de potencia Bomba A / Bomba B | Confirmado | `PINOUT.md` |
| D1, D2 | 2 | Diodo flyback | 1N4007 **[ALTERNATIVA: SS14]** | DO-41 / SMA | Protección contra picos inductivos de cada bomba | Confirmado | `PINOUT.md` |
| R1, R2 | 2 | Resistencia pull-down gate MOSFET | 10 kΩ | 0805 | Gate de Q1/Q2 a GND | Confirmado | `PINOUT.md` |
| R3, R4 | 2 | Divisor de voltaje batería | 100 kΩ / 100 kΩ **[PLACEHOLDER]** | 0805 | Escala la tensión de batería al rango ADC | Placeholder - ajustar al rango real de tu batería | `PINOUT.md` |
| R5, R6 | 2 | Resistencia shunt (si se usa Alternativa B) | 0.1 Ω, 1%, 1W | 2512 | Sensado de corriente low-side junto a U9 - una por bomba | Alternativa B | `PUMP_DRY_RUN_DETECTION.md` |
| C1 | 1 | Condensador de filtro/debounce | 100 nF | 0805 | Filtra ruido del sensor de nivel (debounce hardware) | Confirmado | `PINOUT.md` |
| SW1 | 1 | Interruptor de flotador (Reed Switch) | Genérico, normalmente cerrado/abierto según cableado | - | Sensor de nivel de depósito | Confirmado | `PINOUT.md` |
| BT1 | 2 | Celda Li-ion 18650 | 3500 mAh, en paralelo (7000 mAh total) | 18650 | Energía del sistema, autonomía 3 días nublados. Verificar que el circuito de protección soporte ~1.4A de descarga continua (ambas bombas pueden regar a la vez) | Recomendado | `POWER_BUDGET.md` |
| PV1 | 1 | Panel solar | 6V, 5-10W, monocristalino preferido | - | Recarga de BT1 | Recomendado | `POWER_BUDGET.md` |
| M1, M2 | 2 | Bomba de agua DC (goteo) | A definir por el usuario | - | Actuador de riego (velocidad vía PWM) | Pendiente (modelo del usuario) | `IRRIGATION_SCHEDULING.md` |
| J1 | 0-1 | Breakout de terminales de tornillo (accesorio) | 16 canales, genérico | Módulo | Cableado sin soldar directo al módulo U1 | Opcional | `PIN_BREAKOUT.md` |

## Notas de diseño para quien lleve esto a PCB/KiCad

- **U8 vs U9**: no instalar ambas familias - son dos formas de resolver la misma función
  (detección de marcha en seco). Ver `PUMP_DRY_RUN_DETECTION.md` para la comparación completa;
  U9+R5/R6 es la recomendación de este proyecto (mejor resolución, menor consumo en reposo).
- **Un sensor por bomba es obligatorio, no opcional**: las dos bombas de este proyecto pueden
  regar **al mismo tiempo** (confirmado - no hay interlock de exclusión mutua ni en el
  firmware ni planeado). Un sensor de corriente compartido entre ambas solo podría medir la
  suma de las dos corrientes cuando ambas están activas, sin forma de atribuir esa lectura a
  una bomba en particular - por eso se descartó esa opción (se consideró brevemente en el
  diseño y se corrigió) y el GPIO2/GPIO3 del MCU siguen dedicados uno a cada bomba.
- **U4 (DS3231) y U3 (GT911)** comparten el mismo bus I2C físico (`GPIO8`/`GPIO9`) - en el PCB,
  tender ambos como un solo bus con dos conectores/pads, no como buses separados.
- **U2 CS** está atado permanentemente a GND en vez de a un GPIO (ver `PIN_BREAKOUT.md` sección
  1) - en el esquemático, esa señal del panel TFT debe ir directo a GND, no a un pin del MCU.
- Pines exactos de cada componente: ver [`PINOUT.md`](../../PINOUT.md) (tabla completa) y
  [`PIN_BREAKOUT.md`](PIN_BREAKOUT.md) (mapeo físico al módulo U1).
