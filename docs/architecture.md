# Arquitectura firmware EOLO

## `Context`

`src/Data/Context.h` es el orquestador de alto nivel. Mantiene compatibilidad con escenas, servidor headless y utilidades de debug, pero no debe acumular lógica de bajo nivel nueva.

Regla práctica: si un cambio toca SD, CSV, API, sincronización RTC, estado de captura o cálculo de PWM, debe vivir en servicio dedicado y `Context` solo debe delegar.

## Servicios extraídos

El código de `src/Data/` se mantiene header-only: declaraciones en `*.h`, implementaciones `inline` en `*Impl.h`, incluidas desde `Context.h` tras definir `Context`.

| Responsabilidad | Servicio |
| --- | --- |
| SD, directorios, cola FreeRTOS de log, CSV local | `src/Data/Logging/LogService.h`, `src/Data/Logging/LogServiceImpl.h` |
| Persistencia NVS de sesión | `src/Data/SessionStore.h`, `src/Data/SessionStoreImpl.h` |
| Sincronización RTC por red/módem | `src/Data/RTCNetworkSync.h`, `src/Data/RTCNetworkSyncImpl.h` |
| Estado y ciclo de captura | `src/Data/CaptureController.h`, `src/Data/CaptureControllerImpl.h` |
| Protección térmica y control de motores | `src/Data/MotorCaptureControl.h`, `src/Data/MotorCaptureControlImpl.h` |
| Upload/API de sensores | `src/Data/UploadService.h`, `src/Data/UploadServiceImpl.h` |

## Compatibilidad temporal

`Context` conserva wrappers como `logData()`, `initSD()`, `beginCapture()`, `startRTCNetworkSync()` y campos históricos como `isCapturing`, `sdStatus`, `uploadPending`. Son aliases/delegaciones para evitar reescribir escenas y endpoints en la misma migración.

Nueva lógica debe usar el servicio correspondiente internamente. Cuando las escenas estén desacopladas, esos wrappers pueden eliminarse gradualmente.

## Dependencias

- `platformio.ini` sigue siendo fuente única de dependencias externas.
- No vendorizar librerías del core ESP32 (`Wire`, `SD`, `Preferences`, etc.). Usar framework Arduino ESP32 y `lib_deps`.
- En código header-only, toda función definida fuera de la clase debe ser `inline`.
- Evitar definiciones globales no `inline` en headers.

## Pinouts

Mantener sincronizados:

- `src/Board/Pinout.h`
- `demos/EoloDemoPinout.h`
- `pinouts/*.md`

Chequeo local:

```sh
scripts/check_pinouts.py
```

Compilación recomendada tras tocar arquitectura o pinout:

```sh
pio run -e eolo_express -e eolo_express_legacy -e eolo_standard -e eolo_dron -e eolo_dron_low_power
```
