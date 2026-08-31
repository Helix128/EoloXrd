# Validación de entrega EOLO Dron Low Power — 2026-08-31

## Alcance y veredicto

Se validó el árbol de trabajo presente al iniciar la prueba y únicamente el
entorno `eolo_dron_low_power`. No se modificaron APIs, esquemas ni
comportamiento del producto.

**Prototipo interno: NO APTO / BLOQUEADO.** Las puertas de software y el
flasheo pasan, pero la tasa RS485 observada supera el límite obligatorio y no
se pudo ejecutar la captura de 15 minutos ni las solicitudes HTTP directas al
AP sin cambiar la red del operador. No se declara aptitud interna.

**Unidad de terreno: NO APTO.** Permanecen las brechas de seguridad, batería,
autonomía, ambiente y comportamiento indicadas al final.

No se creó la etiqueta local `eolo-dron-prototype-2026-08-31`, no se hizo push
y no se publicaron artefactos.

## Identidad del candidato

- Fecha: `2026-08-31`, America/Santiago (UTC-04).
- `HEAD`: `0487fcf3b1eac53287ce26fec1f507793acfafb4`.
- SHA-256 del diff actual (`git diff --no-ext-diff --binary`):
  `60e4fc6aa07c7845915368022ba1c36acb9ba0ac76192591ab472324709c8890`.
- Archivos modificados: 13.
- PlatformIO Core `6.1.19`; `espressif32@6.10.0`; Arduino ESP32
  `3.20017.241212+sha.dcc1105b`; esptool `4.5.1`; toolchain
  `xtensa-esp32 8.4.0+2021r2-patch5`.
- Unidad: ESP32-D0WDQ6 revisión 1.0, 4 MiB, MAC `08:3a:f2:b6:f1:e0`.

### Artefactos Low Power

| Artefacto | Tamaño | Offset | SHA-256 |
| --- | ---: | --- | --- |
| `firmware.bin` | 1.129.776 B | `0x10000` | `6dab049da82d0028e3b07ff3a73c58b0f4b722b6a0e3b6c68e418ad94ba3216f` |
| `bootloader.bin` | 18.992 B | `0x1000` | `3d97b71a61331ebaddef2a74e090c35299650f395f1387f85130010e84e46563` |
| `partitions.bin` | 3.072 B | `0x8000` | `0a8b5720e7b77ff11f1462458c3a509dee79224e5279898f26d6a2e3ae0517b7` |
| `firmware.elf` | 17.934.372 B | — | `d1b4028c39734b41ffccceb6ea69291c3b0257dfa53b97ade4341773d7f7c64a` |
| `firmware.map` | 12.549.517 B | — | `eeb2c99bcc67c5fd8516e844451a642617a969ed8c082f9d8a0e64b770418d43` |

Uso reportado: RAM `18,6 %` (`61.072/327.680`) y flash `57,1 %`
(`1.122.957/1.966.080`), bajo los límites de 25 % y 70 %.

## Puertas de software y trazabilidad

| Criterio | Resultado | Evidencia |
| --- | --- | --- |
| Limpieza del diff | **APROBADO** | `git diff --check` → 0. |
| Respaldo/versionado | **APROBADO** | `python3 -m unittest -v test.test_firmware_backup` → 7/7 OK. |
| Pinouts | **APROBADO** con pendiente histórica | `python3 scripts/check_pinouts.py` → 0; solo discrepancia Express Legacy 25/14 pendiente de hardware. |
| Suite nativa | **APROBADO** | `pio test -e native` → 31/31 casos OK. |
| Suites embarcadas Dron | **APROBADO** | Compilación sin upload/ejecución de `test_afm07`, `test_capture_switches`, `test_dynamic_motor_calibration`, `test_flowmotor`, `test_headless_motor_calibration`, `test_headless_setup`, `test_i2c`, `test_log_schema`, `test_ntc`, `test_rs485` y `test_rtc_timesync` → todas OK. |
| Build oficial Low Power | **APROBADO** | `pio run -e eolo_dron_low_power` → SUCCESS. |
| Recurso web generado | **APROBADO** | El generador no cambió `HeadlessSetupWebPage.h`; round-trip gzip igual a `build_html()` (31.037 B comprimidos, 105.958 B originales); sin CSS/JS externos; `node --check web-server/app.js` OK. |
| Remoto sin secretos | **FALLIDO** | La URL tiene forma `https://<credenciales>@github.com/Helix128/EoloXrd.git`; rotar token y reemplazar por URL sin credenciales. |

## Respaldo y carga de la unidad

Antes del upload se leyó la flash completa:

`firmware_backups/devices/eolo_dron/installed_20260831_160855/flash_4MB.bin`

- Tamaño: `4.194.304` B.
- SHA-256: `7ea1d9b2b2a5b75099f9c6726cdb30296ededbfe9ba3de13cfe67cabcf113b45`.
- Identificación: ESP32-D0WDQ6, MAC `08:3a:f2:b6:f1:e0`.

Se ejecutó `pio run -e eolo_dron_low_power -t upload` sobre `/dev/ttyUSB0`.
Esptool informó `Hash of data verified` para bootloader, particiones,
`boot_app0` y firmware, y terminó `SUCCESS` tras reinicio RTS. La lectura
independiente posterior (`read_flash` de los tres segmentos) no pudo
reconectar (`Failed to connect to ESP32: No serial data received`), por lo que
esa comprobación es **NO DISPONIBLE**; la verificación de escritura de esptool
sí quedó registrada.

El respaldo automático del candidato quedó en
`firmware_backups/devices/eolo_dron_low_power/latest/` con los hashes de la
tabla anterior.

## Evidencia física disponible

Registros bajo
`firmware_backups/devices/eolo_dron/installed_20260831_160855/`:

