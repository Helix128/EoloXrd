# GOAL_WEBSERVER

Documento: Problema y solución teórica para el webserver headless (ESP32, PlatformIO/Arduino)

---

## Objetivo
- Migrar respuestas JSON del webserver a ArduinoJson.
- Eliminar por completo el uso de `String` en el firmware (política: 100% avoid Strings).
- Mantener captive portal (DNSServer + softAP) y rutas HTTP actuales.
- Asegurar servidor estable, UI responsiva y bajo riesgo de fragmentación de heap.

## Contexto y restricciones
- Plataforma: ESP32, PlatformIO, framework Arduino.
- Requisitos: evitar `Arduino String` en firmware; usar ArduinoJson (bblanchon/ArduinoJson@^6.20).
- Reutilizar endpoints existentes: `/`, `/api/status`, `/api/logs`, `/api/logs/preview`, `/api/calibration/*`, `/download`, `/api/confirm`, etc.
- Captive portal debe seguir funcionando (DNS + softAP en el puerto 80).

## Problemática detectada (resumen técnico)
1. Uso intensivo de `String` en handlers del webserver
   - Construcción de respuestas JSON con `String out; serializeJson(doc, out); _server.send(..., out)`.
   - Lectura de ficheros con `file.readStringUntil('\n')`, `readStringUntil` y almacenamiento en `String rows[]`.
   - Uso de `_server.arg(...)` que devuelve `String` (parámetros query/body).
   - Funciones utilitarias que aceptan/retornan `String` (p. ej. `isSafeLogBasename`, `safeLogPathFromRequest`).
   - `jsonEscape` que construye `String` manualmente.

   Consecuencia: fragmentación de heap, picos de uso y riesgo de fallos en runtime en un MCU con heap limitado.

2. Serialización JSON ineficiente
   - Serializar a `String` causa asignaciones internas y fragmentación. Necesitamos serializar sin crear `String` intermedios.

3. APIs WebServer que exponen `String`
   - WebServer facilita _arg()/uri()/argName()_ y devuelve `String`. Eso impone una decisión: aceptar pequeñas temporales `String` o sustituir la capa HTTP.

4. Errores de compilación previos por tipos incompletos (Context)
   - Implementaciones inline en headers referenciaban `struct Context` mientras el header solo tenía forward-declaration → "invalid use of incomplete type 'struct Context'".

   Solución aplicable: mover implementaciones dependientes de Context fuera de los headers a `.cpp` para romper la dependencia circular.

5. Streaming / descarga de archivos
   - `_server.streamFile()` y `Content-Disposition` se usan con `String` para componer cabeceras; debemos componer cabeceras con buffers C.

---

## Principios de diseño propuestos
- Política de memoria:
  - Evitar `String` en la lógica del firmware. C permitidos: `char[]`, `const char*`, `malloc/free` controlados.
  - Uso preferente de `StaticJsonDocument` para respuestas pequeñas/estáticas; `DynamicJsonDocument` con tamaño controlado para respuestas variables.
  - Minimizar llamadas a `malloc/free`; preferir buffers estáticos reutilizables para respuestas frecuentes.
- Serialización:
  - Serializar siempre a un `char*` o directamente a un Stream/cliente (streaming) — nunca a `String`.
- Parámetros HTTP:
  - Evitar dependencias de `WebServer::arg()`; leer y parsear la petición en C cuando sea necesario para eliminar `String` completamente.
- SD / I/O:
  - Reemplazar `readStringUntil` por `readBytesUntil` y tratar cadenas como `char[]`.
- Compilación y dependencia:
  - Mover implementaciones que necesitan tipos completos (Context, etc.) a archivos `.cpp` y mantener headers como declaraciones.

---

## Solución teórica propuesta (detallada)

### 1) JSON: ArduinoJson + serialización sin `String`
- Para respuestas pequeñas (ej: `/api/status`) usar `StaticJsonDocument<...> doc;`.
- Para respuestas medianas/grandes usar `DynamicJsonDocument doc(size)` o estimar con `measureJson()` para asignar buffer exacto.

Patrón seguro (sin `String`):

```c
StaticJsonDocument<1024> doc;
// ... llenar doc ...
size_t needed = measureJson(doc) + 1;
char *buf = (char*) malloc(needed);
if (buf) {
  serializeJson(doc, buf, needed);
  _server.send(200, "application/json", buf);
  free(buf);
} else {
  _server.send(503, "application/json", "{\"error\":\"memory\"}");
}
```

