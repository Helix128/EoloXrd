# Pinout EOLO Standard

Fuente: `src/Board/Pinout.h` con `EOLO_TARGET_STANDARD`.

El OLED probado usa SPI de hardware sobre VSPI: SCK `GPIO18`, MISO `GPIO19`,
MOSI `GPIO23`, CS `GPIO27`, DC `GPIO15` y RESET `GPIO2`. El CS queda libre
porque el mapa de motores usa `GPIO14/25`; NeoPixel no está poblado.
La frecuencia SPI del perfil es 4 MHz, igual al valor por defecto de U8g2 para
este controlador.

## Resumen

| Grupo | Señal | GPIO | Define firmware |
| --- | --- | --- | --- |
| I2C | SDA | `GPIO21` | `SDA_PIN` |
| I2C | SCL | `GPIO22` | `SCL_PIN` |
| microSD | CS | `GPIO5` | `SD_CS_PIN` |
| microSD | MOSI | `GPIO23` | `SD_MOSI_PIN` |
| microSD | MISO | `GPIO19` | `SD_MISO_PIN` |
| microSD | SCK | `GPIO18` | `SD_SCK_PIN` |
| Switches headless | SW0..SW3 | `EOLO_PIN_UNUSED` | `WAIT_SW*_PIN`, `DURATION_SW*_PIN` |
| Batería | ADC | `GPIO34` | `BATTERY_ADC_PIN` |
| RS485 | RX | `GPIO35` | `RS485_RX_PIN` |
| RS485 | TX | `GPIO33` | `RS485_TX_PIN` |
| RS485 | DE/RE | `GPIO26` | `RS485_DE_RE_PIN` |
| Motor/bomba | PWM 0 | `GPIO14` | `MOTOR_PWM_PIN_0` |
| Motor/bomba | PWM 1 | `GPIO25` | `MOTOR_PWM_PIN_1` |
| Plantower | RX | `GPIO34` | `PT_RX` |
| Plantower | TX | `GPIO32` | `PT_TX` |
| Periféricos | Alimentación | `GPIO4` | `PPH_PWR_PIN` |
| Módem | PWR | `GPIO13` | `MODEM_PWR_PIN` |
| Módem | RX | `GPIO16` | `MODEM_RX_PIN` |
| Módem | TX | `GPIO17` | `MODEM_TX_PIN` |
| LED estado | NeoPixel | `EOLO_PIN_UNUSED` | `NEOPIXEL_PIN` |

## Funciones no usadas en este target

| Función | Define | Valor |
| --- | --- | --- |
| NTC motor | `NTC_PIN` | `EOLO_PIN_UNUSED` |
| Motor FG | `MOTOR_FG_PIN` | `EOLO_PIN_UNUSED` |
| Alimentación motor | `MOTOR_POWER_PIN` | `EOLO_PIN_UNUSED` |
| NeoPixel | `NEOPIXEL_PIN` | `EOLO_PIN_UNUSED` |

## Notas

- `GPIO34`, `GPIO35`, `GPIO36` y `GPIO39` son solo entrada.
- `GPIO27` queda dedicado a CS del OLED; NeoPixel está deshabilitado.
- Los motores usan `GPIO14/25` con PWM directo (`duty 0 = apagado`), por lo que no comparten GPIO con el OLED.
- Plantower mantiene `GPIO34/GPIO32`: `GPIO34` no se usa como ADC en Standard porque la batería es dual por I2C; `GPIO32` no se usa como switch en firmware no-headless.
- El firmware Standard habilita módem, anemómetro, doble batería, AFM07 y Plantower mediante flags de `platformio.ini`.
