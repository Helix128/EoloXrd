# Manual de usuario EOLO Dron

EOLO Dron es la version sin pantalla. La captura se configura con switches fisicos antes de energizar el equipo, o por Wi-Fi cuando los switches de espera estan en `Off`. Al terminar, el equipo entra en deep sleep y queda detenido hasta reiniciar o cortar y volver a alimentar.

## Preparacion

1. Cargue y conecte la bateria.
2. Inserte una tarjeta microSD funcional.
3. Revise que el sensor de flujo AFM07 y las bombas esten conectados.
4. Asegure una entrada y salida de aire libres, sin dobleces ni obstrucciones.
5. Configure los switches de espera y duracion antes de encender, o deje la espera en `Off` para usar el setup Wi-Fi.

Con switches fisicos, el flujo objetivo del Dron es **5.0 L/min**. En setup Wi-Fi puede configurarse entre **0.0 y 8.0 L/min** para la sesion actual. Este modelo no usa pantalla, modem, anemometro ni sensor Plantower.

## Switches

Los switches se leen al encender. Cada grupo usa dos posiciones: `SW0` y `SW1`.

### Espera antes de capturar

| `WAIT_SW1` | `WAIT_SW0` | Espera |
| --- | --- | --- |
| 0 | 0 | Off |
| 0 | 1 | 5 min |
| 1 | 0 | 15 min |
| 1 | 1 | Instantanea |

### Duracion de captura

| `DURATION_SW1` | `DURATION_SW0` | Duracion |
| --- | --- | --- |
| 0 | 0 | Off |
| 0 | 1 | 5 min |
| 1 | 0 | 15 min |
| 1 | 1 | Infinita |

Si la espera queda en `Off`, el equipo entra al setup Wi-Fi. Con espera distinta de `Off`, la duracion debe estar habilitada para iniciar una captura por switches. Si la espera no esta en `Off` y la duracion queda en `Off`, el equipo no inicia captura.

## Setup Wi-Fi

Cuando `WAIT_SW1` y `WAIT_SW0` estan en `0`, el Dron levanta un punto de acceso:

```text
SSID: eolo-setup
Password: eolo-setup
```

Conecte un telefono o notebook a esa red y abra:

```text
http://eolo.setup
```

o en su defecto:
```text
http://192.168.4.1 
```



OJO: ES IMPORTANTE QUE SEA "http" Y NO "https"!!

Desde la pagina puede configurar:

- Espera antes de capturar: instantanea, 5 min, 15 min o segundos manuales.
- Duracion: 5 min, 15 min, segundos manuales o infinita.
- Flujo objetivo: 0.0 a 8.0 L/min.
- Revision de estado de SD, hora RTC, switches y rango de calibracion disponible.
- Listado, preview y descarga de archivos CSV en `/EOLO/logs`.

Al confirmar, el Wi-Fi se apaga, se guardan esos valores como defaults del formulario para la proxima vez y comienza la sesion configurada. La configuracion web no auto-inicia una captura futura: cada arranque con espera en `Off` requiere confirmar desde la pagina.

## Flujo de uso

1. Configure los switches de espera y duracion, o deje la espera en `Off` para usar Wi-Fi.
2. Energice el equipo.
3. El equipo lee los switches. Si la espera esta en `Off`, abre el setup Wi-Fi; si no, prepara una sesion con flujo de 5.0 L/min.
4. En setup Wi-Fi, conectese a `EOLO-Dron-Setup`, configure la sesion y confirme.
5. Si hay espera configurada, el equipo queda esperando hasta la hora de inicio. Si es instantanea, la captura comienza de inmediato.
6. Durante la captura, las bombas regulan el flujo y se registran muestras cada 10 segundos.
7. Al cumplir la duracion configurada, las bombas se apagan.
8. El equipo entra en deep sleep hasta reset o power-cycle.

Si se selecciona duracion infinita, la captura continua hasta que el equipo sea reiniciado o se corte la alimentacion.

## Datos generados

Los datos se guardan en la microSD:

```text
/EOLO/logs/log_<fecha>.csv
```

El intervalo de registro es de 10 segundos. El CSV del Dron usa estas columnas:

```text
time,flow,flow_target,temperature,humidity,pressure,pm1,pm25,pm10,battery_pct
```

En EOLO Dron no hay Plantower ni anemometro. Por eso no se registran columnas de viento, y las columnas PM no representan una medicion de particulas en este modelo.

## Solucion de problemas

| Sintoma | Revision recomendada |
| --- | --- |
| No inicia captura por switches | Verifique que la espera no este en `Off` y que la duracion este habilitada. |
| No veo la red Wi-Fi | Verifique que los dos switches de espera esten en `Off` al energizar. |
| No abre la pagina | Conectese a `EOLO-Dron-Setup` y abra `http://192.168.4.1/`. |
| No hay archivo CSV | Revise que la microSD este insertada y en buen estado. |
| El flujo no alcanza el objetivo | Revise obstrucciones, mangueras, filtros, bombas, calibracion y conexion del AFM07. |
| La captura no termina | Puede estar configurada como duracion infinita. Reinicie o corte alimentacion para detener. |
| Termino y no responde | Es normal: entra en deep sleep. Reinicie o corte y vuelva a alimentar. |

## Mantenimiento basico

- Mantenga limpias las lineas de aire.
- Revise conectores antes de cada vuelo o montaje.
- Formatee o reemplace la microSD si aparecen fallas repetidas de escritura.
- Realice pruebas breves antes de una campaña larga.
