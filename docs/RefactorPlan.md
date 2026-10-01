# Plan de reestructuración del firmware

Objetivo: separar el código por familia de producto (UI y headless), reducir los
`#if`/`#ifdef` de variante en código compartido y extraer lógica pura a
`EoloCore` con tests `native`, sin cambiar el comportamiento del firmware.

## Estado de ejecución

Fases 0 a 4 ejecutadas en la rama `refactor/estructura` (un commit por paso).
Las fases 5 y 6 siguen diferidas (requieren placa Dron).

| Fase | Resultado |
|---|---|
| 0 | Línea base: 6 entornos, 39 tests nativos, 22 tests Python, 12 suites hardware compiladas en `eolo_dron`. |
| 1 | Borradas las ramas muertas y los tres `#define`. Tamaños de `firmware.bin` idénticos en los 6 entornos. |
| 2 | `I2C_TRANSACTION_TIMEOUT_MS` pasó a los perfiles y los alias de `Legacy.h` se reemplazaron por `EoloConfig::`. Quedan `DisplayModel`, `CMD_RESET_*`, `DRONE_DURATION_INFINITE` y `FONT_*` (no son configuración de variante). Tamaños idénticos. |
| 2b | `eolo_standard_libraries` retirado; `EOLO_I2C_DIRECT_DRIVERS` se mantiene como único toggle. |
| 3 | Extraídos a `EoloCore`: `HttpUrl`, `AtResponse`, `SignalQuality`, `StatusLedPalette`, `ConsoleArgs`; `LogIndexService` reutiliza `RtcTimeParser::toUnix`. 51 tests nativos. Tamaños entre -144 y -624 bytes. |
| 4 | `src/` reorganizado por familia, `scripts/check_family_boundaries.py` y base `[ui]` en `platformio.ini` (flags comunes de Express, Express Legacy y Standard). Los tamaños subieron 176-288 bytes respecto a la Fase 3: RAM y grafo de dependencias de librerías idénticos; la diferencia está en `.flash.rodata`, donde `__FILE__` conserva rutas de include más largas (por ejemplo `src/Application/../Ui/../Common/Board/I2CBus.h`). |

Trabajo adicional sin placa (posterior a la Fase 4): `Legacy.h` quedó solo con
includes (`FONT_*` a `Ui/Drawing/Fonts.h`, `DisplayModel` reemplazado por
`EOLO_DISPLAY_MODEL`, `InputCommand`, `EoloConfig::kDurationInfinite`);
`RS485SlaveStats` y los códigos de error pasaron a `RS485Stats.h` con tests
nativos (53 en total); `scripts/check_all.sh` ejecuta todos los chequeos
(50 pasos con `--with-hw-suites --with-demos`, todos OK).

Desviaciones respecto al plan original:

- **Fase 3, candidatos descartados tras leer los cuerpos:** el planificador de
  `RS485Bus` ya estaba en el core (`RS485TimingModel`) y lo que queda es
  pegamento de mutex y estadísticas; el parseo de hora de `RTCManager` ya
  delega en `RtcTimeParser`. El resto de `LogIndexService` (CSV, HTML,
  `fieldAt`) queda con `String` de Arduino: extraerlo exige reescribir la
  lógica sobre cadenas C sin ganancia clara.
- **Fase 3, comportamiento heredado preservado:** `AtResponse::hasValidIp`
  reproduce el original, que acepta una IPv4 válida contenida dentro de un
  número mayor (por ejemplo `300.1.1.1` se lee como `00.1.1.1` y `1.2.3.4.5`
  como `2.3.4.5`). No se corrigió para mantener el refactor sin cambios de
  comportamiento.
- **Fase 4, nombres en PascalCase** (`Ui/`, `Headless/`, `Modem/`, `Variants/`,
  `Common/`) para seguir la convención de `src/`. `Variants/` agrupa también
  `Legacy.h`, `Types.h`, `ActiveProfile.h`, `Pinout.h` y sus validaciones.
- **Fase 4, `[headless]`:** no se creó una base aparte porque Dron es la única
  familia headless y `eolo_dron_low_power` ya extiende `eolo_dron`.
