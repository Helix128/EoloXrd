# Vista previa de la UI sin hardware

`build.py` compila el codigo **real** de `src/Ui` (Renderer, GUI, escenas) contra un `Context`
simulado y vuelca cada pantalla OLED de 128x64 a PNG (x4, con rejilla de pixeles).

```
pip install --target <carpeta> ziglang          # opcional: compilador C/C++ portable
PYTHONPATH=<carpeta> python scripts/ui_preview/build.py --variant express|express_legacy|standard|dron
```

Requiere haber corrido `pio run` una vez (usa U8g2 de `.pio/libdeps`). Salida en
`build/ui_preview/<variante>/`: un PNG por pantalla, `sheet_header.png` (todos los estados
del header), `sheet_screens.png` y `index.txt` con el orden.

- Los datos de sensores son ficticios (`fillDefaults` en `preview.cpp`): sirve para layout, no mediciones.
- Las variantes solo cambian `FEATURE_*` y numero de motores (ver `VARIANTS` en `build.py`).
- Si no hay `ziglang`, usa `cc` y `c++` instalados. La variante `express_legacy` reproduce el perfil FS3000.
- Para agregar una pantalla: incluir su header en `preview.cpp` y agregar un `shot<Escena>("nombre")`.
- `--check-calibration` ejecuta la `CalibrationScene` real con sensor y motores simulados, y renderiza intro, discovery, rampas, pausa y terminales. No escribe NVS; el mock usa `CalibrationModel` real y controla el resultado de guardado.
- Si una escena usa algo nuevo de `Context`, agregarlo al mock de `preview.cpp`.
