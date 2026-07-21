# Manuales de usuario EOLO

![EOLO](assets/eolo-logo.png)

Esta carpeta contiene los manuales de operacion para los modelos EOLO vigentes. Cada manual esta pensado para uso en terreno: preparacion, configuracion, captura, datos generados, mantenimiento basico y solucion de problemas.

La arquitectura de software y el roadmap de modularización se mantienen en
[architecture.md](architecture.md) y [eolo-modular-roadmap.md](eolo-modular-roadmap.md).

## Elegir manual

| Modelo | Manual | Pinout técnico | Caracteristicas principales |
| --- | --- | --- | --- |
| EOLO Dron | [Manual EOLO Dron](manual-eolo-dron.md) | [Pinout EOLO Dron](../pinouts/eolo-dron.md) | Sin pantalla, switches fisicos o setup Wi-Fi headless, captura automatica, flujo configurable por Wi-Fi, sin Plantower. |
| EOLO Standard | [Manual EOLO Standard](manual-eolo-standard.md) | [Pinout EOLO Standard](../pinouts/eolo-standard.md) | Pantalla y encoder, modem, anemometro, doble bateria/DC, AFM07 y Plantower. |
| EOLO Express | [Manual EOLO Express](manual-eolo-express.md) | [Pinout EOLO Express](../pinouts/eolo-express.md) | Pantalla y encoder, AFM07, Plantower, bateria simple, sin modem ni anemometro. |

## Diferencias rapidas

| Funcion | Dron | Standard | Express |
| --- | --- | --- | --- |
| Pantalla y encoder | No | Si | Si |
| Inicio de captura | Switches fisicos o Wi-Fi si espera esta en Off | Menu en pantalla | Menu en pantalla |
| Flujo objetivo | 5.0 L/min por switches; ajustable 0.0 a 8.0 L/min por Wi-Fi | Ajustable: 0.0 a 8.0 L/min | Ajustable: 0.0 a 8.0 L/min |
| Sensor de flujo | AFM07 | AFM07 | AFM07 |
| Sensor PM Plantower | No | Si | Si |
| Anemometro | No | Si | No |
| Modem | No | Si | No |
| Energia | Bateria | Doble bateria o DC | Bateria simple |

> EOLO Express Legacy no esta incluido en estos manuales. Su pinout técnico está disponible en [Pinout EOLO Express Legacy](../pinouts/eolo-express-legacy.md).

## Pinouts técnicos

La documentación de GPIO reales extraídos del firmware está centralizada en [`../pinouts/`](../pinouts/README.md).
