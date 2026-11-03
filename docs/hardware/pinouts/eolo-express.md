# Pinout EOLO Express Rev.2

Fuente: `src/Variants/Pinout.h` con `EOLO_TARGET_EXPRESS`.

Referencia: este mapa proviene de la demo secuencial oficial de EOLO Express Rev.2. Tiene su propio bloque de pines en `Pinout.h`, distinto del de EOLO-MP. No habilita módem, anemómetro, doble batería ni NeoPixel. No es headless, por lo que los switches de captura no se usan.

## Resumen

| Grupo | Señal | GPIO | Define firmware |
| --- | --- | --- | --- |
| I2C | SDA | `GPIO21` | `SDA_PIN` |
| I2C | SCL | `GPIO22` | `SCL_PIN` |
| microSD | CS | `GPIO5` | `SD_CS_PIN` |
| microSD | MOSI | `GPIO23` | `SD_MOSI_PIN` |
| microSD | MISO | `GPIO19` | `SD_MISO_PIN` |
| microSD | SCK | `GPIO18` | `SD_SCK_PIN` |
| Batería | ADC | `GPIO34` | `BATTERY_ADC_PIN` |
| AFM07 RS485 | RO (RX) | `GPIO27` | `RS485_RX_PIN` |
| AFM07 RS485 | DI (TX) | `GPIO25` | `RS485_TX_PIN` |
| AFM07 RS485 | DE/RE | `GPIO26` | `RS485_DE_RE_PIN` |
| Bomba 1 | PWM 0 | `GPIO32` | `MOTOR_PWM_PIN_0` |
| Bomba 2 | PWM 1 | `GPIO33` | `MOTOR_PWM_PIN_1` |
| Plantower/PMS | RX (ESP) | `GPIO17` | `PT_RX` |
| Plantower/PMS | TX (ESP) | `GPIO16` | `PT_TX` |
| Periféricos | Alimentación (N-FET) | `GPIO4` | `PPH_PWR_PIN` |
| Switch espera | SW0/SW1 | `EOLO_PIN_UNUSED` | `WAIT_SW*_PIN` |
| Switch duración | SW0/SW1 | `EOLO_PIN_UNUSED` | `DURATION_SW*_PIN` |
| Módem | PWR/RX/TX | `EOLO_PIN_UNUSED` | `MODEM_*_PIN` |
| LED estado | NeoPixel | `EOLO_PIN_UNUSED` | `NEOPIXEL_PIN` |

AFM07: Modbus RTU, ID esclavo 2, 4800 8N1, registro de flujo instantáneo `0x0000`. Caudal = raw / 10 (L/min), según `kAfm07FlowDivisor` en `src/Variants/Profiles/Express.h`. Verificado en equipo: el sensor entrega 5.6 L/min y con `/100` el firmware mostraba 0.56. El firmware usa `SoftwareSerial` sobre GPIO27/25; la demo usa `Serial1` sobre los mismos pines.

## Pantalla

El perfil selecciona `SSD1309_I2C` por defecto: controlador `U8G2_SSD1309_128X64_NONAME0_F_HW_I2C` sobre SDA/SCL `GPIO21/22`, con reloj SPI inactivo (0). La selección no modifica bombas, RS485, Plantower ni módem.

El pinout conserva los pines SPI heredados CS=27, DC=15, RESET=2, SCK=18 y MOSI=23, sin usarlos con I²C. CS comparte `GPIO27` con RX RS485, así que seleccionar una pantalla SPI falla en compilación. Antes de habilitar SPI se necesita validar otro cableado; no se reasignan GPIO para ocultar la colisión. No se ha probado la pantalla en placa durante este refactor.

## Funciones no usadas por firmware Express

| Función | Estado en `eolo_express` |
| --- | --- |
| Módem | No habilitado por flags de build |
| Anemómetro | No habilitado por flags de build |
| Doble batería | No habilitado por flags de build |
| NeoPixel | `EOLO_PIN_UNUSED` |
| NTC motor | `EOLO_PIN_UNUSED` |
| Motor FG | `EOLO_PIN_UNUSED` |
| Switches captura | `EOLO_PIN_UNUSED` (Express no es headless) |

## Notas

- `GPIO34` es solo entrada (batería ADC).
- Bombas DC en `GPIO32/33` (MOSFET low-side, PWM **directo**: duty 0 = apagado).
  El perfil tipado `src/Variants/Profiles/Express.h` fija
  `motorPwmInverted = false`.
- RS485/AFM07 en `GPIO27/25/26`, verificado en hardware (lecturas Modbus OK).
- Plantower/PMS usa `GPIO17` (RX ESP) y `GPIO16` (TX ESP).
- El ambiente `eolo_express` habilita motor PWM directo, AFM07 y Plantower.
