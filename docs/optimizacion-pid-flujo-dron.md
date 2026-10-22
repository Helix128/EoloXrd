# Optimización del control de flujo del Dron (PID)

Documento técnico del lazo de flujo de EOLO Dron V2 (`eolo_dron`) y de la variante legacy (`eolo_dron_legacy`). Describe el problema que motivó los cambios, las mediciones, la configuración del prototipo, los resultados en placa y lo que todavía no está verificado.

Estado: prototipo implementado y probado en el simulador de planta. Medido en placa sin filtro real. Los pendientes están en la sección 7.

Archivos involucrados:

- `lib/EoloCore/include/Eolo/Core/Flow/SmartFlowController.h`: CUSUM, zona muerta relativa, integral congelado, tope de PWM y flag `unreachable`.
- `lib/EoloCore/include/Eolo/Core/Flow/FlowMotorController.h`: campos nuevos de `FlowPidConfig` y su validación.
- `lib/EoloCore/include/Eolo/Core/Flow/DualMotorFlowController.h`: máximo del PWM virtual según `motorCount`.
- `src/Common/Data/MotorCaptureControl.h`: log `flow_unreachable`.
- `src/Variants/Profiles/Dron.h` y `src/Variants/Profiles/DronLegacy.h`: constantes por variante.

## 1. Problema

En estado CENTER_LOCKED la bomba queda fijada en un centro y el error se corrige sólo con trim e integral. Con la configuración anterior:

- El trim estaba limitado a ±80 cuentas (`softTrimMax`).
- La integral aportaba como máximo 16 cuentas (`iTerm = ki * integral`, con `ki` 0,8 e `integralLimit` 20 en `Dron.h`).
- El recentrado movía el centro 1 cuenta PWM cada 4 s (`recenterStep` 1, `recenterDelayMs` 4000).
- El breakout (re-búsqueda completa) sólo se disparaba con error mayor a 2 L/min sostenido 6 s.

Consecuencia: si un filtro o una restricción reducía la ganancia de la bomba, el trim no alcanzaba y el flujo quedaba bajo el objetivo para siempre. En placa, con una ganancia emulada de -15 %, el flujo se estabilizó entre -12 % y -13,5 % bajo la consigna.

Otros dos problemas:

- La zona muerta absoluta de 0,08 L/min equivale a 8 % a 1 L/min. El error se estabilizaba en -3,9 %.
- En placa de una bomba, el núcleo usaba un máximo virtual de 4094 (dos bombas). Todo comando por encima de 2047 se descartaba sin aviso y la integral seguía acumulando sin efecto (windup oculto).

## 2. Mediciones en Eolo Dron V2

### 2.1 Curva PWM a flujo

La tabla `kFlowFeedForward` de `Dron.h` (una bomba, 0 a 2047) tiene 10 puntos, de 184 a 2047 cuentas. En medición, la tabla quedó 3 a 6 % por debajo del flujo real. Puntos medidos en banco:

| PWM | Flujo medido (L/min) |
|---|---|
| 600 | 1,73 |
| 1000 | 3,31 |
| 1400 | 4,76 |
| 1800 | 6,04 |
| 2047 | 6,74 |

Pendiente por tramo, calculada a partir de estos puntos: 0,0040 L/min por cuenta entre 600 y 1000; 0,0036 entre 1000 y 1400; 0,0032 entre 1400 y 1800; 0,0028 entre 1800 y 2047. La ganancia baja al subir el PWM, así que un modelo lineal no sirve para todo el rango.

### 2.2 Muestreo y ruido

- El AFM07 entrega una muestra cada 360 ms (2,8 Hz) en Dron V2. El poll tiene un hueco de 250 ms; el sensor en sí refresca a 10 Hz o más. El cuello de botella está en el poll.
- Ruido: sigma entre 0 y 0,007 L/min. Resolución de lectura: 0,01 L/min.

### 2.3 Respuesta temporal

- Tiempo muerto: como máximo 1 muestra (360 ms).
- 63 % de la respuesta a 1,0-1,3 s; 95 % a 1,3-1,6 s.

## 3. Cambios

### 3.1 Máximo virtual y consigna inalcanzable

- El PWM virtual va de 0 a `2047 * motorCount`. Con `motorCount = 1` nunca se manda más de 2047.
- `unreachable` se activa cuando el mando queda en el máximo efectivo con error positivo sostenido durante 3 s (`kUnreachableConfirmMs`). Se libera al salir de esa condición.
- `MotorCaptureControl` escribe `flow_unreachable: ...` sólo en cada cambio de estado, no en cada ciclo.
- Caso de referencia: a 7 L/min (sobre el máximo de la bomba) el comando antes subía a 2280 sin aviso. Ahora se limita a 2047 y el flag se activa.

### 3.2 Zona muerta relativa, deriva e integral

- Zona muerta efectiva = max(`deadband`, `deadbandRel * consigna`). Con 0,012 y 0,005 en Dron: 0,012 L/min hasta 2,4 L/min y 0,025 L/min a 5 L/min.
- `driftThreshold`: error efectivo mínimo para contar deriva. Antes estaba fijo en 0,05.
- `freezeIntegral`: dentro de la zona muerta el integral se congela en vez de decaer 5 % por ciclo.

### 3.3 Detector CUSUM

El CUSUM acumula el error crudo (consigna menos flujo) descontando una holgura `cusumK`. Cuando la suma supera `cusumH`, hay alarma:

1. El trim actual se absorbe en el centro. El trim y la integral pasan a 0.
2. Se aplica un salto del centro igual a la pendiente de la tabla (cuentas por L/min en el punto de operación) por el error medio, multiplicado por `cusumJumpFrac` y acotado a ±`cusumJumpMax`.
3. El lazo entra en RECENTER y espera `cusumHoldMs` más el tiempo de rampa del salto antes de volver a acumular.

