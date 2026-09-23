# Guía para continuar EOLO

El firmware y las librerías locales mantienen implementaciones header-only.
`src/main.cpp` es únicamente la entrada Arduino. No agregue nuevas unidades
`.c` o `.cpp` para extraer lógica: ubique cada definición `inline` en el header
del dueño y mantenga compilable cada corte.

## Dónde cambiar un parámetro

| Tipo de valor | Dueño | Ejemplo |
| --- | --- | --- |
| Capacidad del firmware | `platformio.ini` y `Config/VariantValidation.h` | `FEATURE_FLOW_PID` |
| Ajuste de una variante | `src/Config/Profiles/<Modelo>.h`, expuesto por `ActiveProfile.h` | `Profile::kFlowPid`, umbrales NTC |
| GPIO y cableado | `src/Board/Pinouts/<Modelo>.h`, seleccionado por `Pinout.h` | pines de motor, buses y display |
| Constante interna de un módulo | Header del módulo | cadencia propia del servicio |
| Dato de usuario persistido | Servicio que lee y escribe NVS | `SessionStore.h`: namespace `eolo_session`, clave `startDate` |

`Config/Legacy.h` conserva los alias todavía consumidos. Para retirar uno,
cambie primero sus consumidores a `EoloConfig` o al dueño real, confirme con
`rg` que no quedan usos y compile las variantes. Los macros de selección de
código y los símbolos de librerías de display siguen siendo macros.

## Cómo añadir una variante

1. Añada el perfil completo en `src/Config/Profiles/` y su selección exclusiva
   en `ActiveProfile.h`.
2. Añada un pinout completo en `src/Board/Pinouts/`, selección en `Pinout.h` y
   reglas de conflicto en `PinoutValidation.h`.
3. Declare target y capacidades explícitas en `platformio.ini` y valídelas en
   `Config/VariantValidation.h`. Actualice `scripts/demo_config.py` si la
   variante soporta demos; regenere `platformio.demos.ini` con
   `scripts/generate_demo_envs.py`.
4. Documente el mapa en `pinouts/` y ejecute `scripts/check_pinouts.py`.
   No cambie claves NVS ni formato CSV para introducir una variante.

## Cómo seguir una captura

La entrada es `Application/ActiveApplication.h`. En Dron,
`DronApplication.h` comprueba switches o setup Wi-Fi y llama a
`Context::beginCapture()`. El preflight verifica SD, AFM07 y NTC antes de
`CaptureController::begin()`. Las definiciones que necesitan el `Context`
completo están en `Data/ContextCaptureController.h`, incluido al final de
`Context.h`.

En cada ciclo, `DronApplication::update()` llama a `Context::update()`. Ahí se
sondea la protección térmica, se aplica seguridad de captura y se llama a
`CaptureController::update()`. Ese controlador calcula tiempo y flujo objetivo,
actualiza motor y volumen, y solicita registro cada 10 segundos. Un fallo de
seguridad aborta la captura y pone el motor a PWM cero. El cierre normal
encola el registro final y apaga el motor. `LogService` y `SessionStore` son
los dueños de CSV/SD y NVS.

`lib/EoloCore` solo contiene lógica portable y tipos sin Arduino. Los drivers
y la aplicación están en `src`; todavía no existe una librería
`EoloHardware`.

## Prueba mínima por corte

```sh
pio run -e eolo_express -e eolo_express_legacy -e eolo_standard -e eolo_standard_libraries -e eolo_dron -e eolo_dron_low_power
pio test -e native
python3 scripts/check_pinouts.py
bash scripts/audit_eolo_core_deps.sh
git diff --check
```

Si se toca la composición de `Context`, compile también
`demo_dronprodtest_dron`. La compilación no reemplaza una prueba física de
arranque, captura, apagado seguro del motor y datos guardados en Dron.
Standard, Express y Legacy requieren sus propias placas para validación física.
