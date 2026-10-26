# Datos registrados

Formato de los archivos CSV que EOLO escribe en la tarjeta microSD durante una captura.
Sirve para quien analiza los datos: dónde está cada archivo, qué columnas trae según la variante y cómo leer el índice de capturas.

## Estructura en la SD

```text
/EOLO/
├── log_index.csv       índice maestro de capturas
├── log_index.html      el mismo índice, legible en un navegador
└── logs/
    ├── log_2026_08_31T14_03_05.csv
    └── log_2026_08_31T14_03_05_schema2.csv
```

Cada captura genera un archivo `log_<inicio>.csv` en `/EOLO/logs/`. `<inicio>` es la hora de inicio de la sesión según el RTC, con el formato `YYYY_MM_DDTHH_MM_SS`.

Si un archivo con ese nombre ya existe pero su cabecera es distinta (por ejemplo, tras activar o desactivar el Plantower), el equipo no mezcla esquemas. Escribe en `log_<inicio>_schema2.csv`, luego `_schema3`, y así hasta `_schema99`.

## Filas

Hay una fila cada 10 segundos mientras la captura está activa. La primera línea es la cabecera y los valores van separados por coma, sin comillas.

Las columnas aparecen en este orden, y solo las que corresponden a la variante:

| Columna | Unidad | Origen | Presente en |
| --- | --- | --- | --- |
| `time` | ISO 8601 sin zona | RTC | todas |
| `state` | texto | control de flujo | Mini Rev.1 |
| `flow` | L/min | AFM07 o FS3000 | todas |
| `flow_target` | L/min | programa de la sesión | todas |
| `captured_volume` | L, 3 decimales | integral del flujo | todas |
| `temperature` | °C | BME280 | todas |
| `humidity` | % | BME280 | todas |
| `pressure` | hPa | BME280 | todas |
| `pm1`, `pm25`, `pm10` | µg/m³, enteros | Plantower, lectura atmosférica | Express Rev.2, Express Rev.1 y EOLO-MP, si el Plantower está habilitado en la sesión |
| `wind_speed` | m/s | anemómetro RS485, registro 0 dividido entre 100 | EOLO-MP |
| `wind_direction` | grados, entero sin escalar | anemómetro RS485, registro 1 | EOLO-MP |
| `ntc_temperature` | °C | termistor NTC | Mini Rev.1 |
| `battery_pct` | % | monitor de batería | todas menos Mini Rev.1 |

El código incluye cada grupo de columnas según los flags `FEATURE_FLOW_PID` (`state`), `FEATURE_PLANTOWER`, `FEATURE_ANEMOMETER`, `FEATURE_NTC` y, para la batería, la ausencia de `EOLO_TARGET_DRON_LEGACY`. Resultado por variante:

| Variante | Cabecera |
| --- | --- |
| EOLO Mini Rev.2 | `time,flow,flow_target,captured_volume,temperature,humidity,pressure,battery_pct` |
| EOLO Mini Rev.1 | `time,state,flow,flow_target,captured_volume,temperature,humidity,pressure,ntc_temperature` |
| EOLO Express Rev.2 y Express Rev.1 | `time,flow,flow_target,captured_volume,temperature,humidity,pressure,pm1,pm25,pm10,battery_pct` |
| EOLO-MP | `time,flow,flow_target,captured_volume,temperature,humidity,pressure,pm1,pm25,pm10,wind_speed,wind_direction,battery_pct` |

Sin Plantower habilitado en la sesión, Express Rev.2 y EOLO-MP omiten `pm1`, `pm25` y `pm10`.

Los valores `pm1`, `pm25` y `pm10` son la lectura atmosférica del Plantower, en µg/m³. `PlantowerParser` toma los bytes 10 a 15 de la trama, que son PM1.0, PM2.5 y PM10 atmosféricos, y no los valores de la calibración de fábrica (CF=1) de los bytes 4 a 9. El viento sale del anemómetro por RS485. `wind_speed` es el registro 0 dividido entre 100, en m/s. `wind_direction` es el registro 1 sin escalar, y la pantalla lo muestra en grados. El repositorio no documenta el rango ni la resolución de ese registro; confírmalo en la hoja de datos del anemómetro antes de usar la columna.

