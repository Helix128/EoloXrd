# Validacion de hardware pendiente

Este archivo congela las discrepancias que no se pueden resolver con una
compilacion. Ninguna se corrige automaticamente durante un refactor
estructural.

| Modelo | Decision pendiente | Valor preservado por firmware | Evidencia que falta |
| --- | --- | --- | --- |
| Express | CS/controlador OLED frente a RX RS485 en GPIO27 | SPI SSD1309 con CS GPIO27 y RX RS485 GPIO27 | Esquematico o prueba simultanea OLED/RS485 |
| Express | Escala AFM07 | divisor `100` | Ensayo con caudal conocido |
| Express Legacy | Orden fisico de motores | PWM0 GPIO25, PWM1 GPIO14 | Esquematico o prueba de bombas |
| Standard | Controlador OLED | SSD1306 | Identificacion del modulo fisico |

`scripts/check_pinouts.py` informa explícitamente la diferencia Legacy entre
la documentación histórica (`14/25`) y el firmware preservado (`25/14`). La
excepción es estrecha y verifica ambos valores: el chequeo falla si cualquiera
cambia hasta que exista evidencia de hardware para elegir uno de los dos. No
debe maquillarse cambiando la documentación o el pinout.
