# Roadmap implementación arquitectura modular EOLO

## Estado actual

Migración incremental activa. `Context` sigue como composition root; módulos nuevos viven en `include/Eolo/...` y código legacy en `src/...` delega gradualmente.

## Ya modularizado

| Módulo | Archivo | Estado | Usado por |
| --- | --- | --- | --- |
| Flow predictor | `include/Eolo/Core/Flow/SmartFlowController.h` | Core real | firmware, demo dinámica, tests |
| Flow motor PID | `include/Eolo/Core/Flow/FlowMotorController.h` | Core nuevo | `MotorCaptureControl` |
| Flow data | `include/Eolo/Types/FlowData.h` | Type | AFM07, FS3000 |
| NTC data | `include/Eolo/Types/NTCData.h` | Type | NTC, logs, motor thermal |
| NTC math | `include/Eolo/Core/Sensors/NtcThermistor.h` | Core real | NTC adapter, tests |
| PWM math | `include/Eolo/Core/Motor/PwmMath.h` | Core real | Motor adapter, tests |
| Plantower data | `include/Eolo/Types/PlantowerData.h` | Type | sensor real, demo Plantower |
| Anemometer data | `include/Eolo/Types/AnemometerData.h` | Type | sensor real |
| Capture switch DTO | `include/Eolo/Types/CaptureSwitchData.h` | Type | hardware switches |
| Capture switch logic | `include/Eolo/Core/Input/CaptureSwitchLogic.h` | Core real | `CaptureSwitches`, tests |
| Battery math | `include/Eolo/Core/Power/BatteryMath.h` | Core real | `Battery`, tests |
| RTC parser | `include/Eolo/Core/Time/RtcTimeParser.h` | Core parcial | `RTCManager`, tests |
| Session DTO | `include/Eolo/Types/Session.h` | Type parcial | app/session/headless setup |
| Headless setup DTO/validation | `include/Eolo/Types/HeadlessSetupTypes.h` | Type parcial | headless server/tests |
| Modem DTO/status | `include/Eolo/Types/ModemTypes.h` | Type | `ModemService`, debug |
| Log status | `include/Eolo/Types/LogTypes.h` | Type | `LogService`, status UI |
| Calibration interpolation | `include/Eolo/Core/Calibration/CalibrationModel.h` | Core real | `CalibrationManager` |
| Settings store contract | `include/Eolo/Services/SettingsStore.h` | Interface | pendiente implementación Preferences |

## Shims temporales

- `src/Data/SmartFlowController.h`
  - Wrapper hacia `<Eolo/Core/Flow/SmartFlowController.h>`.
- `src/Data/Session.h`
  - Wrapper hacia `<Eolo/Types/Session.h>`.
- `src/Board/HeadlessSetupTypes.h`
  - Wrapper hacia `<Eolo/Types/HeadlessSetupTypes.h>` y overload Arduino `String`.
- Drivers legacy (`src/Sensors/*`, `src/Board/*`, `src/Data/*`)
  - Siguen como adaptadores app/hardware; varios delegan en Core/Types.

## Sigue en aplicación / hardware legacy

- `src/Data/Context.h`
  - Composition root. No mover a Core.
- `src/Data/Components.h`
  - Ensambla hardware real por feature flags.
- `src/Data/MotorCaptureControl.h`
  - Ya delega PID a `FlowMotorController`, pero conserva lectura sensores, logs, `Context`, `MotorManager`.
- `src/Data/HeadlessMotorCalibration.h`
  - Pendiente separar máquina de estados pura de `Context`, `Preferences` y motor real.
- `src/Data/CalibrationManager.h`
  - Ya delega sort/validate/interpolación a `CalibrationModel`; persistencia `Preferences` y diagnóstico siguen legacy.
- `src/Data/CaptureController.h`
  - Pendiente separar reglas de sesión/captura de UI, logging y `Context`.
- `src/Data/Logging/LogService.h`
  - Solo `SDStatus` movido; falta formato CSV y cola/SD como servicio.
- `src/Data/UploadService.h` / `src/Utility/SensorAPI.h`
  - Pendiente extraer payload/API core.
- `src/Board/RTCManager.h`
  - Parser movido; backend RTClib/ESP32 sigue hardware.
