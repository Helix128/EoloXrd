# Manual EOLO Mini Rev.1

EOLO Mini Rev.1 es el equipo sin pantalla que captura aire con una bomba y mide el caudal con un sensor AFM07. Sirve para campañas de muestreo en terreno: captura por su cuenta y guarda los datos en una microSD.

Al terminar la captura, el equipo entra en deep sleep. Queda detenido hasta que lo reinicies o cortes y vuelvas a dar alimentación. No tiene pantalla, módem, anemómetro ni sensor Plantower.

## Antes de salir a terreno

1. Carga la batería y conéctala.
2. Inserta una tarjeta microSD en buen estado.
3. Revisa que el sensor AFM07 y la bomba estén conectados.
4. Comprueba que la entrada y la salida de aire estén libres, sin dobleces ni obstrucciones.
5. Haz una captura corta de prueba antes de una campaña larga.

Con switches, el caudal objetivo es siempre 5.0 L/min. Por Wi-Fi puedes fijar un caudal entre 0.0 y 8.0 L/min. Ese valor se guarda como predeterminado de la página y no cambia lo que hacen los switches.

## Encender y configurar

Los switches se leen una sola vez, al encender el equipo. Muévelos antes de dar alimentación. Hay dos grupos de dos switches: espera y duración. Un switch en On conecta su pin a masa y se lee como 1. El cableado completo está en [`../hardware/pinouts/eolo-dron-legacy.md`](../hardware/pinouts/eolo-dron-legacy.md).

| `WAIT_SW1` | `WAIT_SW0` | Espera antes de capturar |
| --- | --- | --- |
| 0 | 0 | Off (abre el setup Wi-Fi) |
| 0 | 1 | 1 min |
| 1 | 0 | 5 min |
| 1 | 1 | Instantánea |

| `DURATION_SW1` | `DURATION_SW0` | Duración de la captura |
| --- | --- | --- |
| 0 | 0 | Off |
| 0 | 1 | 5 min |
| 1 | 0 | 15 min |
| 1 | 1 | Infinita |

Con la espera en Off, el equipo abre el setup Wi-Fi e ignora los switches de duración. Con la espera activa y la duración en Off, el equipo permanece encendido sin capturar.

### Configurar por Wi-Fi

Con los dos switches de espera en `0`, el equipo crea su propia red Wi-Fi:

```text
Red:        eolo-dron
Contraseña: eolo-dron
```

Esa contraseña es el valor público por defecto del firmware. Cámbiala antes de entregar el equipo. Para eso, define `HEADLESS_SETUP_AP_PASSWORD` (y, si quieres, `HEADLESS_SETUP_AP_SSID`) en `src/Headless/HeadlessSecrets.h` antes de compilar; la plantilla `HeadlessSecrets.example.h` muestra cómo. WPA2 exige entre 8 y 63 caracteres.

Conecta el teléfono o el notebook a esa red y abre en el navegador:

```text
http://eolo.setup
http://192.168.4.1
```

Usa la segunda dirección si la primera no abre. Escribe `http`, no `https`: el equipo solo sirve HTTP.

La página tiene cuatro pestañas: Captura, Presets, Registros SD y Diagnóstico.

