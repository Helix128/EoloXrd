# Control de flujo de EOLO Mini Rev.2 (PID)

Problema: con un filtro o una restricción en la línea, el flujo quedaba entre 12 y 13,5 % bajo la consigna, y en 1 L/min el error se estabilizaba en -3,9 %.
Solución: zona muerta relativa, tope de PWM con aviso `unreachable`, integral congelado cuando el comando se satura y una ley suave (`smoothLaw`) con detector CUSUM amortiguado.
Estado: implementado y medido en placa. La ley suave no tiene prueba con filtro real (sección 7).

Este documento describe el lazo de flujo de EOLO Mini Rev.2 (`eolo_dron`) y de EOLO Mini Rev.1 (`eolo_dron_legacy`): el problema, las mediciones, la configuración, los resultados en placa y las limitaciones conocidas.

## 1. Problema

En estado CENTER_LOCKED la bomba queda fijada en un centro y el error se corrige solo con trim e integral. Con la configuración anterior:

- El trim estaba limitado a ±80 cuentas (`softTrimMax`).
- La integral aportaba como máximo 16 cuentas (`iTerm = ki * integral`, con `ki` 0,8 e `integralLimit` 20 en `Dron.h`).
- El recentrado movía el centro 1 cuenta PWM cada 4 s (`recenterStep` 1, `recenterDelayMs` 4000).
- El breakout (re-búsqueda completa) solo se disparaba con error mayor a 2 L/min sostenido 6 s.

Consecuencia: si un filtro o una restricción reducía la ganancia de la bomba, el trim no alcanzaba y el flujo quedaba bajo el objetivo para siempre. En placa, con una ganancia emulada de -15 %, el flujo se estabilizó entre -12 % y -13,5 % bajo la consigna.

Otros dos problemas:

- La zona muerta absoluta de 0,08 L/min equivale a 8 % a 1 L/min. El error se estabilizaba en -3,9 %.
- En placa de una bomba, el núcleo usaba un máximo virtual de 4094 (dos bombas). Todo comando por encima de 2047 se descartaba sin aviso y la integral seguía acumulando sin efecto (windup oculto).

## 2. Mediciones en Eolo Mini Rev.2

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

- El AFM07 entrega una muestra cada 360 ms (2,8 Hz) en Mini Rev.2. El poll tiene un hueco de 250 ms; el sensor en sí refresca a 10 Hz o más. El cuello de botella está en el poll.
- Ruido: sigma entre 0 y 0,007 L/min. Resolución de lectura: 0,01 L/min.

### 2.3 Respuesta temporal

- Tiempo muerto: como máximo 1 muestra (360 ms).
- 63 % de la respuesta a 1,0-1,3 s; 95 % a 1,3-1,6 s.

## 3. Cambios

### 3.1 Máximo virtual y consigna inalcanzable

- El PWM virtual va de 0 a `2047 * motorCount`. Con `motorCount = 1` nunca se manda más de 2047.
- `unreachable` se activa cuando el mando queda en el máximo efectivo con error positivo sostenido durante 3 s (`kUnreachableConfirmMs`). Se libera al salir de esa condición.
- `MotorCaptureControl` escribe `flow_unreachable: ...` solo en cada cambio de estado, no en cada ciclo.
- Caso de referencia: a 7 L/min (sobre el máximo de la bomba) el comando antes subía a 2280 sin aviso. Ahora se limita a 2047 y el flag se activa.

### 3.2 Zona muerta relativa, deriva e integral

- Zona muerta efectiva = max(`deadband`, `deadbandRel * consigna`). Con 0,012 y 0,005 en Mini Rev.2: 0,012 L/min hasta 2,4 L/min y 0,025 L/min a 5 L/min.
- `driftThreshold`: error efectivo mínimo para contar deriva. Antes estaba fijo en 0,05.
- `freezeIntegral`: dentro de la zona muerta el integral se congela en vez de decaer 5 % por ciclo.

### 3.3 Detector CUSUM (ley anterior, solo activa si smoothLaw está apagada)

El CUSUM acumula el error crudo (consigna menos flujo) descontando una holgura `cusumK`. Cuando la suma supera `cusumH`, hay alarma:

1. El trim actual se absorbe en el centro. El trim y la integral pasan a 0.
2. Se aplica un salto del centro igual a la pendiente de la tabla (cuentas por L/min en el punto de operación) por el error medio, multiplicado por `cusumJumpFrac` y acotado a ±`cusumJumpMax`.
3. El lazo entra en RECENTER y espera `cusumHoldMs` más el tiempo de rampa del salto antes de volver a acumular.

