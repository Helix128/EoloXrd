# Configuración de compilación

La configuración está separada en cuatro capas:

- `platformio.ini`: selecciona exactamente un target, todas las `FEATURE_*`, el backend I²C y las opciones del compilador.
- `src/Config/Profiles/`: contiene constantes con nombre y el único agregado de tuning coherente (`FlowPidConfig`) de cada modelo. Los perfiles no heredan entre revisiones; para EOLO Standard, editar `Standard.h`.
- `src/Board/Pinouts/`: contiene un mapa completo de GPIO por modelo. `Pinout.h` solo selecciona el mapa y `PinoutValidation.h` valida sus incompatibilidades.
- Los módulos dueños conservan sus constantes internas; logging vive en `Utility/Log.h`.

No agregue valores numéricos de tuning a `build_flags`. Cree o ajuste la constante correspondiente del perfil. Los GPIO de display pertenecen al mapa de pinout, no al perfil. Los macros quedan reservados para seleccionar código (`EOLO_TARGET_*`, `FEATURE_*`) o tipos de compilación. `VariantValidation.h` rechaza targets múltiples, sensores de flujo/control/motor omitidos y combinaciones que no pertenecen al modelo seleccionado.

En PlatformIO ejecute **Config Info** dentro del ambiente, o:

```bash
pio run -e eolo_standard -t config_info
```

La tarea muestra el target y features activos. Los valores ajustables están explícitos en el header del perfil, para que no haya una segunda fuente de verdad en PlatformIO.

La selección de ejecución sigue esta ruta:

```text
platformio.ini -> ActiveProfile + Pinout -> ActiveApplication -> Context/Components
```

`eolo_standard_libraries` es el mismo modelo Standard con el backend I²C por librerías; `eolo_dron_low_power` es un overlay del patrón LED Dron. Ninguno define otro modelo ni otro pinout.