- **Fase 4, límites:** `Ui` y `Headless` no se incluyen entre sí y `Headless` no
  incluye `Modem`. `Common` todavía depende de `Ui`/`Headless`/`Modem` en 14
  includes (`Context.h`, `Components.h`, `DebugConsole.h`, `RTCNetworkSync.h`,
  `UploadService.h`); el script los congela en una lista que solo puede
  disminuir. Eliminarlos es el objetivo de la Fase 5.
- **Demos:** `demo_spistandarddisplaydiag_standard` ya fallaba antes del
  refactor (`RTClib.h` no encontrado en su `lib_deps`); se corrigió en
  `scripts/demo_config.py` y las 27 demos compilan.

## Diagnóstico

- Todo el código de `src/` es header-only; el único `.cpp` es `src/main.cpp`.
  Se mantiene así (decisión del proyecto).
- `Context.h` (48 directivas) y `Components.h` (18) concentran la variabilidad
  con `#if`. En todo `src` hay unas 229 directivas `EOLO_TARGET_*`/`FEATURE_*`.
- `CaptureController`, `MotorCaptureControl` y `HeadlessMotorCalibration`
  reciben `Context&`; `ContextCaptureController.h` y
  `ContextHeadlessMotorCalibration.h` existen para romper el ciclo de includes
  y contienen ramas específicas de Dron.
- `src/` está organizado por capa técnica (`Board/`, `Data/`, `Utility/`), no por
  familia; código exclusivo de Dron o Standard está mezclado con el común.
- `Variants/Legacy.h` fija `BAREBONES`, `CHECK_SENSORS` y `SERIAL_INPUT` en 0:
  hay ramas muertas (segunda clase `RTCManager`, segunda clase `Input`, bloques
  `CHECK_SENSORS` y de valores falsos de `BAREBONES`).
- `Legacy.h` lo incluyen 32 archivos y contiene un `#if` de variante
  (`I2C_TRANSACTION_TIMEOUT_MS`).

## Aclaraciones

- `build_src_filter` no aporta: filtra unidades de compilación y hay una sola.
  Mover archivos a carpetas por familia es organización, no ahorro de
  compilación. El límite entre familias se hace efectivo con una verificación
  (ver Fase 4).
- `if constexpr` no descarta la rama no tomada en funciones normales; no sirve
  para quitar miembros inexistentes. Cada familia tiene su propia estructura
  sobre un núcleo común.
- `HeadlessSetupWebPage.h` es un arreglo de bytes gzip generado por
  `scripts/generate_headless_setup_web.py` desde `web-server/`; no es HTML
  embebido y no hay problema de legibilidad.
- `Motor.h` ya delega en `PwmMath` y `CalibrationManager` ya usa
  `CalibrationModel`: no hay duplicación que extraer.
- `ExpressLegacy` es una variante viva (demos, `scripts/demo_config.py`,
  `test/test_firmware_backup.py`, `.vscode/tasks.json`, docs); no es candidato
  a limpieza.

## Fases

Cada fase tiene una condición para avanzar. No se pasa a la siguiente sin
cumplirla.

### Fase 0 — Línea base

- Compilar todos los entornos con el comando de `docs/architecture.md:94`
  (revisar si `.vscode/tasks.json` ya tiene una tarea equivalente).
- Ejecutar `pio test -e native`, los tests Python
  (`python3 -m unittest discover -s test -p 'test_*.py'`) y la compilación sin
  ejecutar de las suites de hardware de los entornos afectados.
- Registrar el tamaño de `firmware.bin` de cada entorno.
- Listar todas las rutas que referencian archivos: `extra_scripts`,
  `HEADER_PATH` del script generador, `test/*/test_main.cpp`, `demos/`,
  `scripts/demo.py`, `scripts/demo_config.py`, tests Python,
  `.vscode/tasks.json`, `docs/`, `AGENTS.md`.
- **Avanzar cuando:** todo compila y pasa antes de tocar nada.

### Fase 1 — Eliminar código muerto (sin cambio de comportamiento)

Sujeta a la decisión 1.

- Borrar la segunda `RTCManager` (~líneas 463-655), la segunda `Input` (el
  `#else` de `SERIAL_INPUT == false`) y los bloques `CHECK_SENSORS` y
  `BAREBONES` (`RTCManager`, `FS3000`, `Motor`, `BME280`, `WaitScene`,
  `CapturaScene`, `Context.h`, `ContextCaptureController.h`).
