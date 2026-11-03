# Manual EOLO Express Rev.2

EOLO Express Rev.2 es el equipo con pantalla y encoder que captura aire y registra flujo (AFM07) y material particulado (Plantower). Usa una sola batería.

No tiene módem, anemómetro ni doble batería. Todo se maneja desde la pantalla con el encoder y su botón.

## Antes de salir a terreno

1. Conecta la batería cargada.
2. Inserta una microSD antes de encender. El equipo revisa la SD solo al arrancar.
3. Revisa mangueras, filtros, bombas y la entrada y salida de aire.
4. Enciende el equipo y espera el menú principal.
5. Entra en Componentes, luego en Sensores, y comprueba que Flujo y Plantower muestren OK.

## Controles

- Gira el encoder para cambiar valores o moverte por los menús.
- Presiona el botón del encoder para confirmar.

La barra superior muestra la hora, el estado de la SD y el porcentaje de batería (`--` sin dato; `!` en el ícono bajo 15 %). El ícono de la SD indica tarjeta disponible, escribiendo, con error (`!`) o sin tarjeta (X).

## Menú principal

| Opción | Uso |
| --- | --- |
| Nueva sesión | Programa una captura: flujo, hora de inicio y hora de fin. |
| Continuar sesión | Reanuda la sesión guardada. Aparece solo si existe. |
| Captura rápida | Inicia una captura inmediata de 1 hora. |
| Ajustar reloj | Cambia la hora a mano. |
| Control de flujo | Muestra el control automático y permite medir una curva opcional de arranque. |
| Componentes | Muestra los sensores y prueba cada motor. |

### Reloj

Revisa el reloj antes de una campaña larga. El nombre del archivo y la columna `time` dependen de esa hora.

Ajustar reloj ofrece Cambiar hora (hora y minuto), Sincronizar y Regresar. Express no tiene módem, así que Sincronizar no cambia la hora. Usa Cambiar hora.

## Capturar

### Nueva sesión

1. Elige Nueva sesión.
2. Ajusta el flujo objetivo entre 0.0 y 8.0 L/min. El encoder avanza en pasos de 0.1.
3. Confirma la hora de inicio. Si la hora ya pasó, el equipo usa el minuto siguiente.
4. Confirma la hora de fin. Debe ser posterior al inicio. La pantalla muestra la duración calculada.
5. El equipo queda en espera y muestra el flujo actual, la hora de inicio y el tiempo restante. Al llegar la hora de inicio comienza la captura. Al llegar la hora de fin termina y las bombas se apagan.

Durante la espera, el botón guarda la sesión y vuelve al menú principal.

### Captura rápida

1. Elige Captura rápida.
2. Ajusta el flujo objetivo entre 0.0 y 8.0 L/min.
3. Confirma. La captura empieza de inmediato y dura 1 hora.

### Durante la captura

La vista principal muestra el flujo actual y el volumen capturado. Girar el encoder cambia de vista, en este orden:

- Temperatura, humedad y presión.
- PM1.0 y PM2.5, y después PM10.
- Flujo configurado.
- Tiempo transcurrido y tiempo restante.
- Hora de inicio y hora de fin.

Presionar el botón abre el menú de captura. Mientras el menú está abierto, la captura queda en pausa, y el tiempo en pausa alarga la duración de la sesión.

| Opción | Uso |
| --- | --- |
| Continuar | Reanuda la captura. |
| Finalizar | Termina la captura y apaga las bombas. |
| Probar bombas | Cambia la potencia con el encoder (0 a 100 %) y muestra el flujo medido. Al salir, las bombas se apagan. |
| Ajustar hora fin | Cambia la hora de término. |
| Ajustar reloj | Cambia la hora del reloj. |
| Ajustar flujo | Cambia el flujo objetivo de la sesión. |

Al terminar, la pantalla «Fin de captura» muestra la hora de inicio, la de fin, el flujo y el volumen capturado. Si una bomba deja de dar flujo, la pantalla dice «Falla bomba». Presionar el botón abre el menú final con Ajustar reloj, Reiniciar EOLO y Regresar.

## Descargar los datos

Retira la microSD y cópiala a un computador. Copia la carpeta `/EOLO` completa para que los enlaces de `log_index.html` sigan funcionando.

```text
/EOLO/logs/log_<inicio>.csv   un archivo por captura
/EOLO/log_index.csv           índice de capturas
/EOLO/log_index.html          el mismo índice, con enlaces a cada CSV
```

