# Manual de usuario EOLO Dron

EOLO Dron es la version sin pantalla. La captura se configura con switches fisicos antes de energizar el equipo, o por Wi-Fi cuando los switches de espera estan en `Off`. Al terminar, el equipo entra en deep sleep y queda detenido hasta reiniciar o cortar y volver a alimentar.

## Preparacion

1. Cargue y conecte la bateria.
2. Inserte una tarjeta microSD funcional.
3. Revise que el sensor de flujo AFM07 y las bombas esten conectados.
4. Asegure una entrada y salida de aire libres, sin dobleces ni obstrucciones.
5. Configure los switches de espera y duracion antes de encender, o deje la espera en `Off` para usar el setup Wi-Fi headless.

Con switches fisicos, el flujo objetivo del Dron es **5.0 L/min**. En setup Wi-Fi headless puede configurarse entre **0.0 y 8.0 L/min** para la sesion actual. Este modelo no usa pantalla, modem, anemometro ni sensor Plantower.

## Pinout técnico

El pinout real usado por el firmware de EOLO Dron está documentado en [`../pinouts/eolo-dron.md`](../pinouts/eolo-dron.md).

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

Si la espera queda en `Off`, el equipo entra al setup Wi-Fi headless. Con espera distinta de `Off`, la duracion debe estar habilitada para iniciar una captura por switches. Si la espera no esta en `Off` y la duracion queda en `Off`, el equipo no inicia captura.

## Setup Wi-Fi headless

Cuando `WAIT_SW1` y `WAIT_SW0` estan en `0`, el Dron levanta el setup Wi-Fi headless. Esta feature es generica para equipos sin pantalla, pero hoy esta habilitada solo en EOLO Dron.

Defaults de firmware:

```text
SSID: eolo-dron
Password: eolo-dron
```

Pueden cambiarse por build flags `HEADLESS_SETUP_AP_SSID` y `HEADLESS_SETUP_AP_PASSWORD`.

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

## Calibracion automatica de motor

EOLO Dron puede calibrar la bomba en modo headless usando el motor PWM y el sensor de flujo AFM07. La calibracion barre PWM, espera estabilizacion, promedia lecturas validas y guarda una curva `PWM -> flujo` en la memoria interna (`eolo_calib`). Luego las capturas usan esa curva para partir cerca del flujo objetivo y el control closed-loop corrige fino.

### Desde setup Wi-Fi

1. Deje los switches de espera en `Off` y energice el equipo.
2. Conectese a `eolo-dron` y abra `http://eolo.setup` o `http://192.168.4.1`.
3. En **Calibracion automatica**, revise parametros.
4. Asegure entrada/salida de aire libres.
5. Pulse **Iniciar calibracion**.
6. Espere a que el estado indique `done` y revise el rango de flujo.

Parametros recomendados:

```text
PWM inicial: 400
PWM final: 1900
Paso PWM: 80
Estabilizacion: 3000 ms
Intervalo muestra: 800 ms
Muestras por punto: 5
Flujo maximo: 8.0 L/min
```

La escala PWM del firmware principal es 11-bit (`0-2047`) a 20 kHz en GPIO 26. La demo interactiva antigua usa escala 8-bit (`0-255`); para comparar valores use:

```text
PWM11 = round(PWM8 * 2047 / 255)
```

### Desde consola Serial

Entre a modo terminal enviando `!` y use:

```text
drone calib start
drone calib start pwmStart=400 pwmEnd=1900 pwmStep=80 settle=3000 sample=800 n=5 maxFlow=8.0
drone calib status
drone calib show
drone calib abort
drone calib clear
```

Tambien puede usar valores estilo demo con `scale=8`:

```text
drone calib start scale=8 pwmStart=50 pwmEnd=237 pwmStep=10
```

No inicie una captura mientras la calibracion esta corriendo. Si no hay calibracion valida, la captura puede iniciar pero el motor queda sin PWM util.

## Patrones LED

El EOLO Dron usa un NeoPixel de estado en GPIO 27. El codigo de color sigue esta semantica:

