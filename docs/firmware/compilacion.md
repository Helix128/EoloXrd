# Compilación y variantes

Este documento explica cómo `platformio.ini` y `src/Variants/` definen cada variante del firmware.
Sirve para compilar un modelo, ajustar sus parámetros o agregar uno nuevo.

La selección sigue esta ruta:

```text
platformio.ini -> ActiveProfile + Pinout -> ActiveApplication -> Context/Components
```

## Entornos

Cada entorno de firmware extiende `[embedded]` (ESP32, Arduino, C++17) y declara exactamente un `EOLO_TARGET_*`.

| Entorno | Target | Aplicación | Capacidades declaradas |
| --- | --- | --- | --- |
| `eolo_express` (por defecto) | `EOLO_TARGET_EXPRESS` | `UiApplication` | `[ui]`, `FEATURE_FLOW_AFM07` |
| `eolo_express_legacy` | `EOLO_TARGET_EXPRESS_LEGACY` | `UiApplication` | `[ui]`, `FEATURE_FLOW_FS3000` |
| `eolo_standard` | `EOLO_TARGET_STANDARD` | `UiApplication` | `[ui]`, `FEATURE_FLOW_AFM07`, `FEATURE_MODEM`, `FEATURE_ANEMOMETER`, `FEATURE_DUAL_BATTERY` |
| `eolo_dron` | `EOLO_TARGET_DRON` | `UiApplication` | `FEATURE_MOTOR_PWM`, `FEATURE_FLOW_CALIBRATION`, `FEATURE_FLOW_AFM07` |
| `eolo_dron_legacy` | `EOLO_TARGET_DRON_LEGACY` | `DronApplication` | `FEATURE_HEADLESS`, `FEATURE_MOTOR_PWM`, `FEATURE_FLOW_AFM07`, `FEATURE_FLOW_PID`, `FEATURE_NEOPIXEL`, `FEATURE_NTC` |
| `eolo_dron_legacy_low_power` | igual que `eolo_dron_legacy` | `DronApplication` | agrega `STATUS_LED_LOW_POWER` |
| `native` | no aplica | no aplica | pruebas de `EoloCore` en el host |

`[ui]` aporta `FEATURE_MOTOR_PWM`, `FEATURE_FLOW_CALIBRATION` y `FEATURE_PLANTOWER`. `eolo_dron` no lo hereda porque no usa Plantower. `eolo_dron_legacy_low_power` solo cambia el patrón del LED de estado; no define otro modelo ni otro pinout.

Compilar y subir:

```bash
pio run                                  # eolo_express, el entorno por defecto
pio run -e eolo_dron_legacy              # un entorno concreto
pio run -e eolo_standard -t upload       # compila y sube a la placa
pio run -e eolo_standard -t config_info  # muestra target y features activos
```

## Dónde vive cada parámetro

`platformio.ini` solo selecciona código: el target, las `FEATURE_*`, el backend I²C y las opciones del compilador. Los valores numéricos no van en `build_flags`.

- `src/Variants/Profiles/<Modelo>.h`: constantes con nombre de cada modelo y su `FlowPidConfig`. Los perfiles no heredan entre sí.
- `src/Variants/Pinouts/<Modelo>.h`: mapa completo de GPIO por modelo, incluidos los del display. `Pinout.h` lo selecciona y `PinoutValidation.h` valida conflictos.
- `src/Variants/ActiveProfile.h`: elige un único perfil según el target y expone sus constantes.
- `src/Variants/VariantValidation.h`: rechaza targets múltiples, sensores de flujo omitidos y combinaciones de capacidades que el modelo no admite.
- El módulo que usa una constante interna la define en su propio header. El logging, por ejemplo, está en `src/Common/Utility/Log.h`.

Los pines también están documentados en [docs/hardware/pinouts/](../hardware/pinouts/README.md), y `scripts/check_pinouts.py` comprueba que coincidan con `Pinout.h`.

## Backend I²C

