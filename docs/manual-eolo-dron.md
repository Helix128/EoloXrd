# Manual de usuario EOLO Dron

EOLO Dron es la version sin pantalla. La captura se configura con switches fisicos antes de energizar el equipo. Al terminar, el equipo entra en deep sleep y queda detenido hasta reiniciar o cortar y volver a alimentar.

## Preparacion

1. Cargue y conecte la bateria.
2. Inserte una tarjeta microSD funcional.
3. Revise que el sensor de flujo AFM07 y las bombas esten conectados.
4. Asegure una entrada y salida de aire libres, sin dobleces ni obstrucciones.
5. Configure los switches de espera y duracion antes de encender.

El flujo objetivo del Dron es fijo: **5.0 L/min**. Este modelo no usa pantalla, modem, anemometro ni sensor Plantower.

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

Para iniciar una captura, la espera y la duracion deben estar habilitadas. Si cualquiera de las dos queda en `Off`, el equipo no inicia captura.

## Flujo de uso

1. Configure los switches de espera y duracion.
2. Energice el equipo.
3. El equipo lee los switches y prepara una sesion con flujo fijo de 5.0 L/min.
4. Si la espera es de 5 o 15 minutos, el equipo queda esperando hasta la hora de inicio.
5. Si la espera es instantanea, la captura comienza de inmediato.
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
| No inicia captura | Verifique que espera y duracion no esten en `Off`. |
| No hay archivo CSV | Revise que la microSD este insertada y en buen estado. |
| El flujo no alcanza 5.0 L/min | Revise obstrucciones, mangueras, filtros, bombas y conexion del AFM07. |
| La captura no termina | Puede estar configurada como duracion infinita. Reinicie o corte alimentacion para detener. |
| Termino y no responde | Es normal: entra en deep sleep. Reinicie o corte y vuelva a alimentar. |

## Mantenimiento basico

- Mantenga limpias las lineas de aire.
- Revise conectores antes de cada vuelo o montaje.
- Formatee o reemplace la microSD si aparecen fallas repetidas de escritura.
- Realice pruebas breves antes de una campaña larga.