- **Captura**: espera antes de iniciar (0 s, 1 min, 5 min, 15 min o un tiempo manual en minutos y segundos), tipo de duración (tiempo definido o sin límite) y modo de caudal. El modo constante fija un caudal de 0.0 a 8.0 L/min. El modo bloques define hasta 8 tramos de caudal; la suma de sus duraciones no puede superar la duración total.
- **Presets**: guarda la configuración vigente con un nombre de hasta 23 caracteres y carga una guardada. Se pueden guardar hasta 8.
- **Registros SD**: lista, previsualiza, descarga y borra los archivos de datos (ver [Descargar los datos](#descargar-los-datos)).
- **Diagnóstico**: prueba manual del motor (ver [Calibración](#calibración)).

Confirma con «Iniciar sesión de captura». Al confirmar, el Wi-Fi se apaga, la página guarda la configuración como predeterminada y la captura empieza o queda en espera según la espera elegida. Cada arranque con espera en Off vuelve a abrir el setup: el equipo no inicia por sí solo una captura guardada.

Si el firmware tiene configurada una red local, el setup también se conecta a ella y queda disponible como `http://eolo-dron.local/`. Esa red se define en `HeadlessSecrets.h` o con flags `-D` (ver [`../firmware/desarrollo.md`](../firmware/desarrollo.md)). Es configuración de desarrollo; en operación no hace falta.

## Capturar

1. Configura los switches, o deja la espera en Off para usar Wi-Fi.
2. Da alimentación al equipo.
3. Si usas Wi-Fi, conéctate, configura y confirma.
4. Si hay espera, el equipo aguarda hasta la hora de inicio. Al llegar, comprueba que el AFM07 entregue una muestra válida y reintenta cada segundo si no la tiene.
5. Durante la captura, la bomba regula el caudal y el equipo registra una muestra cada 10 segundos.
6. Al cumplirse la duración, la bomba se apaga y el equipo entra en deep sleep.

Con duración infinita, la captura sigue hasta que reinicies el equipo o cortes la alimentación.

### Luz de estado

El LED de estado repite un ciclo de 4 segundos:

| Segundos del ciclo | Qué muestra |
| --- | --- |
| 0–2 | Apagado |
| 2–3 | Estado, según el color |
| 3–4 | Temperatura del motor |

Colores del estado:

- Azul: encendido o en reposo, sin captura.
- Violeta: setup Wi-Fi o diagnóstico.
- Ámbar: espera antes de iniciar.
- Verde: captura en curso.
- Cian: escritura en la SD o registro activo.
- Naranja (3 destellos): motor sobrecalentado durante la captura.
- Rojo: durante la captura, SD ausente o con error, o bloqueo de seguridad del motor. Si la captura termina con error, el rojo queda fijo hasta el deep sleep.
- Blanco o gris: captura terminada.

Colores de temperatura del motor: verde hasta 30 °C, verde amarillento de 30 a 40 °C, amarillo de 40 a 50 °C, naranja de 50 a 60 °C y rojo desde 60 °C. Sobre 65 °C, el rojo parpadea rápido. Si el equipo no puede leer la temperatura, ese segundo se muestra violeta, y también durante el setup Wi-Fi.

### Variante de bajo consumo

`eolo_dron_legacy_low_power` compila el mismo firmware con el flag `STATUS_LED_LOW_POWER`. El único cambio está en el LED de estado: colores de menor intensidad y destellos más espaciados. Tras una captura exitosa, el LED se apaga antes del deep sleep; la versión normal lo deja en gris fijo. Con error, el LED queda en rojo en ambas versiones. La captura, los switches y el deep sleep son iguales. Este manual no incluye mediciones de consumo.

## Descargar los datos

Retira la microSD y cópiala a un computador. También puedes listar, previsualizar, descargar y borrar los archivos desde la pestaña Registros SD, con el Wi-Fi activo. Los archivos quedan así:

```text
/EOLO/logs/log_<fecha>.csv
/EOLO/log_index.csv
/EOLO/log_index.html
```

Copia la carpeta `/EOLO` completa para que los enlaces de `log_index.html` sigan funcionando. Si borras un archivo desde la página, el índice se actualiza. Si lo borras desde un computador, el índice queda desactualizado hasta la siguiente reconciliación, que ocurre al iniciar la SD, o hasta ejecutar `logs index rebuild` por la consola serie.

El formato de las columnas está en [`../datos/README.md`](../datos/README.md).

## Problemas comunes

| Síntoma | Qué revisar |
| --- | --- |
| No inicia captura con switches | La espera no debe estar en Off y la duración no debe estar en Off. |
| No aparece la red Wi-Fi | Pon los dos switches de espera en `0` antes de dar alimentación. |
| No abre la página | Conéctate a `eolo-dron` y abre `http://192.168.4.1`, con `http` y no `https`. |
| No hay archivo CSV | Revisa que la microSD esté insertada y en buen estado. |
| El flujo no llega al objetivo | Revisa obstrucciones, mangueras, filtros, bomba y conexión del AFM07. |
| LED rojo fijo tras la captura | La captura terminó con error. Revisa el motivo en la consola serie (ver abajo). |
| `ntc_temperature` aparece como `-1` | Revisa la conexión del termistor NTC del motor. |
| La captura no termina | Puede estar en duración infinita. Reinicia o corta la alimentación. |
| El equipo no responde al terminar | Es normal: está en deep sleep. Reinicia o vuelve a dar alimentación. |

Cuando una captura se aborta, la consola serie muestra `Drone: captura abortada por fallo (<motivo>)`. Los motivos posibles son `afm_invalid`, `afm_stale`, `afm_diagnostic_blocked`, `sd_unavailable`, `ntc_invalid`, `pump_failure_zero_flow` y `safety`. Revisa el cableado del componente que corresponda, repite una captura corta y, si persiste, consulta [`../firmware/compilacion.md`](../firmware/compilacion.md) y [`../hardware/afm07.md`](../hardware/afm07.md).

## Consola serie

Con el equipo conectado por USB, abre la consola a 115200 baudios:

```bash
pio device monitor -b 115200   # Consola serie del Mini Rev.1
```

`drone status` muestra el estado compacto. `drone status -v` añade los valores crudos. `drone help` lista el resto de comandos, y `logs index rebuild` reconstruye el índice de capturas.

## Calibración

El Mini Rev.1 no requiere calibración manual antes de capturar. El firmware regula el caudal en lazo cerrado con el sensor AFM07, y el setup no incluye un procedimiento de calibración del motor.

La pestaña Diagnóstico permite probar el motor a mano. Al activar el diagnóstico, se suspende la regulación de caudal y puedes fijar la potencia PWM, detener el motor o dar un pulso de arranque. El firmware no sale de este modo desde la página; reinicia el equipo para volver a la operación normal.

## Más información

- Pinout y conexiones: [`../hardware/pinouts/eolo-dron-legacy.md`](../hardware/pinouts/eolo-dron-legacy.md)
- Sensor AFM07: [`../hardware/afm07.md`](../hardware/afm07.md)
- Compilación y diagnóstico: [`../firmware/compilacion.md`](../firmware/compilacion.md)
