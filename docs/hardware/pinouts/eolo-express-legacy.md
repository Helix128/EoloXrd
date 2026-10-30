# Pinout EOLO Express Rev.1

Fuente: `src/Variants/Pinout.h` con `EOLO_TARGET_EXPRESS_LEGACY`.

Este mapa sale del firmware original de noviembre 2025 que corre en el equipo físico: OLED SSD1306 por I2C (`0x3C`, 150 kHz) y bombas en `GPIO26/27`. Sus dos primeros datos se comprobaron en la placa (escaneo I2C) y en el binario respaldado (init SSD1306). La existencia de una subrevisión con cableado alternativo fue reportada; su mapa está pendiente de validación en placa y se documenta en [eolo-express-legacy-alt.md](eolo-express-legacy-alt.md).

EOLO Express Rev.1 comparte la mayor parte del mapa base con EOLO-MP y EOLO Express Rev.2. La diferencia funcional principal es el sensor de flujo FS3000 en vez de AFM07. No habilita módem, anemómetro, doble batería ni NeoPixel.

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
| RS485 | RX | `EOLO_PIN_UNUSED` | `RS485_RX_PIN` |
| RS485 | TX | `EOLO_PIN_UNUSED` | `RS485_TX_PIN` |
| RS485 | DE/RE | `EOLO_PIN_UNUSED` | `RS485_DE_RE_PIN` |
| Motor/bomba | PWM 0 | `GPIO26` | `MOTOR_PWM_PIN_0` |
| Motor/bomba | PWM 1 | `GPIO27` | `MOTOR_PWM_PIN_1` |
| Plantower | RX | `GPIO16` | `PT_RX` |
| Plantower | TX | `GPIO17` | `PT_TX` |
| Periféricos | Alimentación | `GPIO4` | `PPH_PWR_PIN` |
| Módem | PWR | `GPIO13` | `MODEM_PWR_PIN` |
| Módem | RX | `GPIO16` | `MODEM_RX_PIN` |
| Módem | TX | `GPIO17` | `MODEM_TX_PIN` |
| LED estado | NeoPixel | `EOLO_PIN_UNUSED` | `NEOPIXEL_PIN` |

## Diferencias funcionales

| Función | Estado en `eolo_express_legacy` |
| --- | --- |
| Sensor de flujo | FS3000 (`FEATURE_FLOW_FS3000`) |
| AFM07 | No habilitado |
| Plantower | Habilitado |
| Módem | No habilitado por flags de build |
| Anemómetro | No habilitado por flags de build |
| Doble batería | No habilitado por flags de build |
| NeoPixel | `EOLO_PIN_UNUSED` |
| Pantalla | OLED SSD1306 por I2C (`U8G2_SSD1306_128X64_NONAME_F_HW_I2C`), sin pines de display SPI |
| RS485 | No usado (FS3000 va por I2C); GPIO26 es una bomba |
| NTC motor | `EOLO_PIN_UNUSED` |
| Motor FG | `EOLO_PIN_UNUSED` |

## Pantalla SPI y subrevisión de cableado

La pantalla SSD1309 por SPI de software usa CS `GPIO27`, DC `GPIO15`, RESET `GPIO2`, SCK `GPIO18` y MOSI `GPIO23`; el descriptor declara 8 MHz, sin medición de velocidad física. El entorno `eolo_express_legacy_spi` activa el controlador con `EOLO_DISPLAY=SSD1309_SW_SPI` y selecciona también `EOLO_PINOUT=EXPRESS_REV1_ALT`, que cambia al cableado alternativo descrito en [su mapa completo](eolo-express-legacy-alt.md).

No basta con sobrescribir `EOLO_DISPLAY` en el pinout I2C original: allí `GPIO27` controla una bomba y chocaría con CS de la pantalla. `EOLO_PINOUT` solo se admite en el target `EOLO_TARGET_EXPRESS_LEGACY`.

## Notas

- Las bombas van en `GPIO26/27`, como en el firmware original. Los `GPIO25/14` del mapa SPI son de una revisión posterior (marzo 2026).
- Plantower usa `GPIO16/GPIO17`, pines de módem no usados por Express Rev.1.
- El orden de `PWM 0` y `PWM 1` no está confirmado en placa. Ver [Discrepancias](../discrepancias-pines.md).
