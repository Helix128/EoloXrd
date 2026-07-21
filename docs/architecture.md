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
UI, SD y módem. El ciclo captura un `LogRecord` y la cola de SD lo transporta
completo a un worker que no consulta `Context`; el wrapper de aplicación aún
conserva la compatibilidad de escenas y la publicación remota. Los servicios
conservan shims mientras las escenas migran a `UiSnapshot` y comandos pequeños.

## Configuración y persistencia

`src/Config/ActiveProfile.h` expone `EoloConfig` tipado y selecciona un único
perfil por ambiente. `src/Config/Legacy.h` es solo un shim de macros para
código antiguo; el código nuevo no agrega macros de configuración.

`SettingsStore` es la interfaz portable. `PreferencesSettingsStore` adapta NVS
sin cambiar namespaces, claves ni representación existentes, incluyendo la
clave de sesión `startDate`.

La sincronización RTC de red recibe explícitamente `ModemService` y
`RTCManager`; `Context` solo conserva wrappers para la navegación de escenas.
En Standard, OLED y SD comparten VSPI; `SPIBus::Guard` serializa la
inicialización, el render y las operaciones de SD.

## Validación

Antes de un cambio de arquitectura se ejecutan:

```sh
pio run -e eolo_express -e eolo_express_legacy -e eolo_standard -e eolo_dron
pio test -e native
python3 scripts/check_pinouts.py
bash scripts/audit_eolo_core_deps.sh
git diff --check
```

Las demos soportadas se generan desde `scripts/demo_config.py` y se compilan
con `platformio.demos.ini`; no son un segundo firmware con lógica duplicada.
