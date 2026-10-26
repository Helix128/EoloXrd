# Manual EOLO Mini Rev.2

EOLO Mini Rev.2 es un muestreador compacto para montar en un dron. Tiene pantalla OLED y encoder para configurar y seguir la captura en terreno, sin computador.

Registra flujo, volumen, temperatura, humedad, presión y el nivel de batería. No tiene sensor de material particulado, módem ni anemómetro.

## Antes de salir a terreno

1. Conecta la batería 2S.
2. Inserta una microSD. Sin tarjeta, el equipo muestra «Revisar SD» al arrancar y no guarda el archivo CSV.
3. Revisa mangueras, filtros y la entrada de aire.
4. Comprueba que el AFM07 esté conectado por RS485. Sin lectura de flujo, la pantalla muestra «--».
5. Enciende el equipo y espera el menú principal.
6. Revisa la hora en «Ajustar reloj». Las filas del CSV llevan la hora del RTC.

Esta revisión aún no se ha validado en terreno.

## Encender

Gira el encoder para moverte por los menús. Presiona el botón para confirmar.

Al encender, la pantalla muestra el logo y las etapas del arranque: «Revisando SD», «Estabilizando I2C», «Listo» y «Preparando equipo». Al final indica «SD lista», «SD sin tarjeta» o «SD con error». Después pasa al menú principal.

La barra superior muestra la hora, el estado de la SD y el nivel de batería.

## Menú principal

- «Nueva sesión»: programa una captura con hora de inicio y de fin.
- «Continuar sesión»: reanuda la sesión guardada. Aparece solo si existe una sesión guardada.
- «Captura rápida»: inicia de inmediato una captura de 1 hora.
- «Ajustar reloj»: cambia la hora del RTC.
- «Calibrar bombas»: ejecuta el barrido de calibración de la bomba.
- «Componentes»: muestra los sensores y permite probar el motor.

### Ajustar reloj

El menú muestra «Cambiar hora», «Sincronizar» y «Regresar». «Cambiar hora» ajusta hora y minuto; la fecha se conserva. «Sincronizar» no hace nada en este equipo, porque no tiene módem. Usa «Cambiar hora».

### Componentes

«Componentes» abre dos opciones: «Sensores» y «Motor». «Sensores» tiene tres páginas, Caudalímetro, BME280 y Batería, con su estado («OK», «Sin dato» u «Obsoleto») y sus valores. «Motor» gira el encoder para cambiar el PWM de la bomba. Presiona el botón para pasar a «Volver». Al salir, la bomba se apaga.

## Configurar y capturar

### Nueva sesión

1. Elige «Nueva sesión».
2. Gira el encoder para ajustar el «Flujo objetivo», entre 0.0 y 8.0 L/min. Cada paso suma o resta 0,1 L/min.
3. Confirma. Ajusta «Hora (inicio)». Por defecto propone un minuto después de la hora actual.
4. Confirma. Ajusta «Hora (fin)». La pantalla muestra la duración calculada.
5. La pantalla muestra «Inicio en», la cuenta regresiva, la hora de inicio y el flujo actual. Al llegar la hora de inicio comienza la captura.

Pulsar el botón mientras espera, con «Pulsar: volver», guarda la sesión y regresa al menú principal. Luego aparece «Continuar sesión» para retomarla.

### Captura rápida

Elige «Captura rápida», ajusta el «Flujo objetivo» y confirma. La captura empieza de inmediato y dura 1 hora.

### Calibrar bombas

1. Arma el circuito de aire como lo usarás en terreno.
2. Elige «Calibrar bombas». La pantalla muestra «Calibrando», la fase y el «Avance» con el tiempo «Restante».
3. Presiona el botón para pausar. La pantalla muestra «Calibración pausada» con «Continuar» y «Cancelar». «Cancelar» conserva la curva anterior y vuelve al menú principal.
4. Al terminar, presiona el botón para volver al menú principal. La pantalla no muestra si la curva fue aceptada. Si no cumple los mínimos, el firmware conserva la curva anterior.
5. Haz una captura corta de prueba antes de una campaña.

## Durante la captura

