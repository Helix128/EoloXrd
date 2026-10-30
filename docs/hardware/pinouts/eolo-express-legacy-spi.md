# Pinout EOLO Express Rev.1 (OLED SPI)

Variante con OLED SSD1309 por SPI. **Sin verificar en hardware**: la unidad Rev.1 disponible usa OLED I2C ([eolo-express-legacy.md](eolo-express-legacy.md)).

Fuente: `src/Variants/Pinout.h` con `EOLO_TARGET_EXPRESS_LEGACY_SPI`.

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

- El mapa de motores `GPIO14/25` deja libre `GPIO27` para el CS del OLED SPI.
- Plantower usa `GPIO16/GPIO17`, pines de módem no usados por Express Rev.1.
- El orden de `PWM 0` y `PWM 1` no está confirmado en placa. La tabla sigue la documentación histórica; el firmware usa `MOTOR_PWM_PIN_0 25` y `MOTOR_PWM_PIN_1 14`. Ver [Discrepancias](../discrepancias-pines.md).