- Ventajas: evita `String`, tamaño exacto, respuesta con `Content-Length` correcta.
- Desventajas: `malloc/free` para cada petición puede fragmentar en largo plazo. Mitigación: reutilizar un buffer estático o un pequeño pool para respuestas de tamaño limitado.

Alternativa más eficiente (streaming): serializar directamente al cliente stream si se puede garantizar que el servidor permita enviar headers primero y luego escribir cuerpo:

```c
// 1) enviar cabeceras sin cuerpo
_server.sendHeader("Content-Type", "application/json", true);
_server.send(200, "application/json", ""); // enviar cabeceras
// 2) serializar al cliente stream
serializeJson(doc, _server.client());
```

- Ventajas: cero buffers adicionales, cero Strings.
- Riesgos: compatibilidad con la implementación WebServer (asegurar que enviar body después de `send()` sea aceptado), manejo de `Content-Length` o chunked transfer. Requiere pruebas.

Conclusión: Implementar primero el patrón con buffer asignado; evaluar streaming en siguiente iteración para eliminar malloc/free.

### 2) Evitar `String` al leer parámetros HTTP
Opciones:

- Opción A — Reimplementación ligera HTTP parser (recomendada si se quiere 0% String):
  - Usar `WiFiServer`/`WiFiClient` (webserver mínimo) y parsear primera línea `GET /path?a=1&b=2 HTTP/1.1` en un `char` buffer.
  - Extraer query string en `char[]` y aplicar `url_decode()` en sitio.
  - Para `POST` con `application/x-www-form-urlencoded` o JSON: leer `Content-Length` bytes y parsear raw body en buffers.
  - Ventaja: control total, cero `String`.
  - Coste: mayor complejidad, tests, reintegrar captive portal behavior (DNS redirect).

- Opción B — Mantener WebServer y minimizar impacto:
  - Aceptar el uso temporal y localizado de `_server.arg()` (que genera `String`) sólo para parámetros pequeños y copiar inmediatamente a `char[]` con `strlcpy`/`snprintf` antes de salir del handler.
  - Ventaja: mínimo esfuerzo; desventaja: no 100% libre de `String` (pero menor exposición y menor tiempo de vida de String).

- Opción C — Cambiar a otra librería HTTP (p. ej. AsyncWebServer)
  - AsyncWebServer tiene APIs diferentes y callbacks que pueden ofrecer acceso a raw buffers en algunos casos.
  - Evaluar si sus APIs realmente exponen `const char*` sin Strings; a menudo siguen usando `String` internamente.

Recomendación práctica: si el requisito es contractual ("100% avoid Strings"), elegir Opción A. Si es pragmático, empezar por Opción B y eliminar los últimos `String` cuando tiempo permita.

### 3) Lectura de ficheros SD y previews
- Reemplazar `file.readStringUntil('\n')` por `file.readBytesUntil('\n', buf, buf_size)`.
- Evitar arrays `String rows[40]`; usar `char rows[40][ROW_MAX]` o circular buffer de punteros a buffers reciclados.
- Para `file.name()`, usar el `const char*` devuelto por la API (no convertir a `String`). Extraer basename con `strrchr()`.

Ejemplo: extraer últimas N líneas sin `String`:

```c
const size_t ROWS = 40;
const size_t ROW_MAX = 256;
char rows[ROWS][ROW_MAX];
size_t count = 0;
while (file.available()) {
  size_t rlen = file.readBytesUntil('\n', rows[count % ROWS], ROW_MAX-1);
  rows[count % ROWS][rlen] = '\0';
  // trim CR
  if (rlen>0 && rows[count % ROWS][rlen-1]=='\\') rows[count % ROWS][rlen-1]='\0';
  // aceptar/ignorar según lógica
  count++;
}
```

### 4) Cabeceras y descargas (`Content-Disposition`)
- Componer cabecera en `char disp[...]; snprintf(disp, sizeof(disp), "attachment; filename=\"%s\"", filename);
- Evitar concatenación `String`.
- `streamFile` mantiene su propio buffer; sólo asegurar que `filename` venga de `char*` seguro.

### 5) Compilación: tipos incompletos (Context)
- Causa: métodos inline en headers utilizaban `Context` cuando sólo se tenía forward-declaration.
- Solución: declarar la clase/struct y métodos en headers; mover implementaciones que usan `Context` a `.cpp` y `#include "Context.h"` ahí.

Ejemplo:
- `LogService.h` → sólo prototipos y miembros.
- `LogService.cpp` → `#include "LogService.h"` + `#include "Context.h"` y definiciones.

