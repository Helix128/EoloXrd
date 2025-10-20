## EOLO v2 — Documentación

### Introducción
EOLO es un dispositivo que mide y registra parámetros ambientales y controla un motor para generar un flujo de aire medido. Esta documentación explica de forma simple qué hace el sistema, cómo interactuar con él y dónde se guardan los datos.

### Qué hace
- Muestra información en una pequeña pantalla (logo, hora, batería, estado de la SD).
- Permite configurar un flujo objetivo (L/min) y programar cuándo iniciar una captura.
- Mide temperatura, humedad, presión y partículas (si está habilitado el sensor Plantower).
- Registra las mediciones en una tarjeta SD en formato CSV.
- Controla un motor para ajustar el flujo.

### Componentes principales
- Pantalla: muestra menús, valores y estado del equipo.
- Encoder: sirve para navegar menús y confirmar opciones (girar = cambiar valor, pulsar = seleccionar/confirmar).
- Motor: ajusta la generación del flujo de aire.
- Sensores:
  - BME280: temperatura, humedad y presión.
  - Plantower: partículas PM1.0 / PM2.5 / PM10 (opcional, consume más batería).
  - Sensor de flujo (flujo de aire en L/min).
- Batería: alimenta el dispositivo y se muestra el porcentaje en pantalla.
- Tarjeta SD: guarda un archivo `log.csv` con los registros de captura.
- RTC (reloj de tiempo real): permite programar horas de inicio/fin para las capturas.

### Flujo de uso
1. Encendido: el dispositivo arranca y muestra un logo (splash) unos segundos.
2. Menú principal (Home): aparecen opciones como "Iniciar captura inmediata", "Continuar sesión anterior" o "Iniciar nueva sesión".
3. Ajustar flujo: al elegir configurar el flujo, giras el encoder para cambiar el valor en L/min y pulsas para confirmar.
4. Programar horas: si no inicias la captura inmediata, el sistema te pedirá la hora de inicio y fin (se usan minutos del día). Confirma con el botón.
5. Espera: si configuraste un inicio programado, el equipo muestra la pantalla de espera con la hora y el flujo actual. Al alcanzar la hora, inicia la captura automáticamente.
6. Captura activa: durante la captura el sistema controla el motor para mantener el flujo y guarda lecturas periódicas en la SD. En pantalla hay un tablero (dashboard) con información relevante.
7. Fin de la captura: la sesión termina cuando se alcanza la duración programada o cuando se detiene manualmente. Al terminar muestra un resumen y al hacer clic con el encoder muestra un menu que permite reiniciar el eolo o volver al resumen.

### Controles rápidos
- Girar el encoder: moverte entre opciones o cambiar valores (por ejemplo, flujo o minutos).
- Pulsar el encoder: confirmar la opción o avanzar en la configuración.
- En algunas pantallas, el comportamiento cambia según el contexto (por ejemplo, confirmar puede iniciar la captura o guardar la hora).

### Archivo de datos en la SD
- Nombre: `log.csv` (guardado en la tarjeta SD).
- Contiene, por cada registro, una línea con columnas separadas por comas. Entre los campos: timestamp, flujo observado, flujo objetivo, temperatura, humedad, presión, PM1.0, PM2.5, PM10, porcentaje de batería.
- Si el sensor Plantower está desactivado o falla, sus columnas contienen `-1`.

### Archivos clave del proyecto
- `EOLO_v2.ino`: punto de entrada — arranca el sistema y ejecuta el bucle principal.
- `AppContext.h`: reúne todos los módulos hardware y el estado de la aplicación (es el "cerebro" del programa).
- `Drawing/SceneManager.h` y `Drawing/SceneRegistry.h`: manejan las pantallas/escenas que ves en la pantalla.
- `Board/Input.h`: lectura del encoder (ruleta) y del botón.
- `Board/Logger.*`: gestión del almacenamiento en la tarjeta SD.
- `Effectors/Motor.h`: control del motor.
- `Sensors/BME280.h`, `Sensors/Plantower.h`: manejo de sensores.
- `Scenes/*`: cada archivo es una pantalla (Home, Flujo, Hora, Espera, Dashboard, etc.).

### Notas operativas y recomendaciones
- Sensor de partículas (Plantower) aumenta el consumo de batería: si necesitas ahorrar energía, desactívalo en la pantalla correspondiente.
- Si la tarjeta SD no está presente o hay un error, el sistema lo indica en pantalla y por Serial; los datos no se guardarán correctamente.
- Asegúrate de que la batería esté cargada antes de lanzarte a largas sesiones; el porcentaje se muestra en pantalla.
- Al desmontar la SD, espera a que no haya actividad de escritura (idealmente apagar el equipo)

### Problemas comunes visibles desde la interfaz
- La pantalla muestra "SD: ERR!" o similar → revisar tarjeta SD o contactos.
- Valores de PM = -1 → el sensor Plantower está desactivado o la lectura falló.
- El dispositivo no arranca correctamente → revisar alimentación y conexiones.

### Resumen corto
EOLO es básicamente: una pantalla + controles (encoder) que te permiten configurar y lanzar capturas que miden ambiente y partículas, controlando un motor para obtener un flujo objetivo y guardando los datos en una SD con marcas de tiempo.