### Valores ausentes

Si un sensor no tiene una lectura válida, la columna lleva `-1`. Esto aplica a `flow`, a las tres columnas del BME280, a las del Plantower, a `wind_speed`, a `wind_direction` y a `ntc_temperature`. Filtra esos valores antes de promediar. `battery_pct` también vale `-1` si no hay lectura de batería.

### Tiempo

`time` sale del RTC y se escribe con el formato `YYYY-MM-DDTHH:MM:SS`, por ejemplo `2026-08-31T14:03:05`. No incluye desfase horario: es la hora local que tenía el RTC.

El firmware guarda en el RTC la hora local, no UTC. Al sincronizar por HTTP, `RtcTimeParser` suma al `unix` del servidor el `utc_offset` que él mismo entrega, y graba el resultado. El comando `time epoch <unix_utc> <offset_seconds>` hace lo mismo desde la consola; `scripts/send_host_time.py` lo envía con la hora y el desfase del computador. Si ajustas el reloj a mano, vale la hora que ingreses. Revisa la hora del equipo antes de una campaña; el código no garantiza que el RTC esté correcto.

### Estado y fila final

En Mini Rev.1, `state` toma tres valores:

- `Arrancando`: el control de flujo está en la fase de arranque de la bomba.
- `Capturando`: operación normal.
- `Finalizado`: fila de cierre de la captura.

Todas las variantes escriben una fila de cierre al terminar la captura (cierre manual o por tiempo), con la hora real del RTC y el volumen final. Solo Mini Rev.1 la marca con `Finalizado`, porque es la única con columna `state`. En el resto, identifica el final por ser la última fila del archivo.

Si el equipo se reinicia o pierde energía durante la captura, el archivo queda sin fila de cierre. El índice infiere entonces el término desde la última fila válida, que puede ser anterior al término real.

## Índice maestro

`/EOLO/log_index.csv` lista todas las capturas, ordenadas por fecha y hora de inicio. Tiene esta cabecera:

```text
log_file,start_date,start_time,end_date,end_time,captured_volume_l,volume_source
```

`start_*` y `end_*` usan `YYYY-MM-DD` y `HH:MM:SS`. `captured_volume_l` está en litros con tres decimales y queda vacío si no hay volumen disponible. `volume_source` indica de dónde viene el volumen:

- `recorded`: último valor de `captured_volume` del archivo.
- `estimated_flow`: el archivo no tiene `captured_volume`; el volumen se estima sumando `flow` multiplicado por 10 s.
- `unavailable`: no se pudo obtener.

Una captura entra al índice al escribirse su fila final. Si el índice falta o está corrupto, el equipo lo reconstruye al iniciar leyendo todos los `log_*.csv` de `/EOLO/logs/`. La reconstrucción toma la hora de inicio del nombre del archivo y la de término de la última fila con marca de tiempo válida. `log_index.html` presenta la misma tabla con enlaces a cada CSV.

## Descargar los datos

En EOLO Mini Rev.1, usa la página de setup Wi-Fi. Conéctate a la red del equipo y abre `http://192.168.4.1/` (los detalles están en el [manual del Mini Rev.1](../manuales/mini-rev1.md)). La página lista, previsualiza, descarga y borra los CSV. Por HTTP el servidor expone:

```text
GET /api/logs                        lista de archivos en /EOLO/logs
GET /api/logs/preview?file=NOMBRE    vista previa de un CSV
GET /download?file=NOMBRE            descarga de un CSV
GET /download?index                  descarga de log_index.csv
```

Para el resto de variantes, retira la microSD y copia la carpeta `/EOLO` completa. Así los enlaces relativos de `log_index.html` siguen funcionando.

EOLO-MP puede además subir datos por módem. El formato de esa subida no se describe aquí. La SD es la referencia de los datos.

## Calibración

Los valores de `flow` dependen de la calibración del sensor de flujo. Los procedimientos y los resultados están en [Calibración](../hardware/calibracion/README.md).