Amortiguamiento: si el signo de un salto es opuesto al del salto anterior, la fracción se multiplica por 0,7 (mínimo 0,3). Se restablece al cambiar la configuración o al reiniciar el controlador.

Pausa: si el flujo es menor a 0,2 L/min o menor al 10 % de la consigna, el CUSUM no acumula ni dispara. Evita llevar la bomba a PWM máximo con la línea tapada.

Apagado por defecto (`cusumH = 0`). Sólo Dron lo activa.

## 4. Constantes en Dron.h

| Constante | Valor | Significado |
|---|---|---|
| `motorCount` | 1 | Una bomba física; tope de PWM 2047 |
| `deadband` | 0,012 L/min | Zona muerta absoluta |
| `deadbandRel` | 0,005 | Fracción de la consigna para la zona muerta |
| `driftThreshold` | 0,01 L/min | Error mínimo para contar deriva |
| `freezeIntegral` | true | Congela el integral dentro de la zona muerta |
| `cusumK` | 0,006 L/min | Holgura del CUSUM |
| `cusumH` | 0,12 L/min acumulados | Umbral de alarma |
| `cusumJumpMax` | 300 cuentas | Salto máximo por alarma |
| `cusumJumpFrac` | 0,55 | Fracción del salto estimado que se aplica |
| `cusumHoldMs` | 2000 ms | Espera tras cada salto |

`DronLegacy.h` sólo cambia `motorCount = 1`. Express y Standard usan `motorCount = 2` y no activan el CUSUM.

## 5. Campos nuevos de FlowPidConfig

| Campo | Valor por defecto | Significado |
|---|---|---|
| `motorCount` | 2 | Bombas que reciben el PWM virtual (1 o 2) |
| `deadbandRel` | 0 | Zona muerta relativa a la consigna (0 a 0,5) |
| `driftThreshold` | 0,05 | Error efectivo mínimo para contar deriva |
| `freezeIntegral` | false | Congela el integral dentro de la zona muerta |
| `cusumK` | 0 | Holgura del CUSUM |
| `cusumH` | 0 | Umbral del CUSUM; 0 lo desactiva |
| `cusumJumpMax` | 0 | Salto máximo en cuentas; 0 usa 200 |
| `cusumJumpFrac` | 1,0 | Fracción del salto estimado (0,3 a 1,0) |
| `cusumHoldMs` | 0 | Espera tras un salto; 0 usa 2000 ms |

`validateConfig` rechaza `motorCount` fuera de 1..2, `cusumJumpFrac` fuera de 0,3..1,0, `cusumJumpMax` mayor que el PWM máximo y valores negativos o NaN.

## 6. Resultados en placa

Comparación entre la configuración anterior (baseline) y el prototipo:

| Caso | Baseline | Prototipo |
|---|---|---|
| 1 L/min, error estable | -3,9 % fijo | +0,3 a +0,8 % |
| Ganancia -15 %, 5 L/min | -12,2 % tras 69 s | ±1 % en 3,4 a 4,1 s |
| Ganancia -15 %, 3 L/min | -13,5 % tras 45 s | ±1 % en 6,1 s |
| 7 L/min (sobre máximo) | Comando a 2280 sin aviso | Tope 2047 y `unreachable` |
| Estabilidad 5 L/min, 230 s | No medida | 5,00 ± 0,02 L/min |
| Bloqueo inicial a 1 / 3 / 5 L/min | 2,8 / 5,3 / 7,5 s | Sin cambios |

Prueba con filtro real y el prototipo sin amortiguamiento (`cusumJumpFrac` 1,0): caída de -23 %, ±2 % a los 5 s, luego ciclo límite de ±2 % con periodo de unos 5 s y ±1 % recién a unos 57 s. Por eso se añadió `cusumJumpFrac` 0,55.

## 7. Pendiente y no verificado

- El valor 0,55 y la reducción x0,7 por cambio de signo no se han probado en placa con filtro real.
- El simulador de planta no modela el filtro. El test de amortiguamiento también pasa con `cusumJumpFrac` 1,0, así que no discrimina ese caso.
- Falta medir el baseline con filtro real para comparar.
- Riesgo: un bloqueo puede disparar 1 o 2 saltos mientras el flujo cae de 5 L/min a 0,2 L/min.
- Ideas no implementadas:
  - Acelerar el bloqueo inicial (hoy 2,8 a 7,5 s).
  - Kick sin rampa: un kick comandado de 800 llega en la práctica a unas 312 cuentas.
  - Bajar `EOLO_AFM07_POLL_GAP_MS` después de medir la respuesta busy 0x06.
  - Persistir el PID en NVS.
  - `sensorFaultStopMs` de Dron usa el valor por defecto de 6000 ms (`DronLegacy` usa 1500 ms).

## 8. Cómo probar

- Tests portables: `pio test -e native`. Requiere compilador en el host. Los seis tests nuevos son `test_flow_cusum_*`, `test_flow_relative_deadband_*`, `test_flow_single_motor_*`. En Windows sin gcc se validó compilando con `zig c++` contra Unity: 67 tests, 0 fallas.
- Compilación: `pio run -e eolo_dron` y `pio run -e eolo_dron_legacy`, además de las demás variantes que usan `FlowPidConfig`.
- Prueba en placa:
  1. Capturar a 5 L/min y esperar estabilidad.
  2. Unos 20 s después, meter el filtro.
  3. Unos 60 s después, sacarlo.
  4. Verificar que el flujo vuelva a ±1 %.
  5. Revisar el log `flow_unreachable` y que no aparezcan saltos repetidos.
