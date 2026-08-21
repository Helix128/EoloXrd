# Arquitectura modular EOLO

La arquitectura se migra de forma incremental y conserva el firmware
header-only. Las capas tienen una dirección única de dependencias:

```text
EoloCore  <-  EoloHardware  <-  src (composición de la aplicación)
```

## EoloCore

`lib/EoloCore/include/Eolo/` contiene tipos, parsers, conversiones y máquinas
de estado que no conocen Arduino, FreeRTOS, RTClib, SD, Wi-Fi, Preferences,
WebServer ni `Context`. Los includes públicos siguen siendo
`<Eolo/Core/...>` y `<Eolo/Types/...>`.

El ambiente `native` de PlatformIO compila esta capa sin placa ni framework
Arduino. El parser RTC produce `RtcDateTime` y epoch; `RTCManager` es el
adaptador que construye `RTClib::DateTime`. `Session::startUnix` es el único
tipo de hora en el core; NVS mantiene la clave histórica `startDate`.

Modelos portables actuales:

- conversión/parsing de AFM07, FS3000, anemómetro y Plantower;
- contrato fresco/obsoleto de sensores y DTO BME280;
- PID y control de ignición del motor;
- protección térmica NTC, PWM, batería y programación de flujo;
- máquinas de captura y calibración de motor;
- `LogRecord`, `TelemetrySnapshot` y estados de sesión/headless.

## EoloHardware y `src`

Los drivers que necesitan pines, buses, librerías externas o tareas ESP32
permanecen en `src/Sensors` y `src/Board` hasta que sus configuraciones sean
explícitas. Son adaptadores del core: `begin()` es idempotente y los sensores
exponen `getData(DTO&)` con validez y frescura definidas. No se mantienen
copias de parsers en las demos.

`src/Data/Context.h` sigue siendo el composition root. Posee componentes,
inicializa hardware, ejecuta el ciclo y conecta acciones del core con motor,
UI, SD y módem. Expone consultas estrechas, no aliases por referencia al
estado de captura, térmico, SD, logging o upload. `LogService` solo recibe
`LogRecord`; `UploadService` solo publica `TelemetrySnapshot`; y
`MotorCaptureControl` recibe motor, lectura de flujo, calibración y estado
térmico explícitos. Eso evita que estos servicios incluyan o recorran
`Context`.

`CaptureController` conserva temporalmente un adaptador de composición porque
su flujo aún coordina escenas, sesión, motor, logging y módem. Sus definiciones
viven en `ContextCaptureController.h`, que se incluye explícitamente después
de terminar `Context`, sin reinclusiones ni macros de orden. La calibración
headless histórica sigue separada del flujo de producto y no se reactiva por
esta organización.

La entrada Arduino no contiene lógica de producto: `main.cpp` instancia
`ActiveApplication` y delega `begin()`/`update()`. El selector elige
`UiApplication` para Express/Legacy/Standard y `DronApplication` para Dron.

## Configuración y persistencia

`src/Config/ActiveProfile.h` expone constantes de variante con nombre y el
contrato `FlowPidConfig`, y selecciona un único perfil por ambiente. Los GPIO
de display viven con el mapa del modelo en `Board/Pinouts/`.
`VariantValidation.h` exige que el entorno declare sus capacidades completas,
y `Board/Pinout.h` selecciona un mapa completo por modelo sin herencia de
pinout. `src/Config/Legacy.h` es solo un shim de macros para código antiguo; el
código nuevo no agrega macros de configuración.

`SettingsStore` es la interfaz portable. `PreferencesSettingsStore` adapta NVS
sin cambiar namespaces, claves ni representación existentes, incluyendo la
clave de sesión `startDate`.

La sincronización RTC de red recibe explícitamente `ModemService` y
`RTCManager`; `Context` solo conserva wrappers para la navegación de escenas.
En Standard, OLED y SD comparten VSPI; `SPIBus::Guard` serializa la
inicialización, el render y las operaciones de SD.

Las URLs y cuerpos HTTP pasan por `ModemHttpContract`: `SensorAPI`, la cola
del módem, la API directa, RTC y la consola comparten el mismo límite y
rechazan explícitamente solicitudes demasiado largas. No se truncan al entrar
a la cola.

## Validación

Antes de un cambio de arquitectura se ejecutan:

```sh
pio run -e eolo_express -e eolo_express_legacy -e eolo_standard -e eolo_standard_libraries -e eolo_dron -e eolo_dron_low_power
pio test -e native
python3 scripts/check_pinouts.py
bash scripts/audit_eolo_core_deps.sh
git diff --check
```

El chequeo de pinouts informa explícitamente una discrepancia Legacy pendiente
de validación física; no cambia firmware ni documentación para ocultarla. Ver
[`hardware-validation.md`](hardware-validation.md).

Las demos soportadas se generan desde `scripts/demo_config.py` y se compilan
con `platformio.demos.ini`; no son un segundo firmware con lógica duplicada.
