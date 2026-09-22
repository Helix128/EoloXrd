# Pendientes para calificar AFM07 y habilitar producción

Documento de traspaso para retomar en casa. El estado actual sigue siendo
**NO APTO / BLOQUEADO**: no iniciar sesiones productivas ni declarar un
intervalo aprobado hasta cerrar todas las casillas de este documento.

## Evidencia que motiva el bloqueo

- La medición previa a `200 ms` tuvo `103/949` fallos RS485: `66` excepciones
  Modbus `0x04`, `17` timeouts y `0` errores CRC.
- El código y la automatización del barrido están en el árbol de trabajo, pero
  las compuertas finales deben repetirse después de los últimos cambios.
- Ninguna prueba finita demuestra que el hardware nunca fallará. La aceptación
  significa cero fallos durante las ventanas observadas y apagado seguro ante
  cualquier fallo posterior.

## 1. Cierre de software antes de tocar hardware

- [ ] Revisar el diff completo y ejecutar `git diff --check`.
- [ ] Ejecutar `python3 -m unittest -v test.test_firmware_backup`.
- [ ] Ejecutar `python3 scripts/check_pinouts.py` y dejar anotada cualquier
      discrepancia histórica de pinout.
- [ ] Ejecutar `pio test -e native`.
- [ ] Compilar las suites embarcadas Dron sin ejecutar pruebas que requieran
      hardware (`pio test -e eolo_dron_low_power --without-testing`).
- [ ] Ejecutar `pio run -e eolo_dron_low_power`.
- [ ] Repetir, si el tiempo lo permite, la compilación de
      `eolo_express`, `eolo_express_legacy`, `eolo_standard` y
      `eolo_standard_libraries`.
- [ ] Verificar el portal con `node --check web-server/app.js`.
- [ ] Verificar los scripts con:

      ```bash
      python3 -m py_compile scripts/afm07_rate_sweep.py \
        scripts/platformio_afm07_gap.py
      ```

- [ ] Regenerar `src/Board/HeadlessSetupWebPage.h` con
      `python3 scripts/generate_headless_setup_web.py` y comprobar que el
      recurso generado corresponde exactamente al portal fuente.
- [ ] Confirmar que el portal solo muestra `pollGapMs`; no debe existir un
      setter HTTP para cambiar la cadencia.
- [ ] Confirmar en código y en la salida `rs485 status` el contrato fijo:
      ID `0x02`, baud `4800`, función `0x03`, registro de flujo `0x0000`,
      cantidad `1` y diagnóstico `0x0004`.
- [ ] Confirmar que un error AFM07 deja el enclavamiento activo, PWM en cero y
      no rearma el actuador con una muestra posterior sin diagnóstico explícito
      `0x0004 == 0`.
- [ ] Si una confirmación web queda bloqueada por seguridad, comprobar que el
      equipo vuelve a dejar disponible el portal para consultar el diagnóstico;
      corregir ese flujo antes de producción si la interfaz queda apagada.

No copiar como aprobación los resultados históricos de otra compilación o de
otra unidad. Guardar el SHA de Git, el entorno PlatformIO y el binario cargado
junto con cada resultado.

## 2. Preflight físico obligatorio

Con el motor apagado y sin iniciar una captura:

- [ ] Medir alimentación estable entre `9` y `24 V DC` durante el ensayo.
- [ ] Confirmar masa común entre ESP32, transceptor RS485 y AFM07.
- [ ] Confirmar polaridad A/B en ambos extremos.
- [ ] Confirmar terminación y ausencia de derivaciones/cables flojos.
- [ ] Confirmar que el AFM07 está configurado a ID `0x02`, `4800` baud y
      respuesta Modbus RTU.
- [ ] Solicitar `POST /api/diagnostics/afm07` y esperar estado `complete`.
- [ ] Exigir `valueValid=true`, `register0004=0` y
      `rateTuningBlocked=false` en `GET /api/diagnostics/afm07` y
      `/api/diagnostics`.
- [ ] Confirmar por `/api/status` y `/api/diagnostics` que la captura está
      inactiva y que el PWM es `0`.

Si falla cualquiera de estas comprobaciones, detener el barrido: el problema
no es corregible aumentando el rate. Si `0x0004` vale `1`, `2` o `3`, o aparece
una excepción `0x04` incluso a `1000 ms`, revisar sensor, EEPROM, alimentación,
configuración y cableado antes de volver a medir. No ampliar timeouts para
ocultar el fallo.

Mantener la red del computador sin cambios. El script solo debe hablar con la
URL LAN/AP indicada; no activar una conexión STA ni cambiar la red del
operador para conseguir evidencia.

## 3. Barrido de cadencia

Los intervalos candidatos son `1000`, `800`, `600`, `400` y `200 ms`. Para cada
firmware:

1. Compilar con `EOLO_AFM07_POLL_GAP_MS=<gap>` y cargarlo solo después de pasar
   el preflight.
2. Reiniciar la unidad y esperar que el endpoint `/api/diagnostics` responda.
3. Repetir el diagnóstico exclusivo de `0x0004` y exigir valor cero.
4. Confirmar motor apagado/captura inactiva.
5. Esperar `30 s` de estabilización.
6. Reiniciar las estadísticas **después** de la estabilización y del diagnóstico;
   esas operaciones no forman parte de la ventana calificada.
7. Medir durante `180 s` por API LAN directa, guardando una muestra JSON por
   segundo (o el periodo acordado).
8. No reiniciar automáticamente una captura ni saltar una ventana fallida.

Automatización disponible:

```bash
python3 scripts/afm07_rate_sweep.py --upload --preflight-ok \
  --base-url http://192.168.4.1 \
  --intervals 1000 800 600 400 200
```

