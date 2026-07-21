# Progreso de modularización de sensores EOLO

La lógica reutilizable vive en `lib/EoloCore`; los adaptadores con Arduino,
FreeRTOS, pines y buses permanecen en `src/Sensors`.

| Sensor | Modelo core | Adaptador | Contrato |
| --- | --- | --- | --- |
| AFM07 | `AFM07Model` | `src/Sensors/AFM07.h` | `getData(FlowData&)`, fresco/obsoleto |
| FS3000 | `FS3000FlowModel` | `src/Sensors/FS3000.h` | `getData(FlowData&)` |
| Anemometer | `AnemometerModel` | `src/Sensors/Anemometer.h` | `getData(AnemometerData&)` |
| Plantower | `PlantowerParser` | `src/Sensors/Plantower.h` | `getData(PlantowerData&)` |
| BME280 | `BME280Data` | `src/Sensors/BME280.h` | `getData(BME280Data&)` |
| NTC | `NtcThermistor` | `src/Sensors/NTC.h` | `getData(NTCData&)` |

Las demos oficiales de PlatformIO incluyen los mismos modelos core. El test
`test_eolo_core_native` cubre conversiones, límites, checksum y transiciones
sin necesitar placa.

La cola de SD ya transporta un `LogRecord` completo y el worker serializa el
DTO sin consultar `Context`. El publicador de upload recibe un
`TelemetrySnapshot` y un adaptador de API explícitos, conservando los IDs,
sentinelas y payloads históricos.

Pendientes de la siguiente etapa: separar por completo el formateador CSV del
backend SD, conectar una cola de snapshots si el flujo de upload lo necesita y
promover drivers a `EoloHardware` solo cuando sus dependencias sean explícitas.