Amortiguamiento: si el signo de un salto es opuesto al del salto anterior, la fracción se multiplica por 0,7 (mínimo 0,3). Se restablece al cambiar la configuración o al reiniciar el controlador.

Pausa: si el flujo es menor a 0,2 L/min o menor al 10 % de la consigna, el CUSUM no acumula ni dispara. Evita llevar la bomba a PWM máximo con la línea tapada.

Por defecto `cusumH` vale 0 (desactivado). Mini Rev.2 lo configura en 0,12, pero con `smoothLaw` activo no se usa.

### 3.4 Ley suave (smoothLaw), controlador actual en Mini Rev.2

Motivo: evitar saltos de PWM visibles en la bomba. Las prioridades, en orden, fueron centrar rápido, detectar alejamiento permanente del flujo y corregir con más frecuencia, pero con pasos suaves. El CUSUM cumplía lo último a costa de saltos de centro, por eso se reemplazó en Mini Rev.2. Se activa con el flag `smoothLaw`, que solo está encendido en Mini Rev.2. Las demás variantes siguen con la ley anterior.

Diseño (`SmartFlowController::updateSmooth`):

- Arranque y cambio de consigna: primero un paso de feedforward desde la tabla, al 92 % del objetivo (`smoothStartFraction`), escalado por el ratio de ganancia aprendido. Después hay una pausa de 1500 ms (`smoothHoldMs`) y luego una rampa sin integrar.
- Ley: PI incremental. Los términos se expresan en unidades de flujo divididas por la pendiente local de la tabla, multiplicadas por el ratio de ganancia observado. Así la ganancia de lazo es constante en todos los setpoints.
- El término P actúa sobre el cambio medido del flujo, no sobre el error. Ignora cambios menores o iguales a 0,012 L/min.
- Antes del lock, I y P se escalan. La integral usa 1,30 en vez de 0,70 (`smoothKiStart`).
- Salida: PWM interno con fracciones. La salida a la bomba solo cambia cuando el valor interno se aleja al menos 1 cuenta del último enviado (histéresis de 1 cuenta).
- Slew: 30 cuentas por muestra con lock; 100 cuentas por muestra antes del lock.
- Error persistente: la integral corrige de forma continua todo error mayor a 0,006 L/min. No hay saltos de centro ni pasos secantes.
- Se mantienen: techo de 2047 × `motorCount` con flag `unreachable`; guardia de flujo casi cero (el PWM no sube si el flujo está por debajo de 0,2 L/min o del 10 % del objetivo); latch de flujo cero.

Hallazgo: con pausa de 800 ms, el P bajaba unas 100 cuentas mientras el flujo aún subía, y eso causaba oscilación de ±3 % al arrancar. Por eso la pausa es de 1500 ms.

Constantes en `Dron.h`:

| Constante | Valor |
|---|---|
| `smoothKi` | 0,70 |
| `smoothKiStart` | 1,30 |
| `smoothKp` | 0,40 |
| `smoothSlew` | 30 cuentas |
| `smoothSlewStart` | 100 cuentas |
| `smoothStartFraction` | 0,92 |
| `smoothHoldMs` | 1500 ms |
| `deadband` | 0,012 L/min |
| `deadbandRel` | 0 |

### Mediciones de la ley suave

Mediciones en placa (Eolo Mini Rev.2, sin filtro). Asentamiento ±1 % con mediana de 3 muestras, contado desde el comando de inicio.

| Caso | Ley anterior | Ley suave |
|---|---|---|
| Asentamiento a 1 L/min | 11,5 s | 4,1 s |
| Asentamiento a 3 L/min | 3,8 s | 3,3 s |
| Asentamiento a 5 L/min | 7,3 s | 4,5 s |
| Asentamiento a 4 L/min | 7,5 s | 1,9 s |
| Ganancia -15 % a 4 L/min, vuelta a ±1 % | 12,1 s | 5,5 s (sobrepaso +0,5 %) |
| Ganancia +10 %, vuelta a ±1 % | 13,8 s | 5,4 s |
| Salto máximo de PWM por muestra tras perturbación (dos casos) | 115 y 89 | 30 y 30 |
| Saltos CUSUM | 9 + 16 | 0 |
| Error medio bloqueado | -0,5 % a +0,47 % | <= 0,11 % |
| Pico de arranque | 0 a +0,7 % | +0,2 a +2,0 % |

Estable a 3 L/min: flujo entre 2,99 y 3,01 L/min, PWM entre 917 y 922, paso máximo de 1 a 2 cuentas.

Simulación (planta construida con las mediciones, usando las clases C++ reales):