La pantalla principal muestra el «Flujo» en L/min y el «Vol.» acumulado en m³. Muestra «--» cuando no hay dato válido de flujo.

Gira el encoder para cambiar de página. Los puntos inferiores indican la página actual. Las páginas son:

- Temperatura, humedad y presión («Temp», «Hum» y «P»).
- «Flujo configurado».
- «Tiempo transcurrido» y «Tiempo restante», en HH:MM:SS.
- «Inicio» y «Fin», en HH:MM:SS.

Presiona el botón para abrir el menú de captura. Mientras está abierto, la captura queda en pausa. Al continuar, el tiempo en pausa se suma a la duración y la hora de fin se desplaza.

El menú de captura tiene estas opciones:

- «Continuar»: vuelve a la captura.
- «Finalizar»: termina la captura y apaga la bomba.
- «Probar bombas»: muestra «Potencia» (0 a 100 % con el encoder), el PWM del motor y el flujo. El botón apaga la bomba y vuelve al menú.
- «Ajustar hora fin»: cambia la hora de término.
- «Ajustar reloj»: cambia la hora del RTC y vuelve al menú de captura.
- «Ajustar flujo»: cambia el flujo objetivo. Al confirmar, vuelve a la captura.

Si la captura llega a la hora de fin, termina sola y muestra la misma pantalla que «Finalizar».

## Terminar y descargar los datos

Al terminar, la pantalla muestra «Fin de captura» con la hora de inicio, la de fin, el flujo y el «Vol. captura». Si la bomba dejó de mover aire, muestra «Falla bomba» (ver Problemas comunes). Presiona el botón para cerrar la sesión. El menú siguiente muestra «Ajustar reloj» y «Reiniciar EOLO». «Reiniciar EOLO» reinicia el equipo.

Finaliza la captura antes de retirar la microSD. Retírala, cópiala a un computador y conserva la carpeta `/EOLO` completa:

```text
/EOLO/logs/log_<fecha>.csv
/EOLO/log_index.csv
/EOLO/log_index.html
```

Copia `/EOLO` completo para que los enlaces de `log_index.html` sigan funcionando. Mini Rev.2 escribe estas columnas:

```text
time,flow,flow_target,captured_volume,temperature,humidity,pressure,battery_pct
```

El formato completo está en [`../datos/README.md`](../datos/README.md).

## Batería

El equipo usa una batería 2S de iones de litio. El icono de la cabecera muestra su porcentaje, calculado a partir de la tensión del pack. Muestra un guion cuando no hay dato.

Para ver la tensión, entra a «Componentes», luego «Sensores», y gira hasta la página «Batería». El campo «Voltaje» muestra el valor en V. El porcentaje se guarda en la columna `battery_pct` del CSV.

El firmware no tiene alarma ni corte por batería baja. Revisa el nivel en la cabecera antes de cada captura.

## Problemas comunes

| Síntoma | Causa en el firmware | Qué revisar |
| --- | --- | --- |
| «Revisar SD» al arrancar | La microSD falta o tiene error | Inserta o cambia la tarjeta. |
| Icono de SD con X | Sin tarjeta | Inserta la microSD. |
| Icono de SD con «!» | Error de la tarjeta | Revisa o cambia la tarjeta. |
| «Falla bomba» al terminar | Flujo cero con la bomba activa, confirmado en 5 muestras | Revisa la bomba, las mangueras, las fugas y el cableado del motor. |
| «--» en el flujo | Sin dato válido o dato obsoleto del AFM07 | Revisa el cable RS485 del AFM07. |
| «Sin dato» u «Obsoleto» en «Sensores» | Sensor sin lectura reciente | Revisa la conexión del sensor. El BME280 y el RTC van en el bus I2C principal. |
| «Sincronizar» no cambia nada | El equipo no tiene módem | Usa «Cambiar hora». |

## Más información

- Pinout y conexiones: [`../hardware/pinouts/eolo-dron.md`](../hardware/pinouts/eolo-dron.md)
- Sensor de flujo: [`../hardware/afm07.md`](../hardware/afm07.md)
- Formato de los datos: [`../datos/README.md`](../datos/README.md)