- Conservar siempre la rama que hoy se compila: `BAREBONES == false`,
  `!BAREBONES`, `SERIAL_INPUT == false`.
- Quitar los tres `#define` de `Legacy.h`.
- **Avanzar cuando:** todos los entornos compilan y cada `firmware.bin` queda
  del mismo tamaño.

### Fase 2 — Migrar `Legacy.h` a `EoloConfig::`

- Empezar por `I2C_TRANSACTION_TIMEOUT_MS`, que pasa a `Profiles/*.h` como
  `kI2cTransactionTimeoutMs`.
- Un macro por commit; trabajo mecánico.
- **Avanzar cuando:** tamaños idénticos.

### Fase 2b — Retirar `eolo_standard_libraries`

- Se conserva un único toggle global, `EOLO_I2C_DIRECT_DRIVERS` (1 = drivers
  directos `DirectDS3231`/`DirectBME280`, 0 = librerías RTClib/Adafruit BME280),
  tal como está hoy. No se agregan toggles por sensor.
- Se elimina `[env:eolo_standard_libraries]` y su `build_unflags` de
  `platformio.ini`. Las librerías siguen disponibles: `eolo_express` y
  `eolo_express_legacy` las usan con `EOLO_I2C_DIRECT_DRIVERS=0`, y cualquier
  modelo puede cambiar de backend con ese flag.
- Actualizar las menciones del entorno en `README.md`, `docs/architecture.md`,
  `docs/guia-para-continuar.md`, `docs/configuracion-compilacion.md` y
  `docs/pendientes-afm07-produccion.md`.
- **Avanzar cuando:** los 5 entornos restantes compilan con tamaños idénticos a
  la línea base y las demos compilan.

### Fase 3 — Extraer lógica pura a `EoloCore`

Los siguientes son **candidatos**, identificados por nombre de método. Cada uno
se confirma leyendo su cuerpo al empezar su extracción, y se descarta si toca
estado compartido o hardware.

| Origen | Candidato | Destino |
|---|---|---|
| `Modem.h` | `normalizeHttpUrl`, `extractHttpHost`, `buildHttpUrlWithHostIp`, `isValidIPv4`, `parseOctet`, `hasRegisteredStat`, `hasValidIp`, `httpActionStatusText`, `appendLimited` | `Core/Communication/HttpUrl.h`, `AtResponse.h` |
| `ModemService.h` | `signalBarsFromCsq`, `updateSignalFromResponse` | `Core/Communication/SignalQuality.h` |
| `RS485Bus.h` | `selectDueEndpoint`, `scheduleNext`, `noteDeadlineMiss`, `recordResult`, `RS485SlaveStats` | `Core/Communication/RS485Scheduler.h` |
| `StatusLed.h` | `temperatureColor`, `temperaturePeak`, `profileFor`, `normalProfileFor`, `lowPowerProfileFor` | `Core/Ui/StatusLedPatterns.h` |
| `DroneDebugCommands.h` | `readIntArg`, `readFloatArg`, `parsePidConfig`, `formatDuration` | `Core/Debug/ArgParser.h` |
| `LogIndexService.h` | armado de `Entry` y de CSV/HTML | `Core/Logging/LogIndexFormat.h` |
| `RTCManager.h` | parseo de fecha/hora y respuesta del servidor de hora | completar `RtcTimeParser.h` |

- Un commit por extracción, con test en `test/test_eolo_core_native/test_main.cpp`
  y el sitio de uso delegando a la función nueva.
- **Avanzar cuando:** todos los entornos compilan y el cambio de tamaño es
  pequeño y explicado.

### Fase 4 — Mover carpetas

- Estructura: `src/ui/`, `src/headless/`, `src/common/`, `src/modem/`, más
  `src/variants/` (perfiles y pinouts).
  - `headless/`: `HeadlessSetupServer`, `HeadlessSetupTypes`,
    `HeadlessSetupWebPage`, `CaptureSwitches`, `DroneDebugCommands`,
    `DronApplication`, `HeadlessMotorCalibration`.
  - `ui/`: `Drawing/`, `Scenes/`, `UiApplication`, `Input`.
  - `modem/`: `Modem*`, `SensorAPI`, `ModemDebugCommands`.
  - `common/`: sensores, efectores, el resto de `Board/`, `Data/Logging`,
    `SessionStore`, utilidades comunes.
