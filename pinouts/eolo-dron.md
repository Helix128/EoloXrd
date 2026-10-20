# Pinout EOLO Dron (base, hardware V2)

Fuente: `src/Variants/Pinout.h` con `EOLO_TARGET_DRON`.

**Ground truth:** mapa de la demo validada en hardware `demos/DronBase`. El
modelo anterior (headless, switches DIP, NeoPixel, NTC) es
[EOLO Dron Legacy](eolo-dron-legacy.md); **los roles de GPIO4/16/17 en RS485 son
distintos entre ambos** (ver abajo). Dron base pertenece a la familia UI:
OLED + encoder, una bomba, sin Plantower, módem ni anemómetro.

## Resumen

| Grupo | Señal | GPIO | Define firmware |
| --- | --- | --- | --- |
| I2C1 (OLED, RTC, BME280) | SDA | `GPIO21` | `SDA_PIN` |
| I2C1 (OLED, RTC, BME280) | SCL | `GPIO22` | `SCL_PIN` |
| I2C2 (encoder, `Wire1`) | SDA | `GPIO32` | `ENCODER_SDA_PIN` |
| I2C2 (encoder, `Wire1`) | SCL | `GPIO33` | `ENCODER_SCL_PIN` |
| microSD | CS | `GPIO5` | `SD_CS_PIN` |
| microSD | MOSI | `GPIO23` | `SD_MOSI_PIN` |
| microSD | MISO | `GPIO19` | `SD_MISO_PIN` |
| microSD | SCK | `GPIO18` | `SD_SCK_PIN` |
| Batería 2S | ADC | `GPIO35` | `BATTERY_ADC_PIN` |
| AFM07 RS485 | RO (RX) | `GPIO17` | `RS485_RX_PIN` |
| AFM07 RS485 | DI (TX) | `GPIO4` | `RS485_TX_PIN` |
| AFM07 RS485 | DE/RE | `GPIO16` | `RS485_DE_RE_PIN` |
| Bomba | PWM 0 | `GPIO26` | `MOTOR_PWM_PIN_0` |
| Bomba 2 | PWM 1 | `EOLO_PIN_UNUSED` | `MOTOR_PWM_PIN_1` |
| Plantower/PMS | RX/TX | `EOLO_PIN_UNUSED` | `PT_*` |
| Periféricos | Alimentación (N-FET) | `EOLO_PIN_UNUSED` | `PPH_PWR_PIN` |
| Switch espera | SW0/SW1 | `EOLO_PIN_UNUSED` | `WAIT_SW*_PIN` |
| Switch duración | SW0/SW1 | `EOLO_PIN_UNUSED` | `DURATION_SW*_PIN` |
| Módem | PWR/RX/TX | `EOLO_PIN_UNUSED` | `MODEM_*_PIN` |
| LED estado | NeoPixel | `EOLO_PIN_UNUSED` | `NEOPIXEL_PIN` |
| NTC motor | ADC | `EOLO_PIN_UNUSED` | `NTC_PIN` |

## Parámetros eléctricos y de protocolo

- **OLED:** SH1106 128x64 por I2C hardware (`U8G2_SH1106_128X64_NONAME_F_HW_I2C`),
  100 kHz. Con el driver SSD1306 aparece un borde blanco y texto cortado.
- **Encoder:** ATmega328 esclavo, dirección `0x08`, mismo protocolo de 3 bytes
  (dirección, contador, botón) que las demás variantes, pero en `Wire1` a
  **10 kHz** (`kEncoderBusClockHz`).
- **AFM07:** Modbus RTU, ID esclavo **2**, **4800 8N1**, registro `0x0000`,
  divisor `/100` (L/min). El comentario de la demo ("0x01, 9600, x10") está
  desactualizado respecto a su código.
- **Batería:** pack 18650 2S, divisor R1=220k / R2=100k (3.2:1). A 8.4 V el pin
  recibe 2.625 V. Se lee con `analogReadMilliVolts` y la curva de descarga 2S de
  `BatteryMath`.
- **Bomba:** N-FET low-side, PWM directo (duty 0 = apagado), 20 kHz. Requiere
  rampa: un salto brusco hunde el riel y resetea el ESP32 (brownout).

## Notas

- `GPIO35` es solo entrada (batería ADC).
- Dron Legacy usa RX=`GPIO16`, TX=`GPIO17`, DE/RE=`GPIO4`; no mezclar firmwares
  entre ambas placas.
- El ambiente `eolo_dron` habilita motor PWM directo, AFM07 y calibración de
  flujo (el control PID es exclusivo de Dron Legacy).
