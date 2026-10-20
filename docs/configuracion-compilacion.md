# Configuración de compilación

La configuración está separada en cuatro capas:

- `platformio.ini`: selecciona exactamente un target, todas las `FEATURE_*`, el backend I²C y las opciones del compilador.
- `src/Variants/Profiles/`: contiene constantes con nombre y el único agregado de tuning coherente (`FlowPidConfig`) de cada modelo. Los perfiles no heredan entre revisiones; para EOLO Standard, editar `Standard.h`.
- `src/Variants/Pinouts/`: contiene un mapa completo de GPIO por modelo. `Pinout.h` solo selecciona el mapa y `PinoutValidation.h` valida sus incompatibilidades.
- Los módulos dueños conservan sus constantes internas; logging vive en `Utility/Log.h`.

No agregue valores numéricos de tuning a `build_flags`. Cree o ajuste la constante correspondiente del perfil. Los GPIO de display pertenecen al mapa de pinout, no al perfil. Los macros quedan reservados para seleccionar código (`EOLO_TARGET_*`, `FEATURE_*`) o tipos de compilación. `VariantValidation.h` rechaza targets múltiples, sensores de flujo/control/motor omitidos y combinaciones que no pertenecen al modelo seleccionado.

En PlatformIO ejecute **Config Info** dentro del ambiente, o:

```bash
pio run -e eolo_standard -t config_info
```

La tarea muestra el target y features activos. Los valores ajustables están explícitos en el header del perfil, para que no haya una segunda fuente de verdad en PlatformIO.

## Cadencia AFM07 para calificación

`EOLO_AFM07_POLL_GAP_MS` es un parámetro de compilación (50–10000 ms), con
valor predeterminado de `250 ms` para dejar un margen adicional al bus. El
planificador cuenta ese descanso desde el fin de cada transacción; no existe
un setter en el portal. Los nombres históricos `EOLO_AFM07_POLL_INTERVAL_MS` y
`EOLO_AFM_INTERVAL_MS` se aceptan como alias de entrada.

Para construir un candidato de barrido sin editar el árbol:

```bash
EOLO_AFM07_POLL_GAP_MS=400 pio run -e eolo_dron_legacy_low_power
```

La lectura exclusiva del estado AFM07 se solicita por `POST
/api/diagnostics/afm07` (o `rs485 diagnostic` por consola) y debe devolver
`register0004=0`. Un valor `1–3` bloquea la calificación y mantiene el
actuador apagado. Las lecturas inválidas y excepciones de transporte se
reintentan; sólo tres fallos consecutivos activan el bloqueo, y una lectura
válida posterior limpia ese bloqueo transitorio. Después de comprobarlo,
`POST /api/diagnostics/reset` reinicia las estadísticas de la ventana.

El barrido completo, con preflight físico confirmado, se automatiza con:

```bash
python3 scripts/afm07_rate_sweep.py --upload --preflight-ok \
  --base-url http://192.168.4.1
```

El script solo hace HTTP a la URL indicada y no cambia la red del computador;
guarda snapshots JSONL, una calificación por intervalo y `summary.csv`. Se
detiene ante el primer fallo salvo `--continue-on-failure`, y nunca reinicia
una captura automáticamente.

La selección de ejecución sigue esta ruta:

```text
platformio.ini -> ActiveProfile + Pinout -> ActiveApplication -> Context/Components
```

`eolo_dron_legacy_low_power` es un overlay del patrón LED Dron y no define otro modelo ni otro pinout. El backend I²C (drivers directos o librerías RTClib/Adafruit BME280) se elige para todos los sensores a la vez con `EOLO_I2C_DIRECT_DRIVERS=1|0`.

## Dron base (V2) y Dron Legacy

`eolo_dron` (`EOLO_TARGET_DRON`) es el Dron base de hardware V2 y pertenece a la
familia UI: OLED SH1106 por I2C (`EOLO_DISPLAY_SPI 0`), encoder en un bus propio
(`ENCODER_SDA_PIN`/`ENCODER_SCL_PIN`, `Wire1` a `kEncoderBusClockHz`), una bomba,
AFM07 y batería 2S (`BatteryModel::Pack2sLiIon`). No habilita Plantower, por lo
que su entorno declara las capacidades en vez de heredar `[ui]`. Usa
`FEATURE_FLOW_CALIBRATION`; el control PID, la captura por switches, el NeoPixel
y el NTC son de `eolo_dron_legacy` (`EOLO_TARGET_DRON_LEGACY`, headless).

Consideraciones del OLED por I2C: cada frame (≈1 KB, ≈90 ms a 100 kHz) toma el
`I2CBus` como cualquier sensor y el timeout de espera del worker sube a 120 ms
para esta configuración; si hay lecturas I2C degradadas en hardware, evaluar
subir `kI2cClockHz` sólo tras validar el bus a esa velocidad. La referencia
validada en hardware es `demos/DronBase`.