- Azul: sistema encendido o inicio.
- Violeta: configuracion/setup.
- Amarillo/ambar: espera o temporizacion.
- Verde: captura correcta.
- Cian: actividad interna/transitoria.
- Rojo: error.
- Blanco/gris: finalizacion.

| Estado | Modo normal / pruebas | Modo ahorro / produccion | Detalle |
| --- | --- | --- | --- |
| Apagado | LED apagado | LED apagado | Sin indicacion activa. |
| Inicio / idle | Azul pulsante | Pulso azul breve cada ~3 s | Equipo iniciando o sin captura activa. |
| Setup Wi-Fi | Violeta pulsante | Pulso violeta breve cada ~2.2 s | Punto de acceso y pagina web activos. |
| Esperando captura | Ambar parpadeante lento | Pulso ambar breve cada ~5 s | Hay una espera configurada antes de iniciar. |
| Captura activa | Verde pulsante | Pulso verde breve cada ~4 s | Bombas y registro de muestras activos. |
| Ocupado durante captura | Cian parpadeante rapido | Doble pulso cian breve cada ~2.5 s | Escritura de log o tarea interna en curso. |
| Error durante captura | Rojo parpadeante rapido | Triple pulso rojo frecuente | SD ausente o error de SD. Tiene prioridad sobre ocupado. |
| Captura finalizada | Blanco/gris fijo | Pulso blanco/gris breve y apagado antes de dormir | Sesion terminada; el equipo entra luego en deep sleep. |

El modo ahorro se habilita compilando con `STATUS_LED_LOW_POWER`, por ejemplo usando el entorno `eolo_dron_low_power`.

## Flujo de uso

1. Configure los switches de espera y duracion, o deje la espera en `Off` para usar Wi-Fi.
2. Energice el equipo.
3. El equipo lee los switches. Si la espera esta en `Off`, abre el setup Wi-Fi headless; si no, prepara una sesion con flujo de 5.0 L/min.
4. En setup Wi-Fi headless, conectese a `eolo-dron`, configure la sesion y confirme.
5. Si hay espera configurada, el equipo queda esperando hasta la hora de inicio. Si es instantanea, la captura comienza de inmediato.
6. Durante la captura, las bombas regulan el flujo y se registran muestras cada 10 segundos. El NeoPixel indica el estado segun la tabla de patrones LED.
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
time,flow,flow_target,temperature,humidity,pressure,pm1,pm25,pm10,ntc_temperature,battery_pct
```

`ntc_temperature` registra la temperatura del termistor interno si el firmware fue compilado con NTC; si no esta disponible se guarda `-1`.

En EOLO Dron no hay Plantower ni anemometro. Por eso no se registran columnas de viento, y las columnas PM no representan una medicion de particulas en este modelo.

## Solucion de problemas

| Sintoma | Revision recomendada |
| --- | --- |
| No inicia captura por switches | Verifique que la espera no este en `Off` y que la duracion este habilitada. |
| No veo la red Wi-Fi | Verifique que los dos switches de espera esten en `Off` al energizar. |
| No abre la pagina | Conectese a `eolo-dron` y abra `http://192.168.4.1/`. |
| No hay archivo CSV | Revise que la microSD este insertada y en buen estado. |
| `ntc_temperature` aparece como `-1` | Revise conexion del NTC o use firmware con `FEATURE_NTC`. |
| El flujo no alcanza el objetivo | Revise obstrucciones, mangueras, filtros, bombas, calibracion y conexion del AFM07. |
| La captura no termina | Puede estar configurada como duracion infinita. Reinicie o corte alimentacion para detener. |
| Termino y no responde | Es normal: entra en deep sleep. Reinicie o corte y vuelva a alimentar. |

## Mantenimiento basico

- Mantenga limpias las lineas de aire.
- Revise conectores antes de cada vuelo o montaje.
- Formatee o reemplace la microSD si aparecen fallas repetidas de escritura.
- Realice pruebas breves antes de una campaña larga.
