# Demos independientes

Cada carpeta contiene un sketch `.ino` autosuficiente para probar un componente sin depender de `src`, `RS485Bus`, managers ni la aplicacion principal.

Abrir directamente la carpeta del componente en Arduino IDE:

- `AFM07/AFM07.ino`: flujo AFM07 por RS485/Modbus RTU usando `ModbusMaster`.
- `Anemometer/Anemometer.ino`: anemometro RS485/Modbus RTU sin libreria Modbus.
- `Plantower/Plantower.ino`: sensor PMS por UART.
- `BME280/BME280.ino`: sensor ambiental por I2C.
- `FS3000/FS3000.ino`: sensor de flujo por I2C.

Configurar Arduino IDE:

- Placa: ESP32 compatible con el hardware EOLO.
- Monitor serie: `115200`.
- Para `BME280`: instalar `Adafruit BME280 Library` y `Adafruit Unified Sensor`.
- Para `FS3000`: instalar `SparkFun FS3000 Arduino Library`.
- Para `AFM07`: instalar `ModbusMaster`.
- `Anemometer` y `Plantower` usan solo librerias incluidas con el core ESP32 Arduino.

En PlatformIO no se instalan librerias manualmente: las dependencias de cada demo estan declaradas en `platformio.demos.ini`.

## PlatformIO

Las demos tambien se pueden compilar y subir desde la raiz del repo. El archivo
`platformio.demos.ini` se genera automaticamente desde las carpetas `demos/*/*.ino`.
Cada demo queda como un ambiente `demo_*`, por lo que en VSCode/PlatformIO aparece
en Project Tasks con las tareas normales `Build`, `Upload` y `Monitor`.

Comandos utiles:

```bash
./scripts/demo.py list
./scripts/demo.py build AFM07 dron
./scripts/demo.py upload AFM07 dron
./scripts/demo.py monitor AFM07 dron
```

Equivalente directo con PlatformIO:

```bash
python3 scripts/generate_demo_envs.py
pio run -e demo_afm07_dron
pio run -e demo_afm07_dron -t upload
pio device monitor -e demo_afm07_dron
```

Tambien hay un target custom por ambiente para ver informacion rapida:

```bash
pio run -e demo_afm07_dron -t demo_info
```

Para agregar una demo nueva, crear `demos/NuevaDemo/NuevaDemo.ino` y registrar
modelos/dependencias en `scripts/demo_config.py` solo si necesita algo distinto
al default.

Modelo de pinout:

- Cada sketch tiene arriba un bloque `#define EOLO_MODEL_*`.
- Para cambiar de placa, dejar activo solo uno entre `EOLO_MODEL_DRON`, `EOLO_MODEL_STANDARD`, `EOLO_MODEL_EXPRESS` o `EOLO_MODEL_EXPRESS_LEGACY`.
- Los pines estan en `EoloDemoPinout.h`.
- EOLO Dron: RS485 RX `GPIO16`, TX `GPIO17`, DE/RE `GPIO4`; motor PWM `GPIO26`; FG `GPIO35`; DIP `GPIO32`, `GPIO33`, `GPIO14`, `GPIO13`.
- EOLO Standard/Express/Legacy: RS485 RX `GPIO35`, TX `GPIO33`, DE/RE `GPIO26`; Plantower RX `GPIO34`, TX `GPIO32`; alimentacion perifericos `GPIO4`.