- `src/Board/CaptureSwitches.h`
  - Decode movido; GPIO/Print sigue hardware.
- `src/Board/Battery.h`
  - Matemática movida; ADC/I2C sigue hardware.

## Deuda técnica conocida

- `include/Eolo/Core/Time/RtcTimeParser.h` usa `RTClib::DateTime`; no incluye `Arduino.h` directo, pero todavía ata el core de tiempo a RTClib. Próximo corte ideal: `EoloDateTime` o DTO propio.
- `include/Eolo/Types/Session.h` también depende de `RTClib::DateTime` por compatibilidad.
- `src/Board/RTCManager.h` conserva helpers privados `extractJsonInt` duplicados/inactivos; pueden eliminarse en limpieza posterior.
- `eolo_express_legacy` ya compila; mantenerlo en validación local para evitar regresiones de configuración.

## Validación requerida tras cambios modulares

```bash
pio run -e eolo_dron
pio run -e eolo_express
pio run -e eolo_express_legacy
pio run -e demo_dynamicmotorcalibration_dron
pio test -e eolo_dron -f test_flowmotor -f test_rtc_timesync -f test_capture_switches -f test_headless_setup -f test_headless_motor_calibration --without-uploading --without-testing
rg -n "Context\.h|Config\.h|Arduino\.h|WiFi\.h|SD\.h|Preferences\.h|WebServer\.h" include/Eolo/Core include/Eolo/Types
```

## Próximos pasos sugeridos

1. Extraer `HeadlessMotorCalibration` core: config, state machine, point storage, validation, PWM runtime conversion.
2. Extraer formato CSV y payload API.
3. Crear implementación `PreferencesSettingsStore` fuera de Core.
4. Migrar hardware adapters a `include/Eolo/Hardware/...` con shims en `src/...`.
5. Reducir `Context` a wiring: quitar reglas de negocio restantes.
6. Eliminar shims solo cuando `rg` confirme cero includes antiguos.

## Reglas activas

- `include/Eolo/Core/**` no debe incluir `Context.h`, `Config.h`, `Arduino.h`, `WiFi.h`, `SD.h`, `Preferences.h` ni `WebServer.h`.
- `include/Eolo/Types/**` debe evitar dependencias de app. RTClib queda como deuda explícita temporal en tiempo/session.
- Demos y firmware real deben usar el mismo core, nunca copias.

## Feature temporal: perfiles dinámicos de flujo + presets webserver

Implementado antes de continuar refactor modular:

- `Session` ahora soporta perfil de flujo por secciones:
  - `MaxFlowSections = 8`
  - `FlowSection { durationSeconds, targetFlow }`
  - `flowSectionCount`
  - `flowSections[]`
- `FlowSchedule` (`include/Eolo/Core/Flow/FlowSchedule.h`) calcula el target efectivo por `elapsedTime`.
- `CaptureController` actualiza `ctx.session.targetFlow` antes de mover motores/loggear.
- `HeadlessSetupConfig` puede representar flujo fijo o secciones dinámicas.
- Webserver headless permite:
  - configurar flujo fijo
  - configurar secciones con duración + flujo
  - guardar/cargar/borrar presets completos
- Webserver headless ya no expone sección ni endpoints PID. PID interno sigue existiendo para firmware/debug.

Comportamiento:

- Si no hay secciones, se usa `targetFlow` fijo toda la sesión.
- Si hay secciones, cada sección usa su flujo durante su duración.
- Si la captura dura más que la suma de secciones, se mantiene el flujo de la última sección.
- Para duración finita, la suma de secciones no puede exceder la duración total.
- Para duración infinita, se permite mantener la última sección indefinidamente.

Persistencia:

- Defaults headless: `Preferences` namespace `eolo_headless`.
- Presets: `Preferences` namespace `eolo_presets`, hasta `HeadlessSetup::kMaxPresets`.
- Sesión activa: `SessionStore` guarda `flowSectionCount` y `flowSections`.

Punto para retomar refactor:

1. Mantener este feature como comportamiento productivo.
2. Extraer persistencia de presets a `Services` cuando exista implementación real de `SettingsStore`.
3. Extraer API/DTO de presets fuera de `HeadlessSetupServer`.
4. Continuar con `HeadlessMotorCalibration` core y hardware adapters.
