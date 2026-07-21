# Demos de hardware

Las demos de componentes contienen sketches `.ino` que reutilizan la lógica
portable de `EoloCore`. El flujo soportado es PlatformIO: los ambientes
`demo_*` inyectan el modelo de placa, los pines autoritativos y las
dependencias de cada demo. `DronFull` se conserva como diagnóstico secuencial
histórico y `DynamicMotorCalibration` como herramienta interactiva; ninguno es
una segunda implementación del firmware productivo.

- `AFM07/AFM07.ino`: flujo AFM07 por RS485/Modbus RTU usando `ModbusMaster`.
- `Anemometer/Anemometer.ino`: anemometro RS485/Modbus RTU sin libreria Modbus.
- `Plantower/Plantower.ino`: sensor PMS por UART.
- `BME280/BME280.ino`: sensor ambiental por I2C.
- `FS3000/FS3000.ino`: sensor de flujo por I2C.
- `SPIStandardDisplayDiag/SPIStandardDisplayDiag.ino`: patrón mínimo del OLED
  SSD1309 por VSPI para EOLO Standard; no inicializa motores, sensores ni SD.

Las dependencias de cada demo están declaradas en `platformio.demos.ini`; no
hay copias de parsers o tablas específicas para otro flujo de compilación.

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

Para probar primero solo la pantalla Standard:

```bash
pio run -e demo_spistandarddisplaydiag_standard -t upload
pio device monitor -e demo_spistandarddisplaydiag_standard
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
- EOLO Express: RS485 RX `GPIO27`, TX `GPIO25`, DE/RE `GPIO26`; Plantower RX `GPIO17`, TX `GPIO16`; bombas PWM `GPIO32/33`; alimentacion perifericos `GPIO4`.
- EOLO Standard: RS485 RX `GPIO35`, TX `GPIO33`, DE/RE `GPIO26`; OLED SPI `CS27/DC15/RES2`; bombas PWM `GPIO14/25`; Plantower RX `GPIO34`, TX `GPIO32`; alimentacion perifericos `GPIO4`.
- EOLO Express Legacy: RS485 RX `GPIO35`, TX `GPIO33`, DE/RE `GPIO26`; OLED SPI `CS27/DC15/RES2`; bombas PWM `GPIO14/25`; Plantower RX `GPIO16`, TX `GPIO17`; alimentacion perifericos `GPIO4`.
