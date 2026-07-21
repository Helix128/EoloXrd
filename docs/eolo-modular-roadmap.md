# Roadmap de modularización EOLO

Este documento acompaña a [architecture.md](architecture.md), que es la
descripción viva de las capas y sus dependencias.

## Hitos completados

- `lib/EoloCore` local con includes públicos `<Eolo/...>` y ambiente `native`.
- Sesiones basadas en epoch (`Session::startUnix`) sin migración NVS.
- Parser RTC, modelos de flujo, Plantower, anemómetro, NTC, batería, PWM,
  PID, captura y calibración sin dependencias de hardware.
- Adaptadores de sensores con contratos uniformes y demos PlatformIO generadas
  desde una matriz de modelos.
- `src/Board/Pinout.h` como fuente autoritativa, documentación sincronizada y
  auditorías automáticas de pinout/dependencias.
- DTOs portables `LogRecord` y `TelemetrySnapshot`, contrato de configuración
  `SettingsStore` y adaptador `PreferencesSettingsStore`.
- La cola de logging ya transporta el `LogRecord` completo; la tarea de SD
  serializa el DTO sin volver a consultar sensores ni recibir `Context`.
- La sincronización RTC de red recibe módem y RTC explícitos; el wrapper de
  `Context` solo conecta la UI con el servicio.

## Próximos cortes incrementales

1. Separar por completo el formato CSV del backend SD y exponer un writer
   independiente para otras salidas.
2. Conectar el ciclo de upload a una cola de snapshots si se necesita eliminar
   también el wrapper de aplicación; el publicador actual ya consume
   `TelemetrySnapshot` sin leer `Context` directamente.
3. Migrar captura, protección térmica y PID a entradas/salidas explícitas en
   `Context`, conservando los shims de escenas hasta eliminar consumidores
   antiguos.
4. Promover a `EoloHardware` solo drivers que no dependan de perfiles globales,
   `Context` o `Config/Legacy.h`.

Cada corte debe mantener compilables las cuatro variantes, las demos soportadas
y el test nativo de `EoloCore`.