### 6) Estrategia de migración (pasos concretos)
Prioridad alta (inmediata):
1. Mover implementaciones que causan dependencias circulares a `.cpp` y compilar.
2. Añadir dependencia ArduinoJson en `platformio.ini`.
3. Reescribir handlers principales con pattern `measureJson() + malloc + serializeJson + _server.send(...)`.
4. Reemplazar I/O con `readBytesUntil` y `char[]`.
5. Cambiar utilidades: `isSafeLogBasename(const char*)`, `bool safeLogPathFromRequest(char *out, size_t outLen)`.

Prioridad media:
6. Revisar todos los usos de `_server.arg()` y decidir: reemplazar por parser (Opción A) o copiar a buffer (Opción B).
7. Implementar tests funcionales sobre endpoints.

Prioridad baja / opcional:
8. Implementar streaming directo a cliente para grandes respuestas (evitar malloc/reuse).
9. Añadir un pool de buffers para respuestas JSON para reducir malloc/free.

### 7) Checklist de verificación y pruebas
- Build: `pio run -e eolo_dron` debe pasar.
- Runtime tests en dispositivo:
  - Conectar a AP headless, abrir página, probar endpoints.
  - Listar logs, preview, descargar archivo, download-all.
  - Iniciar/abortar/clear calibración.
  - Confirmar config `/api/confirm`.
- Pruebas de memoria:
  - Ejecutar endpoints repetidamente y monitorizar `ESP.getFreeHeap()` o logs para detectar fragmentación.
- Lint/CI:
  - Falla la build si `rg "\bString\b" src | grep -v "allowed exceptions"` encuentra usos remanentes en código productivo.
  - Agregar una prueba que mide y reporta `measureJson()` para responses clave y asegura `needed < BUFSIZE` si se usa buffer estático.

### 8) Regla práctica para evitar Strings
- Tolerar temporalmente `String` si y sólo si:
  - Se produce por la API WebServer y el String no se almacena ni se propaga fuera del handler.
  - Se copia inmediatamente a un `char[]` y la `String` queda fuera de scope.
- Pero si el objetivo es 100% libre de `String`, planificar la reimplementación HTTP (Opción A).

---

## Riesgos y mitigaciones
- Riesgo: `malloc/free` repetido fragmenta el heap.
  - Mitigación: reutilizar buffers, evitar malloc en cada petición, preferir `StaticJsonDocument` y reuse de buffer global.
- Riesgo: reescritura del parser HTTP introduce bugs de seguridad (inyección, path traversal).
  - Mitigación: aplicar validaciones estrictas (ej. isSafeLogBasename), pruebas unitarias, limitación de longitudes.
- Riesgo: cambiar servidor web rompe comportamiento captive portal.
  - Mitigación: replicar rutas de captura (`/generate_204`, `/ncsi.txt`) y probar con clientes reales (Android/iOS).

---

## Plan de acción recomendado (sprint corto)
1. Consolidar cambios que arreglan compilación (mover impl a .cpp). -> hecho en rama actual.
2. Reescribir handlers claves con `serializeJson` a `char*`. -> aplicar a todos los endpoints `/api/*`.
3. Sustituir I/O `String` por `readBytesUntil` y `char[]`.
4. Decidir estrategia para `_server.arg()`:
   - Si tiempo: implementar parser HTTP minimal (0 Strings).
   - Si urgencia: copiar parámetros a `char[]` y documentar deuda técnica.
5. Añadir CI check que falle si aparecen usos de `String` en `src/Board` y `src/Data`.

Tiempos estimados (orientativos):
- Pasos 1-3: 1-2 días.
- Paso 4 (parser completo): +2-4 días.
- Tests y robustez: +1 día.

---

## Conclusión
- Ya aplicado: se corrigieron errores de compilación moviendo implementaciones sensibles fuera de headers, y se introdujo patrón de serialización a `char*` en handlers críticos.
- Fase siguiente: decidir si se implementa parser HTTP propio (para cumplir 100% no-String) o mantener WebServer y minimizar exposición a `String` (pragmático).

Si quieres, genero un TODO/patch list automatizado con `rg` + `edits` para eliminar las ocurrencias remanentes de `String` en `src/Board/HeadlessSetupServer.h` y otros archivos, o preparo un prototipo de parser HTTP minimal que reemplace `_server` para el captive portal.  

---

Fecha: 2026-06-05
Proyecto: EoloXrd (ESP32 headless web setup)

