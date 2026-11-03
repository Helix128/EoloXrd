# FS3000 en Express Rev.1: datos y ajuste del PID

Datos medidos con la demo `FS3000FlowTune` (`demos/FS3000FlowTune/`) en un equipo Express Rev.1 el 2026-11-03, sin filtro en la línea. Sirven para afinar `DualPumpFlow::makeFs3000Config` (`lib/EoloCore/include/Eolo/Core/Flow/DualPumpFlowConfig.h`).

| Archivo | Contenido |
|---|---|
| `sweep.log` | `sweep` por defecto: bomba A sola, B sola y serie (débil a tope + fuerte subiendo), paso 128, 2 s de asentamiento y 3 s de medida |
| `steps.log` | `hold` de 10 s por bomba y seis `step` a 10 ms por muestra |
| `simulacion.cpp` | Simulación con el controlador real y la planta medida, usada para elegir las ganancias (ver abajo) |

## Hallazgo de hardware: la bomba A casi no mueve aire

| Bomba | Arranque | Caudal a tope |
|---|---|---|
| A (GPIO 26) | PWM ~1664 | 0,09-0,12 L/min |
| B (GPIO 27) | PWM ~256 | 4,2-4,3 L/min |
| A a tope + B subiendo | | 4,6 L/min |

Se repitió y da igual (`hold` de 10 s). Una bomba sana mueve varios L/min, así que A está desconectada, rota o su vía de aire no pasa por el sensor. Con este equipo no se puede llegar a 5 L/min (`kDroneTargetFlowLpm`). Si el firmware lo usa tal cual, la prueba de orden de bombas dará A como débil y el control podría disparar `pump_failure_zero_flow` mientras sube A.

Por eso la tabla PWM→caudal del perfil no se tocó: seguiría siendo la nominal de dos bombas. Con la bomba A reparada, repetir `sweep` y rehacer la tabla.

## Lo medido (no depende de la bomba A)

**Dinámica** (escalones con PWM instantáneo, `steps.log`): retardo ~40 ms, constante de tiempo ~65 ms, t90 ~170 ms. Con la media de 16 muestras de `FS3K` el caudal que ve el PID tarda t63 ~200 ms y t90 ~280 ms. Con el AFM07 el retardo era 0,3 s y tau 0,4 s.

**Ruido** (régimen, últimos 1,5 s de cada escalón):

| Caudal | sigma instantáneo | sigma media de 16 | sigma de Δ a 100 ms (media de 16) |
|---|---|---|---|
| 2,2 L/min | 0,04 | 0,02 | 0,02-0,03 |
| 3,7-3,9 | 0,08-0,09 | 0,05-0,07 | 0,05-0,06 |
| 4,3 | 0,10 | 0,08 | 0,06 |

El ruido crece con el caudal y a caudal alto está muy correlacionado (autocorrelación 0,9 a un paso). La zona muerta de lectura del término P de la ley (`kSmoothDyDeadband` = 0,025 L/min) queda por debajo del ruido a 4 L/min.

## Parámetros elegidos

Búsqueda en malla con `simulacion.cpp` (controlador real, planta medida, lectura cada 10 ms con media de 16, ruido medido, ganancia de planta ±25 %, consignas 1-4 L/min y escalones). Compilar con `g++ -std=gnu++17 -O2 -Ilib/EoloCore/include -Iinclude simulacion.cpp`; `./a.out grid` corre la malla.

| Parámetro | Rev.2 (AFM07) | FS3000 | Motivo |
|---|---|---|---|
| `smoothKi` | 0,70 | 1,5 | La planta es 6 veces más rápida; asienta en ~1 s en vez de ~2,5 s |
| `smoothKiStart` | 1,30 | 2,8 | Conserva la razón 1,86 |
| `smoothKp` | 0,15 | 0,05 | El ruido de Δ a 100 ms supera la zona muerta; menos P, menos PWM movido |
| `smoothHoldMs` | 1500 | 300 | Retardo + 2 tau + media de 16 muestras ≈ 250 ms |
| `deadband` | 0,012 | 0,05 | Por debajo del ruido la integral mueve el PWM; 0,05 baja el movimiento ~25 % sin perder error en régimen ni a 1 L/min |

`smoothSlew` (30 o 60) e `intervalMs` (50 o 100) casi no cambian el resultado; se dejaron los de Rev.2.

Resultado en simulación (suma sobre 8 casos): asentamiento 6,9 s contra 18,5 s con los valores de Rev.2. Se mantiene estable con tau 4 veces mayor y ruido doble.

## Pendiente

- Repetir `sweep` con la bomba A reparada y ajustar `kFeedForward` si difiere.
- Probar el lazo cerrado en placa y con el filtro real. La simulación usa una planta ajustada a escalones sin filtro.
- A ganancia de planta +25 % el arranque sube ~15 % sobre la consigna (la tabla manda el PWM inicial); es el mismo comportamiento que en Rev.2.
