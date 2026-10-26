# Demos de hardware

Las demos son sketches `.ino` aislados para probar un componente en una placa. Sirven para separar un problema de hardware de la lógica del firmware.

No son un segundo firmware: reutilizan los modelos de `EoloCore` y los pines de `src/Variants/Pinout.h` (vía `EoloDemoPinout.h`). Cada demo se compila como un entorno `demo_<demo>_<modelo>`, que se genera a partir de `scripts/demo_config.py` en `platformio.demos.ini`.

## Demos disponibles

| Demo | Qué prueba | Modelos |
| --- | --- | --- |
| `AFM07` | Flujo AFM07 por RS485/Modbus RTU con `ModbusMaster` | `dron_legacy`, `standard`, `express`, `express_legacy` |
| `AFM07ProfPins` | AFM07 sobre el cableado RS485 de Express, probando dos configuraciones de ID y baudrate | `express` |
| `AFM07Scan` | Barrido de baudrates e IDs 1 a 20 buscando un esclavo Modbus que responda; solo lecturas | `express` |
| `Anemometer` | Anemómetro RS485/Modbus RTU, sin librería Modbus | `standard`, `express`, `express_legacy` |
| `BME280` | Sensor ambiental por I²C | `dron_legacy`, `standard`, `express`, `express_legacy` |
| `DIPSwitchDiag` | Lectura en tiempo real de los cuatro switches DIP | `dron_legacy` |
| `DronBase` | Referencia validada en hardware del Mini Rev.2: OLED SH1106, encoder, SD, RTC, BME280, bomba PWM, AFM07 y batería 2S | `dron` |
| `DronEncoderTest` | Concurrencia de OLED, encoder en `Wire1` y sensores; mide los eventos perdidos del encoder | `dron` |
| `DronFull` | Diagnóstico secuencial completo del Mini Rev.1 | `dron_legacy` |
| `DronI2CDiag` | Bus I²C principal del Mini Rev.2: pull-ups, escaneo y errores por dispositivo con y sin OLED | `dron` |
| `DronProdTest` | Prueba de calificación de producción del Mini Rev.1 | `dron_legacy` |
| `DronSdDiag` | microSD por SPI: niveles de MISO, CMD0/CMD8 manuales y montaje a varias frecuencias | `dron` |
| `DynamicMotorCalibration` | Calibración dinámica interactiva de motor en lazo cerrado con AFM07, sin guardar en flash | `dron_legacy` |
| `FS3000` | Sensor de flujo FS3000 por I²C | `dron_legacy`, `standard`, `express`, `express_legacy` |
| `I2CDisplayDiag` | Escaneo I²C y patrón de prueba en pantallas SSD1306 y SSD1309 | `express` |
| `MotorDirDiag` | Dirección del PWM de las bombas, con control por serial; arranca siempre en 0 | `express` |
| `Plantower` | Sensor de partículas PMS por UART | `standard`, `express`, `express_legacy` |
| `SPIStandardDisplayDiag` | Demo secuencial completo de EOLO-MP (v24): OLED SSD1309 por VSPI, bombas, AFM07, anemómetro, BME280, RTC y SD | `standard` |

La tabla sale de `scripts/demo_config.py`; `./scripts/demo.py list` muestra lo mismo. Los modelos son `dron` (EOLO Mini Rev.2), `dron_legacy` (Mini Rev.1), `standard` (EOLO-MP Rev.1), `express` (Express Rev.2) y `express_legacy` (Express Rev.1). `DronFull` y `DynamicMotorCalibration` son herramientas de diagnóstico, no parte del flujo productivo.

## Compilar, subir y monitorear

```bash
./scripts/demo.py list                        # demos y modelos soportados
./scripts/demo.py build AFM07 dron_legacy     # compila
./scripts/demo.py upload AFM07 dron_legacy    # compila y sube
./scripts/demo.py monitor AFM07 dron_legacy   # monitor serial
```

El equivalente directo con PlatformIO:

```bash
python3 scripts/generate_demo_envs.py                # regenera platformio.demos.ini
pio run -e demo_afm07_dron_legacy                    # compila
pio run -e demo_afm07_dron_legacy -t upload          # sube
pio device monitor -e demo_afm07_dron_legacy         # monitor serial
pio run -e demo_afm07_dron_legacy -t demo_info       # resumen del entorno
```

`scripts/check_all.sh --with-demos` compila todas.

## Pines

Cada sketch declara arriba un bloque `#define EOLO_MODEL_*` por defecto; los entornos `demo_*` pasan su modelo con `-D` en `platformio.demos.ini`. Los pines están en `EoloDemoPinout.h` y deben coincidir con `src/Variants/Pinout.h`; `scripts/check_pinouts.py` lo comprueba. El mapa por modelo está en [docs/hardware/pinouts/](../docs/hardware/pinouts/README.md).

## Agregar una demo

Los pasos están en [desarrollo.md](../docs/firmware/desarrollo.md#agregar-una-demo).