El modo predeterminado se detiene en el primer intervalo fallido. Para hacer
un barrido exhaustivo en una sesión explícitamente autorizada se puede añadir
`--continue-on-failure`; eso no reanuda ni reinicia capturas. Si se detiene,
continuar manualmente con el siguiente intervalo más lento y repetir la
calificación completa.

El script debe dejar, como mínimo:

```text
artifacts/afm07-rate-sweep/<gap>/samples.jsonl
artifacts/afm07-rate-sweep/<gap>/qualification.json
artifacts/afm07-rate-sweep/summary.csv
```

### Criterios de aprobación de cada intervalo

Todas las muestras de la ventana deben cumplir simultáneamente:

- [ ] Cero excepciones Modbus, timeouts, CRC, tramas incompletas o malformadas,
      bus ocupado, tramas inesperadas, bytes tardíos y misses de deadline.
- [ ] Cero fallos en cada esclavo informado, no solo en el agregado global.
- [ ] AFM07 siempre `online` y con muestra `fresh`.
- [ ] Diagnóstico completo, válido y `register0004 == 0`.
- [ ] SD, NTC, BME280 y RTC válidos.
- [ ] `maxSuccessGapMs <= 1500`.
- [ ] El descanso real mínimo entre transacciones AFM07 cumple el gap
      configurado dentro de la tolerancia de instrumentación de `5 ms`
      (`minRestMs + 5 >= gap configurado`).
- [ ] La captura permanece inactiva y el PWM permanece en cero durante el
      barrido.

Un único incumplimiento invalida el intervalo completo. Registrar el motivo,
la hora, el firmware y el estado físico; no editar el JSON para convertirlo en
aprobado.

## 4. Selección del intervalo de producción

Elegir el intervalo aprobado más rápido y usar **un escalón más lento**:

| Más rápido aprobado | Candidato de producción |
| ---: | ---: |
| `200 ms` | `400 ms` |
| `400 ms` | `600 ms` |
| `600 ms` | `800 ms` |
| `800 ms` | `1000 ms` |
| Solo `1000 ms` | **BLOQUEADO: no hay margen** |

- [ ] Repetir la ventana completa para el candidato más lento; no basta con
      compilarlo.
- [ ] Si el candidato de margen no tiene un resultado aprobado, mantener
      **NO APTO** y ejecutar de nuevo esa calificación antes de producción.
- [ ] Si ningún intervalo aprueba, detener el ajuste y resolver primero el
      hardware/EMI/cableado/sensor.

## 5. Captura real de aceptación (solo después del barrido)

Con el firmware del intervalo seleccionado, el motor apagado durante la
preparación y un caudal externo controlado de `5 L/min`:

- [ ] Confirmar otra vez preflight, diagnóstico `0x0004=0`, SD, NTC, BME280 y
      RTC válidos.
- [ ] Iniciar una única captura de `15 min`; no usar rearranque automático.
- [ ] Verificar durante toda la sesión cero errores RS485 de todas las clases.
- [ ] Verificar `maxSuccessGapMs <= 1500 ms` y AFM07 siempre fresco.
- [ ] Confirmar que no hubo reset, watchdog, pérdida de SD ni pérdida de
      alimentación.
- [ ] Confirmar caudal/volumen externo, timestamps, lecturas BME por fila y
      fila final `Finalizado` en el CSV.
- [ ] Confirmar índice maestro actualizado y descarga/preview del archivo.
- [ ] Confirmar PWM final exactamente `0` y apagado seguro del motor.

Cualquier fallo invalida la captura, fuerza PWM cero y obliga a reiniciar la
calificación completa con el siguiente intervalo más lento. No reanudar la
captura ni declarar una aprobación parcial.

## 6. Cierre documental y de seguridad

- [ ] Conservar JSONL, JSON de calificación, CSV resumen, logs serie, hashes de
      firmware y mediciones de alimentación/cableado.
- [ ] Actualizar `docs/validacion-entrega-eolo-dron-2026-08-31.md` con la
      evidencia nueva sin borrar el historial ni cambiar **NO APTO** antes de
      cerrar todas las puertas.
- [ ] Registrar unidad, MAC, fecha/hora, operador, caudalímetro y entorno de
      compilación.
- [ ] No iniciar sesiones productivas mientras falte el barrido o la captura
      de 15 minutos.

## 7. Pendientes adicionales para aptitud de terreno

Estos puntos ya estaban bloqueados en la validación de entrega y no quedan
resueltos por aprobar el rate AFM07:

- [ ] Medir batería, protección por bajo voltaje, autonomía y retención tras
      corte/power-cycle de `5 min`.
- [ ] Validar circuito neumático y caudalímetro externo, vibración, montaje en
      vuelo, EMI, ambiente e intemperie.
- [ ] Probar las cuatro entradas de switches y las siete decodificaciones
      físicas, incluido arranque con espera `Off` que vuelve a setup Wi-Fi.
- [ ] Validar el flujo completo del portal: captive portal, preset,
      confirmación, listado, preview, descarga, borrado e índice maestro.
- [ ] Revisar autenticación/autorización de endpoints de depuración y cambiar
      las credenciales conocidas del AP.
- [ ] Rotar cualquier token expuesto en la URL del remoto Git y dejar el remoto
      sin secretos antes de compartir/publicar.
- [ ] Repetir la prueba de deep sleep, patrón LED Low Power y apagado después de
      una captura válida.

Hasta que las casillas de las secciones 1–6 estén cerradas y las de terreno
sean aceptadas por el responsable, la etiqueta operativa permanece:

> **NO APTO / BLOQUEADO — no iniciar producción.**

