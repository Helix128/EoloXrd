# Pinout EOLO Dron

Fuente: `src/Board/Pinout.h` con `EOLO_TARGET_DRON`.

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
| Switch espera | SW1 | `GPIO33` | `WAIT_SW1_PIN` |
| Switch duración | SW0 | `GPIO14` | `DURATION_SW0_PIN` |
| Switch duración | SW1 | `GPIO13` | `DURATION_SW1_PIN` |
| LED estado | NeoPixel | `GPIO27` | `NEOPIXEL_PIN` |
| Temperatura motor | NTC | `GPIO34` | `NTC_PIN` |
| RS485 | RX | `GPIO16` | `RS485_RX_PIN` |
| RS485 | TX | `GPIO17` | `RS485_TX_PIN` |
| RS485 | DE/RE | `GPIO4` | `RS485_DE_RE_PIN` |
| Motor | PWM | `GPIO26` | `MOTOR_PWM_PIN_0` |
| Motor | FG | `GPIO35` | `MOTOR_FG_PIN` |

## Funciones no usadas en este target

| Función | Define | Valor |
| --- | --- | --- |
| Batería ADC | `BATTERY_ADC_PIN` | `EOLO_PIN_UNUSED` |
| Plantower RX | `PT_RX` | `EOLO_PIN_UNUSED` |
| Plantower TX | `PT_TX` | `EOLO_PIN_UNUSED` |
| Alimentación periféricos | `PPH_PWR_PIN` | `EOLO_PIN_UNUSED` |
| Módem PWR | `MODEM_PWR_PIN` | `EOLO_PIN_UNUSED` |
| Módem RX | `MODEM_RX_PIN` | `EOLO_PIN_UNUSED` |
| Módem TX | `MODEM_TX_PIN` | `EOLO_PIN_UNUSED` |
| Motor PWM 1 | `MOTOR_PWM_PIN_1` | `EOLO_PIN_UNUSED` |
| Alimentación motor | `MOTOR_POWER_PIN` | `EOLO_PIN_UNUSED` |

## Notas

- `GPIO34` y `GPIO35` son solo entrada.
- El ambiente `eolo_dron` declara `DRONE_SWITCHES_HAVE_EXTERNAL_PULLS`.
- Este target no usa Plantower, módem ni entrada ADC de batería.
