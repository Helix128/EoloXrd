# Discrepancias de hardware pendientes

Lista de diferencias entre el firmware, la documentación y las demos que una compilación no puede resolver.
Sirve para saber qué valores del firmware faltan por confirmar en la placa física. Ninguna se corrige sin esa confirmación.

| Modelo | Decisión pendiente | Valor actual del firmware | Evidencia que falta |
| --- | --- | --- | --- |
| EOLO Express Rev.2 | CS del OLED frente a RX RS485 en GPIO27 | SPI SSD1309 con CS GPIO27 y RX RS485 GPIO27 | Esquemático o prueba simultánea OLED y RS485 |
| EOLO Express Rev.2 | Escala del AFM07 | divisor `100` (`Express.h`) | Ensayo con caudal conocido |
| EOLO Express Rev.1 (OLED I2C) | Orden físico de motores | PWM0 GPIO26, PWM1 GPIO27 (firmware original, nov 2025) | Prueba de bombas en placa |
| EOLO Express Rev.1 (OLED SPI) | Existencia de esta variante y orden de motores | OLED SSD1309 SPI, PWM0 GPIO25, PWM1 GPIO14 | Una unidad con OLED SPI; la unidad disponible usa OLED I2C |
| EOLO-MP | Controlador del OLED | SSD1306 | Identificación del módulo físico |

`scripts/check_pinouts.py` informa de forma explícita la diferencia de Express Rev.1 entre la documentación histórica (`14/25`) y el firmware (`25/14`). Verifica ambos valores y falla si cualquiera cambia, hasta que una prueba en hardware permita elegir uno.

No cambies la documentación ni el pinout solo para que el chequeo pase. Los pinouts por variante están en [pinouts](pinouts/README.md).
