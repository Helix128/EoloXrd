# Discrepancias de hardware pendientes

Lista de diferencias entre el firmware, la documentación y las demos que una compilación no puede resolver.
Sirve para saber qué valores del firmware faltan por confirmar en la placa física. Ninguna se corrige sin esa confirmación.

| Modelo | Decisión pendiente | Valor actual del firmware | Evidencia que falta |
| --- | --- | --- | --- |
| EOLO Express Rev.2 | Pines SPI heredados frente a RX RS485 en GPIO27 | SSD1309 I²C por defecto; SPI inactivo, CS GPIO27 coincide con RX RS485 GPIO27; seleccionar SPI falla al compilar | Esquemático o validación de otro cableado antes de habilitar SPI |
| EOLO Express Rev.1 (OLED I2C) | Orden físico de motores | PWM0 GPIO26, PWM1 GPIO27 (firmware original, nov 2025) | Prueba de bombas en placa |
| EOLO Express Rev.1 (subrevisión con OLED SPI) | Validación del cableado alternativo y orden físico de motores | OLED SSD1309 SPI, PWM0 GPIO25, PWM1 GPIO14; RS485 RX35/TX33/DE-RE26 | Inspección o prueba en la unidad con este cableado; la documentación histórica invierte PWM0/PWM1 |
| EOLO-MP | Controlador del OLED | SSD1306 | Identificación del módulo físico |

`scripts/check_pinouts.py` informa de forma explícita la diferencia de Express Rev.1 con OLED SPI entre la documentación histórica (`14/25`) y el firmware (`25/14`). Verifica ambos valores y falla si cualquiera cambia, hasta que una prueba en hardware permita elegir uno.

No cambies la documentación ni el pinout solo para que el chequeo pase. Los pinouts por variante están en [pinouts](pinouts/README.md).
