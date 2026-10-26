# Desarrollo del firmware

Esta guía explica cómo hacer cambios habituales en el código: dónde va cada parámetro, cómo agregar un sensor, una escena, una demo o una prueba.
Es para quien ya compiló el proyecto y va a tocarlo.
La estructura del código está en [arquitectura.md](arquitectura.md); cómo compilar y flashear, en [compilacion.md](compilacion.md).

## Requisitos

Necesitas PlatformIO Core (`python3 -m pip install platformio`), Python 3 y un cable USB de datos. Para flashear necesitas además una placa ESP32 de la variante correspondiente. `./eolo help` lista los comandos de compilación, respaldo y versión.

## Convenciones de código

El firmware y las librerías locales son header-only. No agregues archivos `.c` ni `.cpp` para extraer lógica: define las funciones `inline` en el header que corresponda. `src/main.cpp` es solo la entrada Arduino.

La lógica que no necesita Arduino, FreeRTOS ni pines va en `lib/EoloCore/include/Eolo/`. Así se prueba en el host con `pio test -e native`. El código que toca hardware va en `src/`.

Sigue el estilo de llaves e `#include` del archivo que editas; no hay formateador configurado. Usa `PascalCase` para tipos, `camelCase` para métodos y campos, y `UPPER_SNAKE_CASE` para flags de compilación (`EOLO_TARGET_DRON_LEGACY`). `src/Ui/` y `src/Headless/` no se incluyen entre sí; `scripts/check_family_boundaries.py` lo comprueba.

## Archivos generados

No edites a mano estos archivos:

- `src/Headless/HeadlessSetupWebPage.h` lo genera `scripts/generate_headless_setup_web.py` antes de compilar los entornos headless. La fuente está en `web-server/`.
- `platformio.demos.ini` lo genera `scripts/generate_demo_envs.py` a partir de `demos/`.

## Secretos locales

Las credenciales de la red Wi-Fi del laboratorio no están en el repositorio. Copia `src/Headless/HeadlessSecrets.example.h` a `src/Headless/HeadlessSecrets.h` (ignorado por git) y completa los valores. También puedes pasarlas con `-D HEADLESS_SETUP_STA_SSID=... -D HEADLESS_SETUP_STA_PASSWORD=...`. Sin ese archivo, el Mini Rev.1 no se conecta a una red local, salvo que la configures desde la página de setup.

## Dónde cambiar un parámetro

| Qué cambias | Dónde | Ejemplo |
| --- | --- | --- |
| Capacidad del firmware | `platformio.ini` y `src/Variants/VariantValidation.h` | `FEATURE_FLOW_PID` |
| Ajuste de una variante | `src/Variants/Profiles/<Modelo>.h`, expuesto por `ActiveProfile.h` | `EoloConfig::Profile::kFlowPid`, umbrales del NTC |
| GPIO y cableado | `src/Variants/Pinouts/<Modelo>.h`, seleccionado por `Pinout.h` | pines de motor, buses y display |
| Constante interna de un módulo | El header del módulo | cadencia propia de un servicio |
| Dato persistido | El servicio que lee y escribe NVS | `SessionStore.h`: namespace `eolo_session`, clave `startDate` |

Los valores numéricos de ajuste no van en `build_flags`; crea la constante en el perfil. Los macros se reservan para elegir código (`EOLO_TARGET_*`, `FEATURE_*`).

`src/Variants/Legacy.h` conserva alias para código antiguo. Para retirar uno, cambia sus usos a `EoloConfig` o al módulo que lo define, confirma con `rg` que no quedan referencias y compila todas las variantes.

## Agregar un sensor

Sigue el patrón de AFM07, NTC o Plantower.

1. Si hay conversión o parseo, crea el modelo en `lib/EoloCore/include/Eolo/Core/Sensors/` y su DTO en `lib/EoloCore/include/Eolo/Types/`. No incluyas Arduino ahí; `scripts/audit_eolo_core_deps.sh` lo comprueba.
2. Crea el adaptador en `src/Common/Sensors/`. Debe tener `begin()` idempotente y `getData(DTO&)` que informe validez y frescura, como `NTC::getData(NTCData&)`.
3. Si el sensor es opcional, protégelo con un `FEATURE_*` nuevo, declarado en el entorno de `platformio.ini` y validado en `VariantValidation.h`.
4. Agrega el miembro en `Components` (`src/Common/Data/Components.h`) e inicialízalo en `Components::begin()`.
5. Si va al CSV, agrega la columna en `src/Common/Data/Logging/LogSchema.h` y el campo en `LogRecord`. Cambiar el formato afecta a quien lee los CSV; documéntalo en [docs/datos/](../datos/README.md).
6. Agrega pruebas del modelo en `test/test_eolo_core_native/test_main.cpp`.

