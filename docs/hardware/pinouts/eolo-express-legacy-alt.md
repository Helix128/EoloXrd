# Pinout EOLO Express Rev.1 (subrevisión con OLED SPI)

Subrevisión de cableado alternativo reportada para Express Rev.1, con OLED SSD1309 por SPI. Comparte el target y perfil funcional Express Legacy; selecciona este mapa mediante `EOLO_PINOUT=EXPRESS_REV1_ALT`. El mapa se conserva según el cableado reportado, pero aún no se ha validado en placa.

Fuente: `src/Variants/Pinout.h` con `EOLO_TARGET_EXPRESS_LEGACY` y `EOLO_PINOUT=EXPRESS_REV1_ALT`.

EOLO Express Rev.1 comparte la mayor parte del mapa base con EOLO-MP y EOLO Express Rev.2. La diferencia funcional principal es el sensor de flujo FS3000 en vez de AFM07. No habilita módem, anemómetro, doble batería ni NeoPixel.

## Resumen

La tabla conserva el orden histórico de las bombas (`14/25`). El firmware mantiene PWM0=`25`, PWM1=`14`; no se debe cablear por esta tabla sin resolver esa discrepancia en placa.

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
| Plantower | RX | `GPIO16` | `PT_RX` |
| Plantower | TX | `GPIO17` | `PT_TX` |
| Periféricos | Alimentación | `GPIO4` | `PPH_PWR_PIN` |
| Módem | PWR | `GPIO13` | `MODEM_PWR_PIN` |
| Módem | RX | `GPIO16` | `MODEM_RX_PIN` |
| Módem | TX | `GPIO17` | `MODEM_TX_PIN` |
| LED estado | NeoPixel | `EOLO_PIN_UNUSED` | `NEOPIXEL_PIN` |

## Diferencias funcionales

| Función | Estado en `eolo_express_legacy_spi` |
| --- | --- |
| Sensor de flujo | FS3000 (`FEATURE_FLOW_FS3000`) |
| AFM07 | No habilitado |
| Plantower | Habilitado |
| Módem | No habilitado por flags de build |
| Anemómetro | No habilitado por flags de build |
| Doble batería | No habilitado por flags de build |
| NeoPixel | `EOLO_PIN_UNUSED`; GPIO27 queda disponible para CS del OLED |
| NTC motor | `EOLO_PIN_UNUSED` |
| Motor FG | `EOLO_PIN_UNUSED` |

## Notas

- El firmware conserva `MOTOR_PWM_PIN_0=25` y `MOTOR_PWM_PIN_1=14`; la documentación histórica invierte el orden. El orden PWM0/PWM1 requiere validación física.
- El mapa reporta RS485 RX `GPIO35`, TX `GPIO33` y DE/RE `GPIO26`; el FS3000 no usa este bus. Su cableado físico está pendiente de validación.
- Plantower usa `GPIO16/GPIO17`, pines de módem no usados por Express Rev.1.
- El mapa SPI usa SCK/MOSI `GPIO18/23`, compartidos con microSD y operados con SPI de software. Ver [Discrepancias](../discrepancias-pines.md).
