# Hardware

Punto de entrada para quien arma, repara o valida una placa EOLO.
Reúne los pinouts por variante, el sensor AFM07, la calibración y los documentos de referencia.

## Variantes

| Variante | Pinout | Notas |
| --- | --- | --- |
| EOLO Mini Rev.2 (`eolo_dron`) | [eolo-dron.md](pinouts/eolo-dron.md) | OLED SH1106 por I2C, encoder en un bus I2C propio, una bomba, AFM07, batería 2S. Compila; sin validar en hardware |
| EOLO Mini Rev.1 (`eolo_dron_legacy`, `eolo_dron_legacy_low_power`) | [eolo-dron-legacy.md](pinouts/eolo-dron-legacy.md) | Sin pantalla. Switches de captura, NTC, NeoPixel, AFM07. Los roles de GPIO4/16/17 en el RS485 difieren de Mini Rev.2 |
| EOLO-MP (`eolo_standard`) | [eolo-standard.md](pinouts/eolo-standard.md) | Un solo RS485 compartido por AFM07 y anemómetro, módem, doble batería, Plantower |
| EOLO Express Rev.2 (`eolo_express`) | [eolo-express.md](pinouts/eolo-express.md) | AFM07, Plantower, batería simple. Tiene su propio bloque de pines |
| EOLO Express Rev.1 (`eolo_express_legacy`) | [eolo-express-legacy.md](pinouts/eolo-express-legacy.md) | Express con FS3000 en vez de AFM07. Orden de motores pendiente de validar |

El índice de pinouts y las notas comunes del ESP32 están en [pinouts/README.md](pinouts/README.md). Verifica que el firmware, las demos y estos documentos coincidan con:

```bash
python3 scripts/check_pinouts.py
```

## Documentos

- [AFM07: calificación y diagnóstico](afm07.md): conexión RS485, comandos de diagnóstico y criterios de aprobación.
- [Calibración](calibracion/README.md): curva del FS3000 y calibración de bombas.
- [Discrepancias de hardware pendientes](discrepancias-pines.md): valores del firmware que aún no se confirman en la placa.
- [Manual AT SIM7500/SIM7600](datasheets/SIM7500_SIM7600_AT_Manual_V3.00.pdf): comandos AT del módem celular de EOLO-MP.

## Pruebas aisladas

Las demos de [`demos/`](../../demos/README.md) prueban un componente a la vez (AFM07, BME280, Plantower, anemómetro, switches, pantalla, motores) sin cargar el firmware completo. Úsalas para aislar un fallo de hardware antes de buscarlo en el firmware.
