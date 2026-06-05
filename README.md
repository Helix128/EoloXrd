<p align="center">
  <img src="docs/assets/eolo-logo.png" alt="EOLO" width="520">
</p>

# EOLO · Firmware y documentación técnica

Firmware PlatformIO/Arduino para **EOLO**, dispositivo desarrollado por el **C+ Centro de Investigación en Tecnologías para la Sociedad de la Universidad del Desarrollo**, y sus variantes operativas.

Este repositorio contiene firmware, manuales, pinouts técnicos, demos de componentes y recursos de calibración para la familia EOLO.

## Familia EOLO

| Variante | Ambiente PlatformIO | Perfil |
| --- | --- | --- |
| EOLO Dron | `eolo_dron`, `eolo_dron_low_power` | Equipo headless. Switches físicos o setup Wi-Fi, AFM07, NTC, NeoPixel, captura automática. |
| EOLO Standard | `eolo_standard` | Pantalla y encoder, módem, anemómetro, doble batería/DC, AFM07 y Plantower. |
| EOLO Express | `eolo_express` | Pantalla y encoder, AFM07, Plantower, batería simple, sin módem ni anemómetro. |
| EOLO Express Legacy | `eolo_express_legacy` | Express con sensor de flujo FS3000 legacy. |

## Documentación

| Recurso | Descripción |
| --- | --- |
| [Manuales EOLO](docs/README.md) | Índice de manuales de usuario por modelo. |
| [Manual EOLO Dron](docs/manual-eolo-dron.md) | Operación headless, switches, Wi-Fi, LED, datos y mantenimiento. |
| [Manual EOLO Standard](docs/manual-eolo-standard.md) | Operación con pantalla, módem, anemómetro, calibración y datos. |
| [Manual EOLO Express](docs/manual-eolo-express.md) | Operación con pantalla, Plantower, AFM07 y batería simple. |
| [Pinouts técnicos](pinouts/README.md) | GPIO reales extraídos del firmware para cada variante. |
| [Demos independientes](demos/README.md) | Sketches para probar sensores y componentes aislados. |
| [Calibración](Calibración.md) | Notas y recursos de calibración. |

## Requisitos de desarrollo

- PlatformIO Core o PlatformIO IDE.
- Placa ESP32 compatible con `esp32dev`.
- Cable USB de datos.
- Hardware EOLO según variante.

Instalación rápida de PlatformIO Core:

```bash
python3 -m pip install platformio
```

## Ambientes PlatformIO

| Ambiente | Uso |
| --- | --- |
| `eolo_express` | Build default. Display, AFM07, Plantower. |
| `eolo_express_legacy` | Express con FS3000 legacy. |
| `eolo_standard` | Display, módem, anemómetro, doble batería, AFM07, Plantower. |
| `eolo_dron` | Equipo headless. Switches, AFM07, NTC, NeoPixel, setup Wi-Fi headless. |
| `eolo_dron_low_power` | Dron headless con LED de bajo consumo. |

Comandos básicos:

```bash
pio run                          # compila ambiente default eolo_express
pio run -e eolo_dron              # compila variante dron
pio run -e eolo_dron -t upload     # sube firmware
pio device monitor -b 115200       # monitor serial
```

`platformio.demos.ini` agrega ambientes `demo_*` generados desde `demos/`.

## Dependencias

Declaradas en `platformio.ini`:

- `adafruit/Adafruit Unified Sensor@^1.1.15`
- `adafruit/Adafruit BME280 Library@^2.3.0`
- `olikraus/U8g2@^2.36.14`
- `sparkfun/SparkFun_FS3000_Arduino_Library@^1.0.5`
- `adafruit/RTClib@^2.1.4`
- `4-20ma/ModbusMaster@^2.0.1`
- `plerup/EspSoftwareSerial@^8.2.0`
- `vshymanskyy/TinyGSM@^0.12.0`
- `adafruit/Adafruit NeoPixel@^1.12.5` solo en ambientes Dron.

La plataforma ESP32 está fijada en `espressif32@6.10.0` para builds reproducibles.

## Pinouts técnicos

Los pinouts están centralizados en [`pinouts/`](pinouts/README.md) y se derivan de `src/Board/Pinout.h`:

- [EOLO Dron](pinouts/eolo-dron.md)
- [EOLO Standard](pinouts/eolo-standard.md)
- [EOLO Express](pinouts/eolo-express.md)
- [EOLO Express Legacy](pinouts/eolo-express-legacy.md)

Nota ESP32: `GPIO34-39` son solo entrada y no tienen pull-up/down interno; el firmware los configura como `INPUT` cuando aplica.

## Setup Wi-Fi headless

`HeadlessSetup` es una feature genérica para equipos sin pantalla. Actualmente se activa en `eolo_dron` cuando los switches de espera están en `Off`.

Defaults:

```text
SSID: eolo-dron
Password: eolo-dron
URL: http://eolo.setup/ o http://192.168.4.1/
```

Credenciales pueden cambiarse con build flags:

```ini
-DHEADLESS_SETUP_AP_SSID=\"mi-red-setup\"
-DHEADLESS_SETUP_AP_PASSWORD=\"clave-local\"
```

No hay credenciales privadas de red externa en el repositorio.

## Demos

```bash
./scripts/demo.py list
./scripts/demo.py build AFM07 dron
./scripts/demo.py upload AFM07 dron
./scripts/demo.py monitor AFM07 dron
```

Ver [`demos/README.md`](demos/README.md).

## Tests

Hay tests en `test/`. Varios requieren hardware real conectado, por ejemplo AFM07, RS485, Plantower o RTC.

```bash
pio test -e eolo_dron
```

## Mantenimiento

- `src/Data/Context.h` concentra demasiadas responsabilidades. Dividir gradualmente cuando se toque captura, logging, SD o sincronización RTC.
- Evitar volver a vendorizar librerías del core ESP32 como `Wire`; usar framework y `lib_deps`.
- Mantener `platformio.ini` como fuente única de dependencias.
- Mantener `pinouts/` sincronizado con `src/Board/Pinout.h` y `demos/EoloDemoPinout.h`.
- Archivos generados/cache como `.pio/`, `__pycache__/` y `.vscode` local no deben versionarse salvo recomendaciones estables.
