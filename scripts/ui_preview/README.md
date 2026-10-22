# Vista previa de la UI sin hardware

`build.py` compila el codigo **real** de `src/Ui` (Renderer, GUI, escenas) contra un `Context`
simulado y vuelca cada pantalla OLED de 128x64 a PNG (x4, con rejilla de pixeles).

```
pip install --target <carpeta> ziglang          # compilador C/C++ portable, una sola vez
PYTHONPATH=<carpeta> python scripts/ui_preview/build.py --variant express|standard|dron
```

Requiere haber corrido `pio run` una vez (usa U8g2 de `.pio/libdeps`). Salida en
`build/ui_preview/<variante>/`: un PNG por pantalla, `sheet_header.png` (todos los estados
del header), `sheet_screens.png` y `index.txt` con el orden.

- Los datos de sensores son ficticios (`fillDefaults` en `preview.cpp`): sirve para layout, no mediciones.
- Las variantes solo cambian `FEATURE_*` y numero de motores (ver `VARIANTS` en `build.py`).
- Para agregar una pantalla: incluir su header en `preview.cpp` y agregar un `shot<Escena>("nombre")`.
- Si una escena usa algo nuevo de `Context`, agregarlo al mock de `preview.cpp`.
- No cubre `CalibrationScene` (necesita el `CalibrationManager` real).
