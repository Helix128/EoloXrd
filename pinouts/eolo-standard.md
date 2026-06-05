# Pinout EOLO Standard

Fuente: `src/Board/Pinout.h` con `EOLO_TARGET_STANDARD`.

## Resumen

| Grupo | Señal | GPIO | Define firmware |
| --- | --- | --- | --- |
| I2C | SDA | `GPIO21` | `SDA_PIN` |
| I2C | SCL | `GPIO22` | `SCL_PIN` |
| microSD | CS | `GPIO5` | `SD_CS_PIN` |
| microSD | MOSI | `GPIO23` | `SD_MOSI_PIN` |
| microSD | MISO | `GPIO19` | `SD_MISO_PIN` |
| microSD | SCK | `GPIO18` | `SD_SCK_PIN` |
| Switch espera | SW0 | `GPIO32` | `WAIT_SW0_PIN` |
| Switch espera | SW1 | `GPIO14` | `WAIT_SW1_PIN` |
| Switch duración | SW0 | `GPIO36` | `DURATION_SW0_PIN` |
| Switch duración | SW1 | `GPIO39` | `DURATION_SW1_PIN` |
| Batería | ADC | `GPIO34` | `BATTERY_ADC_PIN` |
| RS485 | RX | `GPIO35` | `RS485_RX_PIN` |
| RS485 | TX | `GPIO33` | `RS485_TX_PIN` |
| RS485 | DE/RE | `GPIO26` | `RS485_DE_RE_PIN` |
| Motor | PWM 0 | `GPIO25` | `MOTOR_PWM_PIN_0` |
| Motor | PWM 1 | `GPIO27` | `MOTOR_PWM_PIN_1` |
| Plantower | RX | `GPIO34` | `PT_RX` |
| Plantower | TX | `GPIO32` | `PT_TX` |
| Periféricos | Alimentación | `GPIO4` | `PPH_PWR_PIN` |
| Módem | PWR | `GPIO13` | `MODEM_PWR_PIN` |
| Módem | RX | `GPIO16` | `MODEM_RX_PIN` |
| Módem | TX | `GPIO17` | `MODEM_TX_PIN` |
| LED estado | NeoPixel | `GPIO27` | `NEOPIXEL_PIN` |

## Funciones no usadas en este target

| Función | Define | Valor |
| --- | --- | --- |
| NTC motor | `NTC_PIN` | `EOLO_PIN_UNUSED` |
| Motor FG | `MOTOR_FG_PIN` | `EOLO_PIN_UNUSED` |
| Alimentación motor | `MOTOR_POWER_PIN` | `EOLO_PIN_UNUSED` |

## Notas

- `GPIO34`, `GPIO35`, `GPIO36` y `GPIO39` son solo entrada.
- `GPIO36` y `GPIO39`, usados como switches de duración, se configuran como `INPUT`.
- El firmware Standard habilita módem, anemómetro, doble batería, AFM07 y Plantower mediante flags de `platformio.ini`.
