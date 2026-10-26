# Pruebas

Este directorio reúne las pruebas del firmware y de las herramientas de desarrollo. Hay tres grupos: pruebas en el host (suite nativa y Python) y suites que corren en una placa ESP32.

Compilar no prueba el hardware. Reporta por separado lo que solo compiló, lo que corrió en el host y lo que corrió en una placa.

## Suite nativa

`test/test_eolo_core_native/test_main.cpp` prueba `lib/EoloCore` en el host con Unity, sin placa ni Arduino. Cubre parsers, modelos de sensores, PID y control de flujo, máquinas de captura y calibración, protección térmica y utilidades del módem. El entorno `native` solo ejecuta esta suite.

```bash
pio test -e native
```

Las pruebas portables nuevas van en esa suite, como función `test_<comportamiento>` registrada con `RUN_TEST`.

## Pruebas de Python

`test_firmware_backup.py` y `test_production_qualification.py` prueban `scripts/firmware_backup.py` y `scripts/production_qualification.py`. No necesitan hardware.

```bash
python3 -m unittest discover -s test -p 'test_*.py'
```

## Suites de hardware

Cada carpeta `test/test_<suite>/` es una suite Unity que corre en el ESP32. El entorno base `[embedded]` las incluye y excluye solo la nativa. Ejecutarlas sube el firmware de prueba a la placa conectada, así que usa el entorno de la variante conectada.

```bash
pio test -e eolo_dron_legacy -f test_ntc --test-port /dev/ttyUSB0   # sube y ejecuta una suite
```

`--test-port` indica el puerto de la placa. También puedes fijarlo con la opción `test_port` del entorno, pero `platformio.ini` se versiona: no escribas ahí un puerto local.

Para comprobar solo que una suite compila y enlaza, sin placa:

```bash
pio test -e eolo_dron_legacy -f test_ntc --without-uploading --without-testing
```

`scripts/check_all.sh --with-hw-suites` hace esto con todas las suites en `eolo_dron_legacy`. Esa compilación no ejecuta ninguna prueba.

| Suite | Qué prueba | Hardware necesario |
| --- | --- | --- |
| `test_afm07` | Lectura del AFM07 (ID 2) por RS485 y lectura de flujo con la bomba al 20 %; al final apaga la bomba. | AFM07 en el bus RS485 y bomba con mangueras. El flujo debe superar 0,5 L/min. |
| `test_capture_switches` | Tablas de espera y de duración de los switches de captura, decodificación (SW0 es el bit menos significativo), entrada al setup web y descripciones. | Ninguno. Prueba lógica. |
| `test_dynamic_motor_calibration` | Control dinámico del PWM hacia un objetivo de flujo en `SmartFlowController`: subida, bajada, zona muerta, límite de paso y aprendizaje de ganancia. | Ninguno. |
| `test_flowmotor` | Rampa de PWM, límites, lazo PID de flujo (zona muerta e integral acotado) y matemática de batería y PWM. | Ninguno. |
| `test_headless_motor_calibration` | Validación de la configuración PID, arranque con kick, rechazo de sensores obsoletos y requisitos de SD y sensores frescos para actuar. | Ninguno. |
| `test_headless_setup` | Validación de la configuración del setup headless, programación de flujo por secciones y sanitización de nombres de log y de preset. | Ninguno. |
| `test_i2c` | `I2CBus::begin()` es idempotente: una segunda llamada también devuelve verdadero. | Solo el bus I²C del ESP32. No comprueba dispositivos. |
| `test_log_schema` | Columnas del CSV según las `FEATURE_*`, valor faltante (-1), formato de fila y volumen, y entradas del índice de sesiones. | Ninguno. |
| `test_ntc` | Conversión del ADC a temperatura del termistor, valores extremos inválidos y la histéresis del corte por sobrecalentamiento (umbrales de 70 y 60 °C). | Ninguno. Usa valores ADC de prueba. |
| `test_plantower` | Parser de la trama Plantower: trama válida, checksum inválido y resincronización tras ruido. Además, una lectura real del sensor. | Sensor Plantower conectado al UART2 y alimentado por `PPH_PWR_PIN`. |
| `test_rs485` | Estado del transceptor (DE/RE en recepción tras `begin()`), rechazo de la API bloqueante, validación de endpoints y prioridad del AFM07 frente al anemómetro. | Módulo RS485. El anemómetro (ID 1) es opcional: si falta, su endpoint entra en reintento de 5 s. |
| `test_rtc_timesync` | Parser de la hora del servidor (`unix` y `utc_offset`), de fechas manuales con espacio o formato ISO, y del cálculo de epoch con offset. | Ninguno. No usa el RTC físico. |

Solo `test_afm07` activa la bomba, y ninguna suite usa la microSD. Las suites incluyen `Variants/Legacy.h` o headers de `src/`, por lo que se compilan según el target del entorno. No las ejecutes contra una variante distinta de la placa conectada.

## Verificación completa

```bash
scripts/check_all.sh                                # entornos de firmware, nativa, Python y chequeos estáticos
scripts/check_all.sh --with-hw-suites --with-demos  # además compila suites y demos
```

`check_all.sh` compila todos los entornos de firmware y ejecuta la suite nativa, las pruebas Python, `check_pinouts.py`, `check_family_boundaries.py`, `audit_eolo_core_deps.sh` y `git diff --check`.
