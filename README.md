<p align="center">
  <img src="docs/assets/eolo-logo.png" alt="EOLO" width="420">
</p>

# EoloXrd

Firmware PlatformIO/ESP32 de la familia de equipos EOLO.

[![CI](https://github.com/Helix128/EoloXrd/actions/workflows/ci.yml/badge.svg)](https://github.com/Helix128/EoloXrd/actions/workflows/ci.yml)

## Qué es

EOLO es una familia de equipos de monitoreo de material particulado y calidad del aire. Los desarrolla el laboratorio C+ (UDD), el Centro de Investigación en Tecnologías para la Sociedad de la Universidad del Desarrollo.

Este repositorio reúne el firmware de todas las variantes, sus manuales, los pinouts y las herramientas de desarrollo.

## Variantes

| Variante | Env | Descripción |
| --- | --- | --- |
| EOLO Mini Rev.2 | `eolo_dron` | Muestreador compacto para montar en dron, con pantalla para configurarlo en terreno. |
| EOLO Mini Rev.1 | `eolo_dron_legacy`, `eolo_dron_legacy_low_power` | Primera revisión del Mini. No tiene pantalla: se configura con switches o desde el celular. |
| EOLO-MP Rev.1 | `eolo_standard` | Estación de monitoreo de material particulado que mide viento y envía los datos por red celular. |
| EOLO Express Rev.2 | `eolo_express` (env por defecto) | Equipo portátil y más simple que el MP, pensado para campañas de muestreo cortas. |
| EOLO Express Rev.1 | `eolo_express_legacy` | Primera revisión del Express, con otro sensor de flujo. |

## Documentación

- Usar un equipo: [manuales](docs/README.md#manuales) por variante.
- Obtener los datos: [formato de los CSV en la SD](docs/datos/README.md).
- Compilar y flashear: [compilación](docs/firmware/compilacion.md).
- Hardware y pinouts: [hardware](docs/hardware/README.md).
- Modificar el firmware: [guía de desarrollo](docs/firmware/desarrollo.md).

El [índice completo](docs/README.md) enlaza el resto de los documentos.

## Compilar y probar

```bash
pio run                        # compila eolo_express (env por defecto)
pio run -e eolo_dron -t upload # compila y flashea otra variante
pio test -e native             # pruebas portables de EoloCore, sin placa
scripts/check_all.sh           # compila todos los envs y corre las comprobaciones sin hardware
```

## Estructura

```text
src/        firmware: aplicaciones, interfaz, módem, variantes y código común
lib/        EoloCore, lógica portable sin dependencias de Arduino
include/    headers compartidos (Eolo/)
test/       pruebas Unity (PlatformIO) y pruebas Python de las herramientas
scripts/    herramientas de desarrollo y comprobaciones
demos/      sketches para probar sensores y componentes aislados
web-server/ página de setup Wi-Fi del Mini Rev.1
docs/       manuales, datos, firmware, hardware y archivo histórico
```

## Licencia

El código pertenece al laboratorio C+ (UDD). Licencia por definir.