Los pines van en el pinout de cada modelo que lleve el sensor, nunca en el adaptador.

## Agregar una escena

Aplica solo a las variantes con pantalla (`Ui/`).

1. Copia `src/Ui/Scenes/Template.h` a la carpeta que corresponda (`Captura/`, `Setup/` o la raíz de `Scenes/`) y renómbrala. La escena implementa `IScene`; agrégale `static constexpr const char *Name`, que usa el registro.
2. Dibuja en `update(Context&)` y libera en `exit(Context&)` cualquier hardware que hayas activado en la escena.
3. Regístrala en `src/Ui/Drawing/SceneRegistry.h`: incluye el header, agrega una constante en `SceneName`, crea la instancia estática y añádela con `makeScene(...)` a `scenes[]`.
4. Navega a ella con `SceneManager::setScene(SceneName::MiEscena, ctx)`.

Para ver las pantallas sin placa existe `scripts/ui_preview/`.

## Agregar una demo

Una demo es un sketch aislado para probar un componente en una placa. Crea `demos/NuevaDemo/NuevaDemo.ino`, deja el bloque `#define EOLO_MODEL_*` arriba como valor por defecto y usa los pines de `demos/EoloDemoPinout.h`. Si necesita librerías o modelos extra, regístrala en `scripts/demo_config.py`. Después regenera `platformio.demos.ini` y compila con los comandos de [demos/README.md](../../demos/README.md).

## Agregar una prueba

Las pruebas portables van en `test/test_eolo_core_native/test_main.cpp`: escribe una función `test_<comportamiento>` y regístrala con `RUN_TEST`. Las que necesitan una placa van en `test/<suite>/test_main.cpp`. Para el detalle de cada suite y cómo ejecutarla, ver [test/README.md](../../test/README.md).

Tras cambiar `Context` o la composición de una variante, compila también la demo `demo_dronprodtest_dron_legacy`. Una compilación no reemplaza una prueba física de arranque, captura, apagado seguro del motor y datos guardados; EOLO-MP, Express y Mini Rev.1 necesitan cada uno su placa.

## Verificación

Compilar no prueba el hardware, así que reporta por separado lo que compiló, lo que corrió en el host y lo que probaste en placa. Los comandos de prueba están en [test/README.md](../../test/README.md).

Las pruebas en hardware requieren la placa de la variante conectada. Indica su puerto con `--test-port`; no las ejecutes contra otra variante.

## Publicar una versión

Cada versión publicada queda fija: tiene su release en GitHub y aparece en la [página de flasheo](https://helix128.github.io/EoloXrd/), desde donde cualquiera la instala con Chrome o Edge sin PlatformIO.

1. En `CHANGELOG.md`, renombra `[Sin publicar]` a la versión nueva con su fecha, por ejemplo `## [1.0.0] - 2026-10-26`, y deja un `[Sin publicar]` vacío arriba.
2. Haz commit de ese cambio en `main`.
3. Crea y sube el tag:

```bash
git tag v1.0.0
git push origin v1.0.0
```

El workflow `Release` compila los seis envs y crea el release con un zip por variante. Cada zip trae `bootloader.bin`, `partitions.bin`, `boot_app0.bin` y `firmware.bin`, con las notas que tomó de esa sección del CHANGELOG. Al terminar, lanza el workflow `Pages` sobre `main`, que vuelve a armar la página con todas las versiones.

La página escribe las cuatro partes en sus offsets y no toca la NVS (`0x9000`), así que el equipo conserva su calibración, presets y Wi-Fi, salvo que se elija borrarlo. El sitio lo genera `scripts/build_flasher_site.py` a partir de `flasher/`. Para agregar una variante, súmala a `flasher/variants.json`; `test/test_build_flasher_site.py` falla si un env de `platformio.ini` no está ahí.

## Commits

Usa Conventional Commits con la descripción en español, por ejemplo `fix(modem): serializa el spool de telemetría entre tareas`. Los tipos habituales son `fix`, `feat`, `docs` y `chore`.
