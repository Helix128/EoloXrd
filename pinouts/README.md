# Pinouts técnicos EOLO

Pinouts extraídos del firmware en `src/Board/Pinout.h` y contrastados con `demos/EoloDemoPinout.h`. Esta carpeta documenta el mapeo real usado por los ambientes PlatformIO del repositorio.

## Variantes

| Variante | Target / ambiente | Pinout |
| --- | --- | --- |
| EOLO Dron | `EOLO_TARGET_DRON` / `eolo_dron`, `eolo_dron_low_power` | [eolo-dron.md](eolo-dron.md) |
| EOLO Standard | `EOLO_TARGET_STANDARD` / `eolo_standard` | [eolo-standard.md](eolo-standard.md) |
| EOLO Express | `EOLO_TARGET_EXPRESS` / `eolo_express` | [eolo-express.md](eolo-express.md) |
| EOLO Express Legacy | `EOLO_TARGET_EXPRESS_LEGACY` / `eolo_express_legacy` | [eolo-express-legacy.md](eolo-express-legacy.md) |

## Notas comunes ESP32

- `GPIO34`, `GPIO35`, `GPIO36`, `GPIO37`, `GPIO38` y `GPIO39` son solo entrada.
- `GPIO34-39` no tienen pull-up/down interno; el firmware los configura como `INPUT` cuando aplica.
- El firmware valida conflictos básicos de pines en `src/Board/Pinout.h`.
- `scripts/check_pinouts.py` verifica que `src/Board/Pinout.h`, `demos/EoloDemoPinout.h` y estos documentos sigan sincronizados.
- La configuración de Dron declara `DRONE_SWITCHES_HAVE_EXTERNAL_PULLS` en `platformio.ini`.
- `EOLO_PIN_UNUSED` equivale a `-1`: función no cableada o no usada para ese target.
