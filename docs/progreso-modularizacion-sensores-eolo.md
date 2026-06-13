# Progreso de modularización de sensores EOLO

## Contexto

Se continuó el plan de extracción de lógica reutilizable desde drivers de sensores hacia `include/Eolo`, manteniendo en `src/` la capa dependiente de Arduino/ESP32, FreeRTOS, pines, buses y librerías externas.

El estilo seguido es:

- `include/Eolo/Core/...`: lógica pura, testeable y reutilizable.
- `include/Eolo/Types/...`: estructuras de datos compartidas.
- `src/Sensors/...`: wrappers/adapters de hardware.
- `src/Board/...`: buses, pines, servicios físicos y runtime específico.

## Cambios avanzados

### Nuevos modelos/parsers EOLO creados

- `include/Eolo/Core/Sensors/PlantowerParser.h`
  - Parser de tramas Plantower extraído como lógica pura.
  - Mantiene estado interno, checksum y salida `PlantowerData`.

- `include/Eolo/Core/Sensors/FS3000FlowModel.h`
  - Modelo de conversión velocidad→caudal para FS3000.
  - Incluye tabla e interpolación/extrapolación.

- `include/Eolo/Core/Sensors/AFM07Model.h`
  - Conversión raw→flow para AFM07.
  - Funciones para éxito/error de lectura y cálculo fresh/stale/valid.

- `include/Eolo/Core/Sensors/AnemometerModel.h`
  - Conversión raw speed→m/s y km/h.
  - Funciones para éxito/error de lectura y validez por antigüedad.

- `include/Eolo/Types/BME280Data.h`
  - Tipo común para temperatura, humedad, presión y validez.

### Debug verboso runtime

Se incorporó `src/Utility/DebugFlags.h` para controlar logs verbosos en runtime:

- `EoloDebug::verboseLogsEnabled()`
- `EoloDebug::setVerboseLogsEnabled(bool)`
- `EoloDebug::toggleVerboseLogs()`

Archivos ajustados para usar ese flag:

- `src/Board/Input.h`
- `src/Board/RS485Bus.h`
- `src/Data/CaptureController.h`
- `src/Data/Logging/LogService.h`
- `src/Effectors/Motor.h`
- `src/Sensors/AFM07.h`
- `src/Sensors/Anemometer.h`
- `src/Sensors/Plantower.h`
- `src/Utility/RS485Monitor.h`
- `src/Utility/SystemDebugCommands.h`

Comando debug actualizado:

```txt
verbose [on|off|status|toggle]
```

También acepta `v` como alias.

## Estado actual

El parser/modelos EOLO nuevos están creados, pero todavía falta terminar la adaptación de algunos wrappers para usarlos directamente.

Estado por sensor:

| Sensor | Core EOLO creado | Wrapper adaptado | Pendiente |
|---|---:|---:|---|
| NTC | sí | sí | ninguno relevante |
| Plantower | sí | no completo | reemplazar parser local por include EOLO |
| FS3000 | sí | no | usar `FS3000FlowModel::flowFromVelocity()` |
| AFM07 | sí | no | usar `AFM07Model` en éxito/error/getData |
| Anemometer | sí | no | usar `AnemometerModel` en éxito/error/getData |
| BME280 | tipo creado | no | añadir `getData(BME280Data&)` |

## Tareas pendientes

1. Adaptar `src/Sensors/Plantower.h`
   - Eliminar clase local `PlantowerParser`.
   - Incluir `<Eolo/Core/Sensors/PlantowerParser.h>`.
   - Mantener `Plantower` como adapter hardware.

2. Adaptar `src/Sensors/FS3000.h`
   - Incluir `<Eolo/Core/Sensors/FS3000FlowModel.h>`.
   - Cambiar `getFlow()` para delegar en `FS3000FlowModel::flowFromVelocity()`.
   - Opcional: eliminar tabla local.

3. Adaptar `src/Sensors/AFM07.h`
   - Incluir `<Eolo/Core/Sensors/AFM07Model.h>`.
   - Usar `applyReadSuccess`, `applyReadFailure` y `refreshAge`.

4. Adaptar `src/Sensors/Anemometer.h`
   - Incluir `<Eolo/Core/Sensors/AnemometerModel.h>`.
   - Usar `applyReadSuccess`, `applyReadFailure` y `refreshValidity`.

5. Adaptar `src/Sensors/BME280.h`
   - Incluir `<Eolo/Types/BME280Data.h>`.
   - Añadir `bool getData(BME280Data &output)` sin romper campos públicos existentes.

6. Tests
   - Actualizar `test/test_plantower/test_main.cpp` para incluir parser EOLO directamente.
   - Añadir pruebas de `FS3000FlowModel`.
   - Añadir pruebas unitarias de `AFM07Model` y `AnemometerModel` sin hardware.
   - Ejecutar tests disponibles con PlatformIO.

7. Evaluar después
   - Mover `RS485PatternValidator` a `include/Eolo/Core/Bus/` solo si se desacopla de FreeRTOS/Arduino.
   - Extraer tipos/estadísticas puras de `RS485Monitor` si vale la pena.

## Recomendación de próximos commits

- `adapta wrappers a modelos de sensores eolo`
- `agrega pruebas para modelos de sensores eolo`