- Verificación de límites entre familias: un `#error` en las cabeceras de
  `ui/` y `headless/`, o un script que falle si una incluye a la otra.
- `platformio.ini`: bases `base_ui` y `base_headless`; retirar `eolo_standard_libraries` junto con su
  `build_unflags` (Fase 2b).
- Actualizar todas las rutas listadas en la Fase 0, incluido `HEADER_PATH` del
  script generador.
- **Avanzar cuando:** tamaños idénticos (solo pueden variar por cadenas
  `__FILE__`) y la verificación de límites pasa.

### Fase 5 — Separar `Context` y `Components` por familia

- Cada familia con su propia estructura compuesta sobre un núcleo común:
  `CoreContext` + `UiContext` + `HeadlessContext`.
- `CaptureController`, `MotorCaptureControl` y `HeadlessMotorCalibration`
  reciben un contexto núcleo más un gancho pequeño para lo específico de la
  familia (por ejemplo `prepareCaptureStart` de Dron en `beginCapture`).
- Los métodos de `ContextCaptureController.h` y
  `ContextHeadlessMotorCalibration.h` pasan al contexto de su familia.
- Quedarán algunos `#if` dentro de la familia UI (modem, anemómetro, batería
  dual entre Express y Standard). Es aceptable: el objetivo es eliminar
  `FEATURE_HEADLESS` y `EOLO_TARGET_DRON` del código compartido.
- **Avanzar cuando:** revalidación en placa Dron
  (`docs/validacion-entrega-eolo-dron-2026-08-31.md`). Compilar no basta.

### Fase 6 — Partir clases (alcance reducido)

- Solo `HeadlessSetupServer` → `PresetStore` (slots en `Preferences`) y
  `LogBrowser` (listar, previsualizar, descargar y borrar logs, con validación
  de ruta).
- Se difiere el resto (`i2cWorker` de `Components`, `ModemService`, `I2CBus`,
  `RS485Bus` transporte/diagnóstico AFM), que son tareas FreeRTOS con timing
  sensible, salvo que aparezca un motivo concreto.

## Decisiones tomadas

1. **Toggles muertos** (`BAREBONES`, `CHECK_SENSORS`, `SERIAL_INPUT`): se
   borran (ya no se usan para pruebas de banco). La Fase 1 se ejecuta completa.
2. **`eolo_standard_libraries`**: se elimina el entorno; se conservan las
   librerías (RTClib y Adafruit BME280) detrás del toggle global
   `EOLO_I2C_DIRECT_DRIVERS`, que no cambia. Un solo toggle para todos los
   sensores evita mezclar backends en el mismo bus (Adafruit BusIO puede
   reinicializar `Wire` durante reintentos). Ver Fase 2b.
3. **`HeadlessSetupWebPage.h`**: sigue versionado.
4. **Demos en la Fase 0**: `platformio.demos.ini` (27 entornos `demo_*`,
   generado por `scripts/generate_demo_envs.py`) queda fuera de la línea base;
   la base son los 6 entornos de firmware. Requisito: las demos no deben
   romperse. Al terminar la Fase 4 (y la 2b) se compilan las 27 demos una vez
   como verificación (`./scripts/demo.py`), y se revisan sus rutas y las de
   `scripts/`.
5. **Sin placa Dron disponible**: la Fase 5 (separar `Context`/`Components`)
   queda diferida hasta tener placa. La Fase 6 también toca código que solo usa
   Dron (`HeadlessSetupServer`), por lo que se difiere o se ejecuta con
   compilación y tests nativos como única verificación, con ese riesgo
   registrado.

## Alcance y límites

Este plan se elaboró a partir de `platformio.ini`, `main.cpp`,
`ActiveApplication`, `ActiveProfile`, `VariantValidation`, `Legacy.h`, listados
de métodos y conteos de directivas, no de una lectura completa del código. Los
candidatos de la Fase 3 y las separaciones de la Fase 6 requieren leer los
cuerpos antes de ejecutarse.
