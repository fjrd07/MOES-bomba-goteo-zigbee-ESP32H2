# Bomba-Riego-Goteo-Z2M - Directivas del Proyecto

Controlador de riego dual (ESP32-H2, Zigbee 3.0) con programador de riego, pantalla táctil,
batería+solar. Ver `README.md` para la descripción general y `docs/hardware/` para el detalle
técnico completo (pinout, esquemáticos, presupuesto de energía, programador de riego, BOM).

## Directiva: mantener el BOM actualizado en cada cambio de hardware

**Cada vez que un cambio de este proyecto agregue, quite o modifique un componente físico**
(sensor, MOSFET, RTC, batería, panel solar, conector, etc.) - no solo firmware - actualiza en
el MISMO cambio, no como tarea aparte:

- [`docs/hardware/BOM.md`](docs/hardware/BOM.md) - tabla legible con designador, componente,
  valor/parte, encapsulado, función, estado y la fuente (qué doc lo respalda).
- [`docs/hardware/BOM.csv`](docs/hardware/BOM.csv) - misma información en el formato estándar
  que reconoce el plugin de BOM de KiCad (`Designator, Value, Footprint, Quantity, Description,
  Datasheet, Manufacturer, MPN, Notes`) - úsalo para exportar a cualquier herramienta externa de
  diseño de esquemáticos/PCB (incluida proflow.ai), ya que es un formato portable y no requiere
  adivinar la API propietaria de una herramienta específica.

Reglas al editar el BOM:
- **Retiro de un componente**: elimina su fila de ambos archivos, no la dejes comentada/tachada.
- **Adición**: asigna el siguiente designador libre de su categoría (U=circuitos integrados/
  módulos, Q=transistores/MOSFET, D=diodos, R=resistencias, C=condensadores, BT=baterías,
  PV=paneles solares, M=motores/bombas, J=conectores, SW=interruptores/sensores mecánicos).
- **Nunca inventes un número de parte (MPN) o footprint que no esté ya documentado en otro
  archivo de `docs/hardware/`** - si no hay una fuente clara, dejar el campo genérico
  ("Generic") o marcarlo `[PLACEHOLDER]`/`[ALTERNATIVA]` según corresponda, igual que ya se
  hace en el resto del BOM. El objetivo es que una herramienta externa (o un humano) pueda
  confiar en el archivo sin tener que re-verificar cada celda.
- Si hay dos opciones válidas para la misma función (p.ej. ACS712 vs shunt+INA180), documenta
  ambas como alternativas explícitas, nunca combines cantidades de las dos como si se
  necesitaran ambas a la vez.

## Contexto técnico que no debe re-derivarse desde cero

- El módulo MCU (`ESP32-H2 Supermini`) solo expone **15 GPIO utilizables en total**:
  `0,1,2,3,4,5,8,9,10,11,12,13,14,23,24`. GPIO6, 7, 15, 22, 25 NO existen en su header - ver
  `docs/hardware/PIN_BREAKOUT.md`. Cualquier adición de hardware que necesite un GPIO debe
  partir de este inventario, no asumir que "sobra un pin".
- El stack Zigbee de este proyecto (`src/zb_lib/zb_scheduler_lib.cpp`) está escrito directo
  sobre `esp-zigbee-sdk`, no sobre el wrapper de alto nivel `Zigbee.h` de arduino-esp32 - ver
  ese archivo para el porqué antes de proponer volver a la librería de alto nivel.
- El reloj usa un RTC externo DS3231 (I2C, comparte bus con el táctil) en vez de los pines
  nativos `XTAL_32K` del ESP32-H2 (que son GPIO13/14, ya ocupados por LCD_MOSI/TOUCH_INT) - ver
  `docs/hardware/IRRIGATION_SCHEDULING.md`.
