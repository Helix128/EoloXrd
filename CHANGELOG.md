# Registro de cambios

Formato basado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/). Las versiones siguen [SemVer](https://semver.org/lang/es/) y cada una se publica como release y en la [página de flasheo](https://helix128.github.io/EoloXrd/).

## [Sin publicar]

### Agregado

- Pantalla: tras 15 s sin actividad baja el brillo con suavidad y a los 30 s hace fade out y se apaga; al interactuar vuelve de inmediato. Nuevo menú «Opciones» (en el inicio y en la pausa de captura) con «Pantalla» (brillo y ambos tiempos, que se guardan) y «Reloj» (el ajuste de reloj, que sale de esos menús). «Sincronizar» solo se muestra en equipos con modem. Una captura activa conserva el comportamiento de apagado.

### Corregido

- Al terminar «Probar bombas» durante una captura, la interfaz vuelve a la información de captura y no al menú de pausa.

## [1.0.0] - 2026-10-26

### Agregado

- Plantilla `src/Headless/HeadlessSecrets.example.h` para las credenciales Wi-Fi locales.
- Integración continua en `.github/workflows/ci.yml`.
- Releases automáticos al subir un tag `vX.Y.Z` y página de flasheo web en GitHub Pages (`flasher/`), con todas las versiones publicadas.
- `CHANGELOG.md`.

### Cambiado

- Las credenciales de la red local del Mini Rev.1, y opcionalmente la clave del AP de setup, se leen desde `src/Headless/HeadlessSecrets.h` (ignorado por git) o desde flags `-D`. El log del setup ya no imprime la contraseña del AP.
- El generador de la página de setup compara el HTML descomprimido, así que `HeadlessSetupWebPage.h` solo cambia cuando cambia el HTML. Falla si falta el logo o si `index.html` no calza con los patrones esperados.
- El logo de la página de setup está en `web-server/`.
- Se reorganizó la raíz y `docs/`, que ahora se divide en `manuales/`, `firmware/`, `hardware/`, `datos/` y `archivo/`.
- Se reescribieron `README.md`, `AGENTS.md` y los índices de documentación.
- La documentación usa los nombres de producto: EOLO Mini Rev.1/Rev.2 (antes Dron Legacy/V2), EOLO-MP (antes Standard) y EOLO Express Rev.1/Rev.2. Los envs e identificadores del código no cambian.

### Corregido

- `SensorAPI` serializa el acceso al spool de telemetría en la SD entre el loop y la tarea del módem (`SPIBus::Guard`) y usa contadores atómicos.
- `./eolo help`, los menús de `.vscode/tasks.json` y `generate_demo_envs.py`, que reescribía `platformio.demos.ini` sin cambios.

### Eliminado

- Notas personales, placeholders y logos duplicados de la raíz; el material de calibración pasó a `docs/hardware/calibracion/`.

## Historial anterior

Resumen por mes, tomado del historial de git.

### 2026-10

- Soporte para EOLO Mini Rev.2 (`eolo_dron`) y separación del Mini Rev.1, con demos y herramientas propias.
- Correcciones de motor para Mini Rev.2, I2C y de la interfaz.
- Mejoras de convergencia del control PID de flujo.
- Pruebas de SD.
- Corrección de página vacía en captura sin Plantower.

### 2026-09

- Refactor de configuraciones y variantes (perfiles y pinouts por modelo; PR #1).
- Protecciones para el motor y LED RGB modificado.

### 2026-08

- Mini Rev.1 funcional, con pruebas propias.
- Índice de logs, correcciones de logs y de respaldos.
- Mejoras de estabilidad ante errores.

### 2026-07

- Rediseño de RS485 para corregir interferencias entre AFM07 y anemómetro.
- Correcciones para Express, EOLO-MP y Mini; ajustes de timing.

### 2026-06

- Setup Wi-Fi headless, portal cautivo y servidor web con presets para el Mini Rev.1.
- Protección térmica del motor con NTC y LED.
- Control de flujo, calibración headless y capa de datos (sesiones, subida, sincronización RTC).
- Dependencias gestionadas desde PlatformIO.
- Extracción de `EoloCore`, demos desde PlatformIO, manuales y pinouts.

### 2026-05

- Terminal de depuración y comandos adicionales.
- Rediseño del sistema de interfaz, switches físicos y target `eolo_dron`.
- Estado de batería del RTC, sincronización de hora y correcciones del módem.
- Consistencia en los buses I2C y RS485.

### 2026-04

- Rediseño de RS485 para concurrencia y de la calibración.
- Los flags de modelo pasan a feature flags por función (batería doble, módem, etc.).

### 2026-03

- Calibración separada del contexto (`CalibrationManager`) y mejoras de logging de Plantower.
- Rediseños de RS485.

### 2026-01

- Lectura de sensores con tareas nativas de FreeRTOS, en reemplazo de `ESPJob`.

### 2025-12

- Biblioteca Wire con manejo de errores, depuración del BME280 y corrección de Preferences/NVS.
- Estandarización de pines de energía de periféricos y módem.

### 2025-10 y 2025-11

- Primer commit, guardado y carga de sesiones, pruebas de motor.
- Entorno PlatformIO para EOLO grande y chico, y rediseño de la batería.