`EOLO_I2C_DIRECT_DRIVERS` elige el backend de todos los sensores I²C a la vez: `1` usa los drivers directos (`DirectDS3231`, `DirectBME280`) y `0` usa RTClib y Adafruit BME280. No se mezclan porque Adafruit BusIO puede reinicializar `Wire` durante un reintento y perturbar a los drivers directos. Express y Express Rev.1 usan `0`; los demás entornos, `1`.

## Mini Rev.2 y Mini Rev.1

`eolo_dron` es EOLO Mini Rev.2. Pertenece a la familia UI: OLED SH1106 por I²C (`EOLO_DISPLAY_SPI 0`), encoder en un bus propio (`ENCODER_SDA_PIN`, `ENCODER_SCL_PIN`, `Wire1` a `kEncoderBusClockHz`), una bomba, AFM07 y batería 2S (`BatteryModel::Pack2sLiIon`). Compila. Para el hardware de este entorno, la referencia validada es `demos/DronBase`.

`eolo_dron_legacy` es headless. Le corresponden el control PID de flujo, la captura por switches, el NeoPixel y el NTC.

Cada frame del OLED (1 KB, unos 90 ms a 100 kHz) toma el `I2CBus` como cualquier sensor. Si aparecen lecturas I²C degradadas, valora subir `kI2cClockHz` en `Dron.h`, pero solo después de validar el bus a esa velocidad.

## Cadencia AFM07

`EOLO_AFM07_POLL_GAP_MS` es el descanso mínimo entre transacciones AFM07, contado desde el final de cada una. Acepta de 50 a 10000 ms y vale 250 ms por defecto. El portal de setup no puede cambiarlo. Los nombres `EOLO_AFM07_POLL_INTERVAL_MS`, `EOLO_AFM_INTERVAL_MS` y `EOLO_AFM_POLL_INTERVAL_MS` se aceptan como nombres alternativos.

Para compilar un candidato sin editar el árbol, define la variable de entorno:

```bash
EOLO_AFM07_POLL_GAP_MS=400 pio run -e eolo_dron_legacy_low_power
```

La lectura exclusiva del estado AFM07 se pide con `POST /api/diagnostics/afm07` o con `rs485 diagnostic` por consola, y debe devolver `register0004=0`. Un valor distinto de 0 bloquea la calificación. Las lecturas inválidas y las excepciones de transporte se reintentan; tres fallos consecutivos activan el bloqueo y una lectura válida posterior lo limpia. Después de comprobarlo, `POST /api/diagnostics/reset` reinicia las estadísticas de la ventana.

El barrido completo, con el preflight físico confirmado, se automatiza con:

```bash
python3 scripts/afm07_rate_sweep.py --upload --preflight-ok \
  --base-url http://192.168.4.1
```

El script solo hace HTTP a la URL indicada y no cambia la red del computador. Guarda snapshots JSONL, una calificación por intervalo y `summary.csv`. Se detiene ante el primer fallo salvo con `--continue-on-failure`, y nunca reinicia una captura por su cuenta.

## Agregar una variante

1. Crea el perfil en `src/Variants/Profiles/` y selecciónalo en `ActiveProfile.h`.
2. Crea el pinout en `src/Variants/Pinouts/`, selecciónalo en `Pinout.h` y agrega sus reglas de conflicto en `PinoutValidation.h`.
3. Agrega el entorno en `platformio.ini` con su `EOLO_TARGET_*` y sus capacidades, y valídalas en `VariantValidation.h`.
4. Agrega el caso en `src/Application/ActiveApplication.h` para elegir la aplicación.
5. Si la variante tiene demos, registra el modelo en `scripts/demo_config.py` y regenera `platformio.demos.ini` con `python3 scripts/generate_demo_envs.py`.
6. Documenta el mapa de pines en `docs/hardware/pinouts/` y ejecuta `python3 scripts/check_pinouts.py`.

Compila el entorno nuevo y los existentes antes de dar el cambio por bueno. No cambies claves NVS ni el formato CSV para introducir una variante.