El formato de los CSV, incluido el nombre de cada archivo y las columnas de cada variante, está en [`../datos/README.md`](../datos/README.md).

## Calibración

La pantalla Control de flujo informa que el control es automático. Medir una curva de bombas es opcional: puede ayudar al arranque después de cambiar bombas, mangueras o parte del circuito de aire, o cuando el flujo real no siga al objetivo. Este procedimiento también aplica a Express Rev.1.

1. Arma el circuito de aire como lo usarás en terreno y comprueba que haya una lectura de flujo.
2. Elige Control de flujo. En la pantalla «Control automatico», gira el encoder hasta «Medir» y confirma. «Volver» aparece seleccionado inicialmente; confirmar esa opción regresa al menú sin tocar la curva anterior.
3. Durante la medición, el equipo barre ambos motores y muestra la fase, el avance y el tiempo restante.
4. Para pausar, presiona el botón. La pantalla «Pausa segura» apaga las bombas y ofrece «Seguir» y «Salir»; gira el encoder para elegir y presiona para confirmar. «Salir» aparece seleccionado inicialmente y conserva la curva anterior.
5. Al terminar, la pantalla indica «Curva guardada» y el número de puntos, o «Error de curva» con el motivo. Si hay un error, la curva anterior permanece intacta.

La curva medida se guarda en la memoria Flash, no en la SD. El controlador la usa como punto de partida y sigue ajustando el caudal en lazo cerrado. Sin curva guardada, el control automático también regula el flujo; medirla no es requisito para capturar. Esta curva de bombas es distinta de la conversión del FS3000 a L/min: la pantalla no recalibra el sensor.

No obstruyas el flujo ni desconectes sensores o bombas durante la medición. Si aparece «Sin dato de flujo» o «Curva no guardada», revisa fugas, filtros, mangueras, conexiones y alimentación antes de intentarlo de nuevo. Tras cambiar componentes del circuito, valida el control con una prueba en el equipo antes de una campaña.

La curva debe cumplir los mínimos de puntos y rango de flujo para guardarse. Los detalles de ambas calibraciones están en [Calibración de flujo y bombas](../hardware/calibracion/README.md).

## Problemas comunes

Los comandos de la consola serie se escriben a 115200 baudios. Si falta la SD, las filas no se escriben. Si falta un sensor, su columna del CSV queda en `-1`.

| Síntoma | Qué revisar |
| --- | --- |
| Al encender aparece «Revisar SD» o el ícono de SD tiene X | No hay tarjeta o no se pudo abrir. Inserta la tarjeta y reinicia el equipo: la SD solo se revisa al arrancar. |
| El ícono de SD muestra `!` durante la captura | Error de escritura. El equipo no vuelve a intentar la SD hasta reiniciarse. Revisa la tarjeta y reinicia antes de la siguiente captura. |
| No hay CSV después de una captura | La SD faltaba o tenía error. La consola muestra «SD no disponible; se omite log local». |
| El flujo muestra `--` o «Sin dato» en Componentes > Sensores | El AFM07 no responde. Revisa cableado y alimentación. |
| El flujo es inestable | Revisa mangueras, filtros y fugas. Si cambió el circuito, valida el control con una prueba en el equipo; la curva opcional se mide en Control de flujo > Medir. |
| La captura termina con «Falla bomba» | El AFM07 marcó caudal cero con la bomba encendida, así que el equipo detuvo la captura. Revisa bombas, mangueras, filtros y alimentación. Prueba cada motor en Componentes > Motores antes de iniciar otra captura. |
| El PM muestra 0 o no cambia | El Plantower no responde. Sin lectura válida, la pantalla muestra 0 y el CSV guarda `-1`. Revisa su conexión y alimentación. |
| La batería muestra «Sin dato» | No hay lectura válida de la batería. Revisa la conexión. El CSV guarda `-1`. |
| La hora del archivo es incorrecta | Ajusta la hora con Ajustar reloj > Cambiar hora. Sincronizar no funciona en Express. |
| «Error de curva» | Lee el motivo en pantalla. «Sin dato de flujo» requiere revisar el sensor y sus conexiones; «Curva no guardada» puede indicar una medición que no cumple los mínimos o un fallo al guardar en Flash. La curva anterior se conserva. |

## Más información

- Pinout y conexiones: [`../hardware/pinouts/eolo-express.md`](../hardware/pinouts/eolo-express.md)
- Calibración y sensores de flujo: [`../hardware/calibracion/README.md`](../hardware/calibracion/README.md)