- Lock ±1 % en tres consignas: 3,6 / 5,7 / 5,0 s con la ley suave, frente a 9,7 / 4,7 / 7,5 s con la anterior.
- Recuperación tras -15 %: 4,5 s en media, frente a 14,7 s.
- Obstrucción lenta de -25 % en 60 s: error peor de -0,9 %, frente a -2,6 %.

Curva en lazo abierto medida: 1000 -> 3,30; 1400 -> 4,77; 1800 -> 6,06; 2047 -> 6,83 L/min. La tabla `kFlowFeedForward` queda entre 3 y 6 % baja. Respuesta: 63 % en 1,05 a 1,3 s; 95 % en 1,4 a 1,7 s.

### Limitaciones de la ley suave

- La placa corrió con pausa de 800 ms en el firmware flasheado; la pausa de 1500 ms se fijó por serial. El valor de `Dron.h` no se ha probado como firmware flasheado.
- No se probó cambio de setpoint en placa.
- Las perturbaciones de ganancia fueron por software: escalan el PWM, así que a PWM bajo cortan más del 15 %.
- No se probó con filtro, que ya no está en el equipo.
- La tabla FF no se actualizó.
- A 4 L/min el PWM sigue el ruido: ±4 cuentas, con variación total de 95 cuentas. La ley anterior tiene 0, porque se congela en su zona muerta y queda en -0,2 a -0,5 %. Si se quiere menos movimiento, bajar `smoothKp` a unos 0,3 o subir la zona muerta de 0,012 (sin probar).
- A 1 L/min el flag `locked` (±2 % durante 3 muestras) llega más tarde: 4,8 s frente a 2,9 s. Pero el flujo entra a ±1 % mucho antes.

Pruebas: 72 tests nativos, de los cuales 5 son nuevos (`test_flow_smooth_law_*`), 0 fallas.

Para flashear, ver [compilacion.md](compilacion.md).

## 4. Constantes en `Dron.h`

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

`DronLegacy.h` define `motorCount = 1` y no configura CUSUM ni `smoothLaw`. Express y EOLO-MP no definen `motorCount`, así que usan el valor por defecto (2), sin CUSUM.

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

## 7. Limitaciones conocidas

- El valor 0,55 y la reducción ×0,7 por cambio de signo no se han probado en placa con filtro real.
- El simulador de planta no modela el filtro. El test de amortiguamiento también pasa con `cusumJumpFrac` 1,0, así que no discrimina ese caso.
- No hay medición de la línea base con filtro real para comparar.
- Riesgo: un bloqueo puede disparar 1 o 2 saltos mientras el flujo cae de 5 L/min a 0,2 L/min.
- Trabajo no implementado:
  - Acelerar el bloqueo inicial (hoy 2,8 a 7,5 s).
  - Kick sin rampa: un kick comandado de 800 llega en la práctica a unas 312 cuentas.
  - Bajar `EOLO_AFM07_POLL_GAP_MS` después de medir la respuesta busy 0x06.
  - Persistir el PID en NVS.
  - `sensorFaultStopMs` de Mini Rev.2 usa el valor por defecto de 6000 ms (`DronLegacy` usa 1500 ms).

## 8. Cómo probar

- Tests portables: `pio test -e native`. Requiere compilador en el host. Los seis tests nuevos son `test_flow_cusum_*`, `test_flow_relative_deadband_*`, `test_flow_single_motor_*`. En Windows sin gcc se validó compilando con `zig c++` contra Unity: 67 tests, 0 fallas.
- Compilación: los entornos `eolo_dron` y `eolo_dron_legacy` y las demás variantes que usan `FlowPidConfig`, según [compilacion.md](compilacion.md).
- Prueba en placa:
  1. Capturar a 5 L/min y esperar estabilidad.
  2. Unos 20 s después, meter el filtro.
  3. Unos 60 s después, sacarlo.
  4. Verificar que el flujo vuelva a ±1 %.
  5. Revisar el log `flow_unreachable` y que no aparezcan saltos repetidos.

## Archivos involucrados

- `lib/EoloCore/include/Eolo/Core/Flow/SmartFlowController.h`: CUSUM, zona muerta relativa, integral congelado, tope de PWM y flag `unreachable`.
- `lib/EoloCore/include/Eolo/Core/Flow/FlowMotorController.h`: campos nuevos de `FlowPidConfig` y su validación.
- `lib/EoloCore/include/Eolo/Core/Flow/DualMotorFlowController.h`: máximo del PWM virtual según `motorCount`.
- `src/Common/Data/MotorCaptureControl.h`: log `flow_unreachable`.
- `src/Variants/Profiles/Dron.h` y `src/Variants/Profiles/DronLegacy.h`: constantes por variante.
