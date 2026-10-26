# AFM07: calificación y diagnóstico

El AFM07 es el sensor de flujo de aire de EOLO Mini Rev.2, Mini Rev.1, EOLO-MP y EOLO Express Rev.2. Mide el caudal en L/min y se lee por Modbus RTU sobre RS485.
Este documento explica cómo conectarlo, cómo diagnosticarlo y qué debe cumplir antes de usar un equipo en producción.

## Conexión

El AFM07 comparte el bus RS485 con el anemómetro (EOLO-MP). El firmware lo lee con este contrato fijo, definido en `lib/EoloCore/include/Eolo/Core/Communication/RS485Protocol.h`:

- ID de esclavo `0x02`.
- 4800 baud.
- Función `0x03` (leer registros), registro de flujo `0x0000`, un registro.
- Registro de diagnóstico `0x0004`. Debe valer `0`.

El anemómetro usa el ID `0x01`. Los pines del transceptor RS485 cambian por variante:

| Variante | RX | TX | DE/RE |
| --- | --- | --- | --- |
| EOLO Mini Rev.2 | `GPIO17` | `GPIO4` | `GPIO16` |
| EOLO Mini Rev.1 | `GPIO16` | `GPIO17` | `GPIO4` |
| EOLO-MP | `GPIO35` | `GPIO33` | `GPIO26` |
| EOLO Express Rev.2 | `GPIO27` | `GPIO25` | `GPIO26` |

Los valores salen de `src/Variants/Pinouts/`. El detalle de cada placa está en [pinouts](pinouts/README.md).

## Cadencia de lectura

El descanso mínimo entre transacciones AFM07 se fija al compilar con `EOLO_AFM07_POLL_GAP_MS`: de 50 a 10000 ms, 250 ms por defecto. El portal de setup solo muestra el valor (`pollGapMs`); no puede cambiarlo. Los detalles de compilación están en [Compilación](../firmware/compilacion.md).

## Diagnosticar

Hay tres herramientas. Ninguna cambia la cadencia ni reanuda una captura.

### Consola serie

La consola serie acepta estos comandos (`pio device monitor -b 115200`):

```text
rs485 status        estadísticas por esclavo (IDs 0x01 y 0x02)
rs485 diagnostic    lee el registro 0x0004 del AFM07 con pausa exclusiva
r                   reinicia estadísticas del profiler y de RS485
drone status -v     estado del Mini Rev.1 con detalles
```

`rs485 diagnostic` solo encola la lectura; consulta el resultado con `rs485 status`. Los comandos `drone ...` existen solo en Mini Rev.1 y están en `src/Headless/DroneDebugCommands.h`.

### API del setup Wi-Fi

Conéctate a la red del Mini Rev.1 (`http://192.168.4.1/`, ver el [manual](../manuales/mini-rev1.md)) y usa estos endpoints:

```text
GET  /api/diagnostics          estadísticas RS485, estado de sensores y de captura
POST /api/diagnostics/afm07    pide la lectura exclusiva de 0x0004
GET  /api/diagnostics/afm07    resultado de esa lectura
POST /api/diagnostics/reset    reinicia las estadísticas
GET  /api/status               estado general y captura
```

El `POST` a `/api/diagnostics/afm07` responde `409` si hay una captura activa. El resultado trae `register0004`, `valueValid` y `rateTuningBlocked`. El bloqueo por `register0004` distinto de cero se mantiene hasta una lectura explícita con `0x0004 == 0`.

### Scripts

`scripts/platformio_afm07_gap.py` es un script `pre:` de PlatformIO. Lee la variable de entorno `EOLO_AFM07_POLL_GAP_MS` y la pasa como define a esa compilación, sin editar `platformio.ini`.

```bash
# compila con un descanso de 400 ms
EOLO_AFM07_POLL_GAP_MS=400 pio run -e eolo_dron_legacy_low_power
```

`scripts/afm07_rate_sweep.py` compila (y con `--upload` carga) un firmware por cada intervalo, mide durante una ventana por la API LAN y escribe el resultado.

```bash
# barrido completo; --preflight-ok confirma que hiciste el preflight físico
python3 scripts/afm07_rate_sweep.py --upload --preflight-ok \
  --base-url http://192.168.4.1 \
  --intervals 1000 800 600 400 200

# solo compilar cada intervalo, sin contactar el equipo
python3 scripts/afm07_rate_sweep.py --compile-only --intervals 1000 800
```

Opciones útiles: `--env` (por defecto `eolo_dron_legacy_low_power`), `--stabilization-seconds` (30), `--measurement-seconds` (180), `--sample-seconds` (1), `--output-dir` y `--continue-on-failure`. El barrido se detiene en el primer intervalo fallido salvo que uses esa última opción. No reinicia capturas ni cambia la red del computador.

Cada intervalo deja `samples.jsonl` y `qualification.json` en `artifacts/afm07-rate-sweep/<gap>/`, y el script agrega una fila a `artifacts/afm07-rate-sweep/summary.csv`.

## Preflight físico

Haz estas comprobaciones con el motor apagado y sin captura:

- Alimentación estable entre 9 y 24 V DC durante todo el ensayo.
- Masa común entre ESP32, transceptor RS485 y AFM07.
- Polaridad A/B correcta en ambos extremos.
- Terminación correcta, sin derivaciones ni cables flojos.
- AFM07 configurado con ID `0x02`, 4800 baud y respuesta Modbus RTU.
- `POST /api/diagnostics/afm07` termina en estado `complete`, con `valueValid=true`, `register0004=0` y `rateTuningBlocked=false`.
- `/api/status` y `/api/diagnostics` muestran la captura inactiva y PWM `0`.

