# Pinout EOLO Express

Fuente: `src/Board/Pinout.h` con `EOLO_TARGET_EXPRESS`.

**Ground truth:** este mapa proviene de la demo secuencial oficial del EOLO
Express (demo del profesor). El firmware Express **ya no** comparte el bloque de
pines con Standard: tiene su propio bloque en `Pinout.h`. No habilita módem,
anemómetro, doble batería ni NeoPixel. Express no es headless, por lo que los
switches de captura no se usan.

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

**AFM07:** Modbus RTU, ID esclavo **2**, **4800 8N1**, registro de flujo
instantáneo `0x0000` (raw/10 = L/min). El firmware usa `SoftwareSerial` sobre
GPIO27/25; la demo del profe usa `Serial1` sobre los mismos pines.

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
| Módem | `EOLO_PIN_UNUSED` |

## Notas

- `GPIO34` es solo entrada (batería ADC).
- Bombas DC en `GPIO32/33` (MOSFET low-side, PWM **directo**: duty 0 = apagado).
  El perfil tipado `src/Config/Profiles/Express.h` fija
  `motorPwmInverted = false`.
- RS485/AFM07 en `GPIO27/25/26`, verificado en hardware (lecturas Modbus OK).
- Plantower/PMS usa `GPIO17` (RX ESP) y `GPIO16` (TX ESP).
- El ambiente `eolo_express` habilita motor PWM directo, AFM07 y Plantower.
