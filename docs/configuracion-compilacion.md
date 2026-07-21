# Configuración de compilación

La configuración está separada en tres capas:

- `platformio.ini`: selecciona el target, las `FEATURE_*`, dependencias y opciones del compilador.
- `src/Config/Profiles/`: contiene los valores tipados de cada modelo. Para EOLO Standard, editar `Standard.h`.
- Los módulos dueños conservan sus constantes internas; logging vive en `Utility/Log.h`.

No agregue valores numéricos de tuning a `build_flags`. Cree o ajuste el valor correspondiente del perfil. Los macros quedan reservados para seleccionar código (`EOLO_TARGET_*`, `FEATURE_*`) o tipos de compilación.

En PlatformIO ejecute **Config Info** dentro del ambiente, o:

```bash
pio run -e eolo_standard -t config_info
```

La tarea muestra el target y features activos. Los valores ajustables están explícitos en el header del perfil, para que no haya una segunda fuente de verdad en PlatformIO.