Si falla alguna, detén el barrido: subir la cadencia no lo arregla. Si `0x0004` vale 1, 2 o 3, o aparece una excepción `0x04` incluso a 1000 ms, revisa sensor, alimentación, configuración y cableado antes de medir de nuevo. No amplíes los timeouts para ocultar el fallo.

## Criterios de aprobación

Se prueban los intervalos 1000, 800, 600, 400 y 200 ms. Para cada uno, el firmware se compila con ese descanso y se mide así: reinicio, diagnóstico de `0x0004` igual a cero, 30 s de estabilización, reinicio de estadísticas y 180 s de ventana con una muestra por segundo.

Un intervalo aprueba solo si todas las muestras cumplen a la vez:

- Cero excepciones Modbus, timeouts, errores CRC, tramas incompletas o malformadas, bus ocupado, tramas inesperadas, bytes tardíos y fallos de plazo, en el agregado y en cada esclavo.
- AFM07 siempre `online` y con muestra fresca.
- Diagnóstico completo, válido y `register0004 == 0`.
- SD, NTC, BME280 y RTC válidos.
- `maxSuccessGapMs <= 1500`.
- Descanso real mínimo entre transacciones AFM07 de al menos el configurado, con 5 ms de tolerancia (`minRestMs + 5 >= gap`).
- Captura inactiva y PWM en cero durante todo el barrido.

Un solo incumplimiento invalida el intervalo. Anota el motivo, la hora, el firmware y el estado físico, y no edites el JSON de resultados.

Para producción usa un escalón más lento que el intervalo aprobado más rápido, y repite la ventana completa con ese valor.

| Más rápido aprobado | Intervalo de producción |
| ---: | ---: |
| 200 ms | 400 ms |
| 400 ms | 600 ms |
| 600 ms | 800 ms |
| 800 ms | 1000 ms |
| solo 1000 ms | sin margen: no apto |

Ninguna prueba finita demuestra que el hardware no fallará. Aprobar significa cero fallos durante las ventanas observadas y apagado seguro del motor ante cualquier fallo posterior.

## Problemas conocidos

Observado el 2026-08-31 con un Mini Rev.1 en el barrido a 200 ms: 949 transacciones, 103 fallos, de ellos 66 excepciones Modbus `0x04` y 17 timeouts, sin errores CRC. El AFM07 siguió en línea y `maxSuccessGapMs` fue 1437. El registro completo está en [la validación de entrega](../archivo/validacion-entrega-dron-2026-08-31.md).

No hay una causa confirmada. Con ese resultado el equipo no se declaró apto para producción. Una excepción `0x04` obliga a detener el ajuste de cadencia y a revisar el hardware, como indica el preflight.

## Checklist de calificación

Marca cada casilla al completarla, en el orden en que aparecen. Mientras queden casillas abiertas, no declares apto el Mini Rev.1 para producción. Guarda el SHA de Git, el entorno PlatformIO y el binario cargado junto con cada resultado; un resultado de otra compilación u otra unidad no cuenta como aprobación.

Verificación de software:

- [ ] `python3 scripts/check_pinouts.py` pasa; anota cualquier discrepancia histórica.
- [ ] `pio test -e native` pasa.
- [ ] `pio run -e eolo_dron_legacy_low_power` compila, y las suites embebidas Mini compilan con `--without-testing`.
- [ ] Compilan también `eolo_express`, `eolo_express_legacy` y `eolo_standard`.
- [ ] `node --check web-server/app.js` pasa, y `python3 -m py_compile scripts/afm07_rate_sweep.py scripts/platformio_afm07_gap.py` también.
- [ ] `src/Headless/HeadlessSetupWebPage.h` está regenerado con `python3 scripts/generate_headless_setup_web.py` y coincide con el portal fuente.
- [ ] El portal solo muestra `pollGapMs`, sin setter HTTP.
- [ ] Un error del AFM07 deja el enclavamiento activo y PWM en cero, y una muestra posterior no rearma el actuador sin un diagnóstico explícito `0x0004 == 0`.
- [ ] Si una confirmación web queda bloqueada por seguridad, el portal sigue disponible para consultar el diagnóstico.

Calificación en hardware:

- [ ] Preflight físico completo.
- [ ] Barrido de cadencia con los cinco intervalos, y ventana completa repetida con el intervalo de producción.
- [ ] Captura de aceptación: una sola captura de 15 minutos con un caudal externo controlado de 5 L/min, sin rearranque automático. Debe tener cero errores RS485, `maxSuccessGapMs <= 1500`, AFM07 siempre fresco, ningún reset, watchdog ni pérdida de SD o alimentación, volumen coherente con el medidor externo, fila final `Finalizado` en el CSV, índice maestro actualizado, descarga y preview del archivo, y PWM final exactamente `0`.
- [ ] Conservar JSONL, JSON de calificación, CSV resumen, logs serie, hashes de firmware y mediciones de alimentación y cableado, con unidad, MAC, fecha, operador, medidor y entorno de compilación.

Aptitud de terreno (no se resuelve aprobando la cadencia):

- [ ] Batería: protección por bajo voltaje, autonomía y retención tras un corte de 5 minutos.
- [ ] Circuito neumático y medidor externo, vibración, montaje en vuelo, EMI, ambiente e intemperie.
- [ ] Las cuatro entradas de switches y las siete decodificaciones físicas, incluido el arranque con espera `Off` que vuelve al setup Wi-Fi.
- [ ] Flujo completo del portal: captive portal, preset, confirmación, listado, preview, descarga, borrado e índice maestro.
- [ ] Autenticación de los endpoints de depuración y cambio de las credenciales conocidas del AP.
- [ ] Deep sleep, patrón del LED Low Power y apagado tras una captura válida.
