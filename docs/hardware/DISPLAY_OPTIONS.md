# Opciones de Pantalla Táctil de Bajo Consumo (3.5" diagonal)

Contexto: el mayor consumidor del sistema actual es el backlight del TFT IPS 3.5" (~80-150mA
mientras esté encendido, ver [`POWER_BUDGET.md`](POWER_BUDGET.md)). Este documento compara
alternativas para bajar ese consumo, manteniendo el tamaño físico (~3.5" diagonal, mismo formato
que el panel actual) y la capacidad táctil.

## 0. Antes de cambiar hardware: arreglar el software (gratis, mayor impacto) - ✅ implementado

Con el **mismo TFT IPS + GT911 actual**, dos cambios de firmware recuperan la mayor parte del
consumo sin comprar nada nuevo (implementados en `DisplayHandler.cpp`/`main.cpp`,
constantes en `Config.h`):

- **Backlight**: se apaga (`PIN_LCD_BL` a `LOW`) tras `BACKLIGHT_TIMEOUT_MS` (20s por defecto)
  sin toques, vía `updateDisplayPowerSaving()` llamado en cada `loop()`. También se deja de
  redibujar la pantalla cada segundo mientras está dormida (`gestionarInterfaz()` corta antes).
- **GT911**: al mismo tiempo se le envía el comando de sleep documentado por el fabricante
  (escritura I2C directa al registro `0x8040` = `0x05`, función `gt911EnterSleep()` en
  `DisplayHandler.cpp`) - el chip sigue detectando toques en su propio modo de bajo consumo
  ("Green mode") y los reporta con normalidad por I2C en cuanto se le toca, así que no hace
  falta ninguna secuencia especial de "despertar": el `tp.read()` de siempre lo detecta.

**Pendiente de verificar en hardware real**: la dirección I2C del GT911 (`GT911_I2C_ADDR` en
`Config.h`) se asumió en `0x5D` (la más común). Si tras flashear el toque deja de despertar la
pantalla, cambia esa constante a `0x14` - es la otra dirección posible según cómo se strapeó el
pin `INT` durante el reset de fábrica del panel, y no hay forma de saber cuál sin probar en la
placa concreta.

**✅ Prevención de toques accidentales al despertar (implementado)**: `checkTouch()` en
`DisplayHandler.cpp` distingue si la pantalla estaba dormida antes del toque - si lo estaba,
ese primer toque **solo** enciende backlight + saca al GT911 de reposo, y se descarta como
acción (no procesa botones). Hace falta un segundo toque, ya con la pantalla despierta, para
que `handleTouch()` interprete taps sobre los controles de las bombas. Evita encender/apagar
una bomba sin querer por el mismo toque que reactiva la pantalla.

Esto solo, sin tocar el hardware, es el ~80% del ahorro posible (ver Escenario B en
`POWER_BUDGET.md`). Vale la pena hacerlo exista o no un cambio de pantalla.

## 1. Comparativa de opciones de hardware

| Opción | Consumo relativo | Táctil | Legibilidad al sol | Disponibilidad/Costo | UX (velocidad de respuesta) |
|---|---|---|---|---|---|
| **TFT IPS + resistivo (XPT2046)** | Medio-bajo | Sí (resistivo, requiere presión firme o stylus) | Mala (se lava con el sol) | Alta - muy común, mismo precio que el actual | Rápida (igual que ahora) |
| **TFT IPS + capacitivo (GT911, actual) + sleep por software** | Medio (con el fix de arriba) | Sí (capacitivo, toque suave) | Mala | Ya lo tienes | Rápida |
| **Transflectivo/sunlight-readable + resistivo o capacitivo** | Medio-bajo (sin backlight de día) | Sí | **Buena** - usa luz ambiente, backlight solo de noche | Media - menos común, algo más caro | Rápida |
| **Sharp Memory LCD (monocromo) + film táctil resistivo añadido** | **Muy bajo** (µA en reposo, mA activo) | Sí, pero el táctil es un accesorio aparte, no integrado de fábrica | **Excelente** - reflectivo puro, sin backlight | Baja - nicho (Mouser/Digikey/Adafruit, no AliExpress), más caro, y hay que integrar el overlay táctil tú mismo | Rápida (a diferencia del e-paper) |
| **E-Paper + touch overlay** | **Mínimo** (casi cero en reposo) | Poco común integrado; refresco de 1-20s hace el toque incómodo | Excelente | Baja-media | **Lenta** - no apto para "tocar y ver respuesta inmediata" |
| **OLED táctil** | Depende del contenido (negro=gratis, blanco=caro) | Sí (capacitivo) | **Muy mala** - se lava casi por completo al sol directo | Media | Rápida |

## Mi recomendación para este proyecto

Dado que es un controlador de riego (probablemente cerca de un jardín, expuesto a luz diurna) y
necesita respuesta táctil inmediata para encender/apagar bombas:

1. **Primero**: implementar el fix de software (sección 0) sobre el hardware actual - es gratis
   y resuelve la mayor parte del problema.
2. **Si aun así quieres cambiar hardware**: un panel **transflectivo/sunlight-readable de 3.5"**
   (capacitivo o resistivo) es el mejor equilibrio para este caso de uso específico - ataca el
   problema real (backlight encendido todo el tiempo, y encima probablemente al sol) en vez de
   solo reducir consumo en abstracto. El Sharp Memory LCD da el consumo más bajo de todos, pero
   perder el táctil integrado y el color, más lo nicho de conseguirlo, lo hace poco práctico
   frente al beneficio marginal una vez ya aplicado el fix de software.
3. **Evitar E-Paper para esta pantalla de control** (aunque sea la de menor consumo en reposo):
   el refresco lento choca directamente con la necesidad de feedback táctil inmediato al tocar
   "Bomba A/B". Sí tendría sentido para un panel secundario de solo-lectura (ej. mostrar
   estado/batería) sin interacción táctil.

¿Quieres que implemente el fix de software (sección 0) sobre el hardware actual, o prefieres que
cotice/dimensione un panel transflectivo concreto primero?
