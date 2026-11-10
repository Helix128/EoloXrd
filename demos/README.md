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
| `FS3000FlowTune` | Curva PWM→caudal de cada bomba y en serie, ruido y respuesta a escalón del FS3000, en CSV por serial | `express_legacy` |
| `I2CDisplayDiag` | Escaneo I²C y patrón de prueba en pantallas SSD1306 y SSD1309 | `express` |
| `MotorDirDiag` | Dirección del PWM de las bombas, con control por serial; arranca siempre en 0 | `express` |
| `Plantower` | Sensor de partículas PMS por UART | `standard`, `express`, `express_legacy` |
| `SPIStandardDisplayDiag` | Demo secuencial completo de EOLO-MP (v24): OLED SSD1309 por VSPI, bombas, AFM07, anemómetro, BME280, RTC y SD | `standard` |
| `SystemDiag` | Inventario y diagnóstico serial por perfil de la placa | `dron`, `dron_legacy`, `standard`, `express`, `express_legacy`, `express_legacy_spi` |

La tabla sale de `scripts/demo_config.py`; `./scripts/demo.py list` muestra lo mismo. Los modelos son `dron` (EOLO Mini Rev.2), `dron_legacy` (Mini Rev.1), `standard` (EOLO-MP Rev.1), `express` (Express Rev.2), `express_legacy` (Express Rev.1) y `express_legacy_spi` (Express Rev.1 con cableado alternativo). `DronFull` y `DynamicMotorCalibration` son herramientas de diagnóstico, no parte del flujo productivo.

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

## Diagnóstico SystemDiag

Selecciona explícitamente el modelo físico: no existe una identificación
automática fiable y los mismos GPIO pueden tener funciones distintas entre
placas. Cargar esta demo reemplaza el firmware de aplicación: conserva antes
un respaldo verificable del firmware instalado y cómo restaurarlo. Un respaldo
de una compilación local no demuestra que sea idéntica al firmware instalado.

Para EOLO Express Rev.1, por ejemplo:

```bash
./scripts/demo.py build SystemDiag express_legacy
./scripts/demo.py upload SystemDiag express_legacy
./scripts/demo.py monitor SystemDiag express_legacy
```

La demo utiliza el pinout del perfil seleccionado; no valida automáticamente
que corresponda a la placa conectada. `express_legacy_spi` selecciona el
[cableado alternativo con OLED SPI](../docs/hardware/pinouts/eolo-express-legacy-alt.md),
todavía sin validar físicamente. No lo selecciones si desconoces el cableado
de la placa.

En el monitor, escribe `r` para repetir la ronda o `p` para escuchar el
Plantower hasta 30 segundos. Una trama válida con PM cero se distingue de la
ausencia de datos válidos; en ese caso se muestran bytes, cabeceras y una muestra
hexadecimal. Según las capacidades del perfil, comprueba:

- Bus I²C y lectura de BME280, estado OSF del RTC, FS3000, encoder y batería
  doble cuando corresponda.
- Montaje y lectura de la raíz de la SD; existencia de claves NVS.
- Entradas de batería ADC, NTC y DIP; comunicación Modbus con AFM07 y
  anemómetro; recepción UART del Plantower, cuando el perfil los contempla.

Las pruebas no verifican funcionamiento de motores, imagen de la pantalla,
precisión de sensores, validez de los datos NVS, escritura en SD ni módem.
Una respuesta I²C por sí sola no demuestra que el dispositivo funcione ni que
la placa sea de un modelo determinado. El diagnóstico no solicita escritura en
RTC, NVS ni archivos de la SD, ni ejecuta una prueba de motores. El BME280 se configura
temporalmente para tomar la muestra. La demo ejecuta pruebas secuenciales y no
reproduce la concurrencia del firmware productivo.

Para pruebas adicionales, consulta las demos compatibles de la tabla: por
ejemplo, `DronI2CDiag` y `DronSdDiag` para Mini Rev.2, `DronFull` para Mini
Rev.1, `SPIStandardDisplayDiag` para EOLO-MP, `I2CDisplayDiag` y `MotorDirDiag`
para Express Rev.2, y `FS3000FlowTune` para Express Rev.1. Algunas activan
motores o realizan escrituras: revisa sus instrucciones antes de ejecutarlas.

Para comprobar la compilación de todos los perfiles:

```bash
for modelo in dron dron_legacy standard express express_legacy express_legacy_spi; do
    ./scripts/demo.py build SystemDiag "$modelo" || break
done
```

La compilación no sustituye las pruebas físicas con cada placa.

## Pines

Cada sketch declara arriba un bloque `#define EOLO_MODEL_*` por defecto; los entornos `demo_*` pasan su modelo con `-D` en `platformio.demos.ini`. Los pines están en `EoloDemoPinout.h` y deben coincidir con `src/Variants/Pinout.h`; `scripts/check_pinouts.py` lo comprueba. El mapa por modelo está en [docs/hardware/pinouts/](../docs/hardware/pinouts/README.md).

## Agregar una demo

Los pasos están en [desarrollo.md](../docs/firmware/desarrollo.md#agregar-una-demo).