- `serial_preflight_terminal.log` — SHA-256
  `b8e35a1359e3bbe38d70116c7fe551635bf814f671abd8958ad3d2c421059e34`.
- `serial_preflight_detail.log` — SHA-256
  `6b2f9a93bb821820c7400f3364706e52199556f39edc32826b2b619bd39c823f`.
- `serial_diag_60s.log` — SHA-256
  `81bd8ba7bbd9f8ed36d40da913547fd0657d328268d3050107469b7791607f48`.

La consola reportó setup Wi‑Fi con AP activo en `192.168.4.1`, SSID
`eolo-dron` y LED violeta. El escaneo del host observó el AP en canal 6,
BSSID `08:3A:F2:B6:F1:E1`, WPA2. La última observación dejó RTC
`2026-08-31T16:17:04`, SD `ok`, motor `0 %` y PWM `0/0`.

Sensores en reposo: BME280 válido (`23,2 °C`, `52,8 %`, `1021,6 hPa`), NTC
válido (`27,2 °C`; observación 26,9–27,9 °C) y AFM07 online. I²C registró
`1217/1217` transacciones OK, 0 fallos y 0 recuperaciones; `0x68` (RTC) y
`0x76` (BME280) quedaron con fallos consecutivos 0. El intervalo máximo entre
respuestas AFM07 correcto observado fue `917 ms`, menor que 1,5 s.

RS485 registró `307` transacciones, `274` OK y `33` fallidas (`8` timeout,
`25` excepciones): `10,75 %` fallidas. Supera el máximo obligatorio de 2 % y
constituye una puerta **FALLIDA**, aunque AFM07 siguió online.

La prueba HTTP directa al AP, captive portal, `/healthz`, diagnósticos,
preset, listado, preview y descarga quedó **BLOQUEADA** por la instrucción
explícita de no cambiar la red del operador. Hubo un intento inicial de
asociación temporal antes de esa instrucción; se interrumpió antes de hacer
peticiones y el host terminó restaurado en `udd-recicla`. No se usa ninguna
consulta STA como evidencia funcional. El AP se sustenta solo por consola y
escaneo, no por una sesión HTTP completa.

## Matriz de aceptación física/funcional

| Puerta | Resultado | Medición/razón |
| --- | --- | --- |
| Batería, circuito neumático y caudalímetro externo calibrado | **NO DISPONIBLE** | No se aportó instrumento/medición externa. |
| microSD funcional | **APROBADO** | Consola: `SD: ok`; no se ejecutó captura QA. |
| BME280 reparado en `0x76` | **APROBADO** | Lecturas válidas y 0 fallos I²C consecutivos. |
| RTC correcto | **APROBADO** | Fecha/hora válida y avance monotónico observado. |
| NTC válido, motor apagado, sin sobretemperatura | **APROBADO** en reposo | 26,9–27,9 °C; PWM 0; sin umbral térmico alcanzado. |
| AFM07 fresco/online (<1,5 s) | **APROBADO** en reposo | `maxokgap=917 ms`, estado online. |
| I²C sin fallos consecutivos | **APROBADO** | 1217/1217 OK durante diagnóstico. |
| RS485 ≤2 % fallos | **FALLIDO** | 33/307 = 10,75 %. |
| Portal AP, captive, endpoints y recursos | **BLOQUEADO** | Requiere asociar host al AP; no se cambia la red. |
| Preset/config/listado/preview/descarga y QA 60 s | **BLOQUEADO** | No se envió confirmación HTTP. |
| Cuatro switches y las siete decodificaciones físicas | **BLOQUEADO** | Solo se comprobó tabla por tests; unidad quedó Off/Off. |
| Resistencia instantánea + duración 15 min | **BLOQUEADO** | No se activaron switches ni motor. |
| Captura 15 min, caudal y volumen externo | **BLOQUEADO** | Sin caudalímetro/captura; no hay CSV QA nuevo. |
| BME por fila, timestamps, volumen, fila `Finalizado` | **BLOQUEADO** | No se generó captura QA. |
| Sin resets/watchdog/SD perdida durante captura | **BLOQUEADO** | No hubo ventana de 15 minutos; RS485 ya falla en reposo. |
| Índice maestro actualizado y descargable | **BLOQUEADO** | `logs index status`: 33 examinados, 2 actuales, 31 recuperados; sin QA. |
| PWM 0, escritura completa, patrón Low Power y deep sleep | **BLOQUEADO** | No se inició sesión. |
| Corte 5 min y retención RTC | **BLOQUEADO** | No se hizo power-cycle físico. |
| Arranque con espera Off regresa a setup Wi‑Fi | **APROBADO** en estado observado | Unidad arrancó/quedó en setup con cuatro entradas Off; sin power-cycle de 5 min. |

## Brechas de entrega y terreno

La unidad no es apta para terreno hasta resolver y volver a certificar:

1. AFM07 inválido conserva el último PWM.
2. NTC desconectado antes del sobrecalentamiento no bloquea el motor.
3. SD ausente no impide iniciar o mantener la captura.
4. Endpoints de depuración pueden accionar el motor sin autenticación robusta y
   el AP usa credenciales conocidas (`eolo-dron`/`eolo-dron`).
5. No existe medición/protección verificable de batería por bajo voltaje;
   `battery_pct` permanece en `-1`.
6. Modo infinito, autonomía, vibración, montaje en vuelo, ambiente, EMI y
   resistencia a intemperie no están certificados.
7. Rotar la credencial del remoto Git y dejar la URL sin secretos antes de
   compartir o publicar.

No se borraron registros históricos ni se modificó el remoto. La última
observación válida dejó el motor apagado y la red del host restaurada; el
estado posterior al intento de lectura post-flash quedó **NO DISPONIBLE**.
