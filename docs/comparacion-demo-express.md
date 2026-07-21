# Comparación histórica: demo secuencial y firmware EOLO Express

Este documento conserva las diferencias que motivaron la consolidación del
firmware Express. La configuración vigente se consulta en
`src/Config/Profiles/Express.h` y el mapa eléctrico autoritativo en
`src/Board/Pinout.h`; los valores de esta página no sustituyen esas fuentes.

## PWM y motor

| Aspecto | Demo secuencial | Firmware Express vigente |
| --- | --- | --- |
| Polaridad | Duty directo: `0` apaga la bomba | `EoloConfig::board.motorPwmInverted == false` (PWM directo) |
| Resolución | 8 bits (0..255) | 11 bits, definida por `motorPwmResolutionBits` |
| Frecuencia | 20 kHz | 20 kHz, definida por el driver de motor |
| Canales | 0 y 1 | 0 y 1 |
| GPIO | Depende de la revisión de la demo | Bomba 1 `GPIO32`, bomba 2 `GPIO33` |

`demos/MotorDirDiag` permite comprobar la polaridad sobre el hardware real sin
modificar el firmware. El driver usa el perfil tipado; no se añade un macro de
tuning a PlatformIO.

## Pantalla y bus I2C

El perfil Express usa `U8G2_SSD1309_128X64_NONAME2_F_4W_SW_SPI`. El ambiente
Standard usa la variante HW-SPI correspondiente a su perfil. El bus I2C común
queda en SDA `GPIO21`, SCL `GPIO22` y `150 kHz` para Express.

`I2CBus` serializa las operaciones con un mutex recursivo. BME280, RTC, batería,
entrada I2C y diagnóstico toman el mismo guard; el display se protege durante
la inicialización y el render. El clock ya no se cambia durante la animación de
la escena splash.

Para confirmar el controlador de un panel físico se mantiene
`demos/I2CDisplayDiag`, que prueba SSD1306 y SSD1309 en secuencia.

## Mapa Express vigente

| Señal | GPIO |
| --- | --- |
| I2C SDA/SCL | 21/22 |
| RS485 RO (RX) / DI (TX) / DE-RE | 27/25/26 |
| Plantower/PMS RX/TX del ESP32 | 17/16 |
| Alimentación PMS/periféricos | 4 |
| SD CS | 5 |
| Bombas PWM | 32/33 |
| Encoder I2C | 0x08 |

El firmware y las demos obtienen estos valores mediante
`demos/EoloDemoPinout.h`, que incluye directamente `src/Board/Pinout.h`.
`scripts/check_pinouts.py` comprueba la sincronización con la documentación de
`pinouts/`.

## Estado de verificación

Las pruebas eléctricas descritas originalmente siguen siendo diagnósticos
guiados, no resultados reproducibles desde el repositorio. Las demos
`AFM07ProfPins`, `AFM07Scan`, `MotorDirDiag` e `I2CDisplayDiag` permiten repetir
las comprobaciones en la placa. El pinout Express queda documentado como
`27/25/26` para RS485 y `32/33` para bombas; no se conserva la antigua
afirmación de que el firmware usaba `35/33` o `25/27`.

La limitación de auto-reset/bootloader de la placa continúa siendo un detalle
de operación: puede ser necesario mantener BOOT/IO0 durante “Connecting...” y
pulsar EN/RST.
