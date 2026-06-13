# Arquitectura modular EOLO

## Veredicto general

La idea de modularizar EOLO en capas reutilizables es buena e inteligente, siempre que se haga de forma incremental y con reglas claras de dependencia.

No conviene hacer un refactor masivo inmediato que solo mueva archivos a carpetas nuevas. Eso sería riesgoso y puede terminar en un monolito con rutas distintas.

La mejora real aparece cuando cada módulo queda menos acoplado a:

- `Context`
- `Config.h`
- hardware específico
- servicios globales
- estado compartido innecesario

EOLO debería apuntar a una arquitectura limpia, firmware-friendly, testeable y honesta. No necesita verse “enterprise”; necesita ser mantenible y reutilizable.

---

## Objetivo

Permitir que firmware real, demos y tests usen la misma lógica común sin duplicarla ni depender de todo el sistema EOLO.

Ejemplo principal:

La demo de calibración/control de flujo debería usar el mismo controlador predictivo que el sistema real, pero sin arrastrar:

- `Context`
- WiFi
- SD
- webserver
- sensores reales
- servicios completos
- UI
- estado global de la aplicación

La demo debería usar el controlador core real, no una copia ni una versión paralela.

---

## Estructura propuesta inicial

Como primer paso, conviene usar `include/Eolo/...`, porque es menos traumático que crear librerías PlatformIO completas desde el inicio.

```txt
include/
  Eolo/
    Types/
    Core/
    Hardware/
    Services/

src/
  App/              # opcional futuro
  Board/
  Data/
  Drawing/
  Utility/
```

A futuro, si las fronteras quedan estables, se puede migrar a librerías PlatformIO:

```txt
lib/
  EoloTypes/
  EoloCore/
  EoloHardware/
  EoloServices/
```

No conviene pasar a `lib/` demasiado temprano. Si se hace antes de limpiar dependencias, puede terminar naciendo un `lib/EoloEverything`, que sería solo el mismo monolito con otra dirección.

---

# Capas

## 1. `Eolo/Types`

Contratos y estructuras simples compartidas.

Debe depender de casi nada. Idealmente solo C/C++ estándar:

```cpp
#include <stdint.h>
```

Ejemplos posibles:

```txt
include/Eolo/Types/FlowData.h
include/Eolo/Types/NTCData.h
include/Eolo/Types/BatteryData.h
include/Eolo/Types/CaptureConfig.h
include/Eolo/Types/SensorStatus.h
include/Eolo/Types/MotorCommand.h
```

Reglas:

- No incluir `Context.h`.
- No incluir `ArduinoJson`.
- Evitar `Arduino.h` si no es necesario.
- No leer macros globales desde `Config.h`, salvo casos muy justificados.
- Mantener structs simples, explícitos y portables.

Ejemplo:

```cpp
#pragma once

#include <stdint.h>

struct MotorCommand {
    uint8_t pwm;
    bool enabled;
};
```

---

## 2. `Eolo/Core`

Algoritmos puros y lógica reutilizable independiente del hardware.

Puede depender de `Eolo/Types`.

Ejemplos:

```txt
include/Eolo/Core/Flow/SmartFlowController.h
include/Eolo/Core/Flow/CalibrationCurve.h
include/Eolo/Core/Filters/ExponentialFilter.h
include/Eolo/Core/Math/Clamp.h
```

Reglas:

- No depender de `Context`.
- No depender de WiFi.
- No depender de SD.
- No depender de Preferences.
- No depender de WebServer.
- No depender de display.
- No depender de modem.
- Idealmente no depender de `Arduino.h`.
- Configuración por structs/parámetros, no por estado global.
- Debe poder usarse desde demos y tests sin inicializar hardware EOLO.

Ejemplo correcto:

```cpp
#include <Eolo/Core/Flow/SmartFlowController.h>

SmartFlowStatus status = controller.update(
    nowMs,
    currentPwm,
    targetFlow,
    measuredFlow,
    dtSeconds,
    maxPwm
);
```

Aunque sería preferible usar un struct de entrada para evitar funciones con demasiados parámetros.

Ejemplo recomendado:

```cpp
SmartFlowInput input {
    .nowMs = nowMs,
    .currentPwm = currentPwm,
    .targetFlow = targetFlow,
    .measuredFlow = measuredFlow,
    .dtSeconds = dtSeconds
};

SmartFlowStatus status = controller.update(input);
```

---

## 3. `Eolo/Hardware`

Drivers y adaptadores de hardware reutilizables.

Puede depender de `Eolo/Types` y, si tiene sentido, de `Eolo/Core`.

Ejemplos:

```txt
include/Eolo/Hardware/Flow/AFM07FlowSensor.h
include/Eolo/Hardware/Flow/FS3000FlowSensor.h
include/Eolo/Hardware/Motor/MotorPwm.h
include/Eolo/Hardware/Sensors/NTC.h
include/Eolo/Hardware/Bus/I2CBus.h
```

Reglas:

- Puede incluir `Arduino.h`, `Wire.h`, etc.
- No debería depender de `Context`.
- Configuración por constructor, `begin(config)` o structs.
- Evitar leer demasiadas macros globales desde `Config.h`.
- Debe ser reusable entre firmware real, demos y posibles tests con mocks.

Ejemplo:

```cpp
MotorPwm motor(pin, channel, frequency);
motor.begin();
motor.setPwm(128);
```

---

## 4. `Eolo/Services`

Servicios reutilizables de nivel más alto.

Puede depender de:

- `Eolo/Types`
- `Eolo/Core`
- `Eolo/Hardware`

Ejemplos:

```txt
include/Eolo/Services/Logging/LogService.h
include/Eolo/Services/RTC/RTCNetworkSync.h
include/Eolo/Services/Capture/CaptureController.h
include/Eolo/Services/Flow/MotorCaptureControl.h
```

Reglas:

- Puede usar Arduino, SD, Preferences, etc.
- Debe recibir dependencias explícitamente cuando sea posible.
- Idealmente no depender directamente de `Context`.
- Durante la migración, esta capa puede ser más flexible que `Core`.
- No debe convertirse en un nuevo lugar para meter todo.

Ejemplo sano:

```cpp
MotorCaptureControl control(
    flowSensor,
    motor,
    smartFlowController,
    logger
);
```

Ejemplo menos sano:

```cpp
MotorCaptureControl control(context);
```

No siempre será posible evitarlo al inicio, pero debería ser una deuda técnica explícita.

---

## 5. `src/` como aplicación EOLO

`src/` queda como ensamblador del producto real.

Aquí viven:

```txt
src/Data/Context.h
src/main.cpp
src/Board/HeadlessSetupServer.h
src/Drawing/...
src/Utility/...
```

`Context` no debería convertirse en librería al inicio.

Su rol es conectar:

- componentes
- servicios
- sesión
- UI
- hardware
- configuración real
- estado global inevitable de la aplicación

`Context` es composición de aplicación, no core reutilizable.

---

# Regla de dependencia

La idea general es que las capas más reutilizables no conozcan las capas más concretas.

Una forma más precisa de verlo sería:

```txt
App / Context
  ↓
Services
  ↓
Hardware      Core
     ↓          ↓
        Types
```

`Types` está abajo como base común.

`Core` debería ser lo más puro posible.

`Hardware` puede usar `Types`, y a veces `Core`, pero idealmente `Core` jamás sabe que existe `Hardware`.

Prohibido dentro de `Eolo/Core`:

```cpp
#include "../../../src/Data/Context.h"
#include "../../../src/Config.h"
#include <WiFi.h>
```

Correcto:

```cpp
#include <Eolo/Core/Flow/SmartFlowController.h>
#include "Context.h"
```

Es decir:

La app puede conocer el core, pero el core no puede conocer la app.

---

# `Data` vs `Types`

Preferir `Eolo/Types` para estructuras simples y contratos.

`Data` es ambiguo y puede transformarse en cajón desastre.

Usar `Data` solo si representa estado/dominio más complejo, por ejemplo:

```txt
Eolo/Data/Session.h
Eolo/Data/CalibrationTable.h
Eolo/Data/UiSnapshot.h
```

Pero inicialmente se recomienda evitar `Eolo/Data` y usar:

```txt
Eolo/Types
Eolo/Core
Eolo/Hardware
Eolo/Services
```

---

# Primer paso recomendado

Mover solo el controlador predictivo:

```txt
src/Data/SmartFlowController.h
```

a:

```txt
include/Eolo/Core/Flow/SmartFlowController.h
```

Actualizar includes:

```cpp
#include <Eolo/Core/Flow/SmartFlowController.h>
```

en:

- `src/Data/MotorCaptureControl.h`
- `demos/DynamicMotorCalibration/DynamicMotorCalibration.ino`
- tests relacionados

Este cambio prueba la arquitectura con bajo riesgo y beneficio inmediato.

Pero no basta con mover el archivo.

El cambio solo vale la pena si el controlador queda sin dependencias innecesarias como:

```cpp
#include "Context.h"
#include "Config.h"
#include <Arduino.h>
```

Si solo se mueve el archivo sin limpiar dependencias, el desorden sigue igual pero con una ruta más bonita.

---

# Relación con PID/predictor actual

El sistema real ya fue actualizado para usar `SmartFlowController` como controlador predictivo ligero.

Características del modelo:

- Ring buffer fijo de 24 muestras.
- Sin `malloc`.
- Sin `vector`.
- Sin regresión pesada.
- Estima ganancia runtime usando `deltaFlow / deltaPwm`.
- Puede interpolar entre muestras históricas cuando hay confianza.
- Usa fallback PID cuando el modelo todavía no es confiable.
- Expone diagnóstico:
  - `mode`
  - `estimatedGain`
  - `confidence`
  - `modelValid`

Modos:

```txt
PID
GAIN_PREDICT
INTERPOLATE
BOOST
```

La demo debería usar exactamente ese mismo controlador core.

No debería existir:

- lógica paralela
- copia del controlador
- predictor alternativo solo para demo
- PID duplicado en otro archivo

---

# API recomendada para `SmartFlowController`

Una API con muchos parámetros sueltos puede volverse frágil:

```cpp
controller.update(nowMs, currentPwm, targetFlow, measuredFlow, dtSeconds, maxPwm);
```

El problema es que, con el tiempo, puede crecer y quedar así:

```cpp
controller.update(nowMs, currentPwm, targetFlow, measuredFlow, dtSeconds, maxPwm, minPwm, gain, confidence, boost);
```

Eso es difícil de leer y fácil de romper.

Mejor usar structs:

```cpp
struct SmartFlowConfig {
    uint8_t minPwm;
    uint8_t maxPwm;
    float kp;
    float ki;
    float kd;
    float minConfidence;
};

struct SmartFlowInput {
    uint32_t nowMs;
    uint8_t currentPwm;
    float targetFlow;
    float measuredFlow;
    float dtSeconds;
};

struct SmartFlowStatus {
    uint8_t nextPwm;
    float estimatedGain;
    float confidence;
    bool modelValid;
    SmartFlowMode mode;
};
```

Uso:

```cpp
SmartFlowConfig config {
    .minPwm = 0,
    .maxPwm = 255,
    .kp = 0.8f,
    .ki = 0.1f,
    .kd = 0.0f,
    .minConfidence = 0.65f
};

SmartFlowController controller(config);

SmartFlowInput input {
    .nowMs = nowMs,
    .currentPwm = currentPwm,
    .targetFlow = targetFlow,
    .measuredFlow = measuredFlow,
    .dtSeconds = dtSeconds
};

SmartFlowStatus status = controller.update(input);
```

Esto es más legible, más difícil de usar mal y más fácil de extender.

---

# Cuándo mover a `lib/`

Mover a `lib/` cuando las fronteras estén claras y los módulos sean realmente reutilizables.

Ejemplo futuro:

```txt
lib/EoloCore/
  library.json
  src/Eolo/Core/Flow/SmartFlowController.h

lib/EoloHardware/
  library.json
  src/Eolo/Hardware/Flow/AFM07FlowSensor.h

lib/EoloServices/
  library.json
  src/Eolo/Services/Logging/LogService.h
```

No hacer esto antes de reducir dependencias cruzadas.

Si todavía hay includes como estos:

```cpp
#include "../../src/Data/Context.h"
#include "../../src/Config.h"
```

entonces todavía no es momento de pasarlo a `lib/`.

---

# Criterio para decidir si algo merece módulo común

Sí conviene mover si cumple al menos 3 de 4:

- Se usaría en demo, test o futuro target.
- No necesita `Context`.
- Puede configurarse por parámetros/structs, no solo por macros globales.
- Tiene o puede tener tests propios.

No conviene mover todavía si:

- Solo existe para una pantalla/ruta específica.
- Depende de medio sistema.
- Requiere `Context` para funcionar.
- Su API cambia con cada feature nueva.
- Es solo una función wrapper que no aporta claridad.

---

# Orden de migración sugerido

## Fase 1: Core puro

Mover primero:

- `SmartFlowController`
- filtros
- curvas de calibración matemáticas
- helpers numéricos

Bajo riesgo, alto beneficio.

Objetivo:

```txt
Core debe compilar sin Context, WiFi, SD, display ni sensores.
```

---

## Fase 2: Types compartidos

Mover:

- `FlowData`
- `NTCData`
- estados de sensores
- comandos de motor
- configs simples

Esto reduce acoplamiento entre capas.

---

## Fase 3: Hardware reusable

Mover o crear adaptadores para:

- sensores de flujo
- NTC
- motor PWM
- bus I2C

Riesgo medio.

Requiere limpiar `Config.h` gradualmente.

---

## Fase 4: Services

Mover o limpiar:

- logging
- RTC sync
- upload
- capture
- motor capture/control

Riesgo más alto.

Hacer solo cuando `Core` y `Hardware` estén más estables.

---

# Qué NO hacer

- No mover todo de golpe.
- No crear `lib/EoloEverything`.
- No meter `Context` en `Eolo/Core`.
- No usar carpetas nuevas como sustituto de desacoplamiento real.
- No duplicar lógica entre demo y firmware real.
- No crear abstracciones solo porque “se ven limpias”.
- No crear funciones microscópicas si no agregan significado.
- No mezclar español e inglés en nombres de código.
- No dejar comentarios que solo repiten lo que ya dice el código.

---

# Revamp anti-código IA

La revisión de código debería prevenir que EOLO parezca generado por IA.

No porque usar IA sea malo, sino porque el código generado por IA suele dejar síntomas claros:

- demasiadas funciones “por si acaso”
- nombres genéricos
- comentarios obvios
- capas artificiales
- helpers que solo se usan una vez
- abstracciones antes de tener el problema real
- mezcla de español/inglés
- funciones que hacen casi lo mismo con nombres distintos
- clases tipo `Manager`, `Handler`, `Processor`, `Controller`, todas medio iguales

La meta es que EOLO parezca firmware escrito por alguien que entiende el sistema.

---

## Regla brutal

Para cada función, clase o archivo, preguntar:

```txt
¿Esto existe porque simplifica el sistema real,
o porque se veía ordenado?
```

Si no simplifica nada, se borra, se fusiona o se deja local.

Ejemplo de olor a IA:

```cpp
void initializeFlowSystem();
void setupFlowSystem();
void configureFlowSensor();
void prepareFlowSensor();
void beginFlowMeasurement();
```

Muchas veces eso debería ser simplemente:

```cpp
flowSensor.begin(config);
flowControl.begin(config);
```

O incluso:

```cpp
setupFlowControl();
```

si el detalle no merece más abstracción.

---

# Idioma del código

Recomendación fuerte:

- Código en inglés.
- Comentarios en inglés o español, pero con consistencia.
- Documentación interna en español si resulta más cómodo para el equipo.

Nombres recomendados:

```cpp
SmartFlowController
FlowSensor
MotorCommand
CaptureConfig
SensorStatus
estimatedGain
targetFlow
measuredFlow
```

Evitar mezcla:

```cpp
calcularFlow()
estadoSensor
targetCaudal
flujoMedido
```

La mezcla español/inglés hace que el proyecto se vea menos consistente, aunque funcione bien.

---

# Comentarios

Usar pocos comentarios, pero útiles.

Buen comentario:

```cpp
// Ignore unstable samples during motor ramp-up.
```

Mal comentario:

```cpp
// Set the PWM value.
motor.setPwm(pwm);
```

El comentario bueno explica por qué.

El comentario malo repite qué hace el código.

---

# Auditoría de funciones

Marcar cada función como una de estas:

```txt
KEEP       Se usa, tiene propósito claro.
MERGE      Es muy chica o duplica otra.
DELETE     No aporta, no se usa o es legacy.
LOCAL      No merece estar en una clase/header.
RENAME     Hace algo útil pero su nombre miente.
```

Ejemplo problemático:

```cpp
float calculateAdjustedPWM(...);
float computeCorrectedPWM(...);
float getNewPWM(...);
```

Probablemente una sola debería sobrevivir:

```cpp
uint8_t computeNextPwm(...);
```

---

# Limpiar nombres genéricos

Evitar nombres como:

```txt
Manager
Handler
Processor
Data
Utils
Helper
doThing()
process()
handle()
run()
```

Preferir nombres de dominio real:

```txt
MotorCaptureControl
SmartFlowController
FlowCalibrationCurve
NtcTemperatureReader
CaptureSession
```

`Manager` casi siempre significa que la responsabilidad de la clase no está clara.

---

# Reducir archivos cajón desastre

Cuidado con carpetas como:

```txt
Utility/
Helpers/
Common/
Data/
Manager/
```

No son malas por sí mismas, pero suelen atraer código sin dueño claro.

Mejor organizar por dominio:

```txt
Core/Flow/
Core/Filters/
Core/Math/
Hardware/Flow/
Hardware/Motor/
Services/Capture/
Services/Logging/
```

---

# Funciones cortas, pero no microscópicas

No conviene caer en el extremo de hacer una función para cada línea.

Ejemplo IA-ish:

```cpp
bool isFlowValid(float flow) {
    return flow > 0;
}

bool isPwmValid(int pwm) {
    return pwm >= 0 && pwm <= 255;
}

bool isTargetValid(float target) {
    return target > 0;
}
```

Mejor:

```cpp
bool isValidInput(float targetFlow, float measuredFlow, int pwm) {
    return targetFlow > 0.0f &&
           measuredFlow >= 0.0f &&
           pwm >= 0 &&
           pwm <= 255;
}
```

O incluso dejarlo inline si solo se usa una vez.

---

# Política de eliminación

Regla simple:

```txt
Si una función privada tiene una sola línea,
se usa una sola vez,
y su nombre no agrega significado,
se elimina.
```

Ejemplo innecesario:

```cpp
float getTargetFlow() {
    return _targetFlow;
}
```

Si es privado y solo se usa adentro, probablemente sobra.

Pero esta sí puede valer:

```cpp
bool shouldUsePrediction(const FlowModelState& model) {
    return model.valid && model.confidence >= _config.minConfidence;
}
```

Porque encapsula una decisión de dominio.

---

# Sanear `Config.h`

`Config.h` debería quedar como configuración de la app real, no como dios global del universo.

Malo:

```cpp
SmartFlowController controller;
controller.update(flow, DEFAULT_MAX_PWM, FLOW_GAIN_ALPHA, FLOW_MIN_CONFIDENCE);
```

Mejor:

```cpp
SmartFlowConfig config = {
    .maxPwm = FLOW_MAX_PWM,
    .gainAlpha = FLOW_GAIN_ALPHA,
    .minConfidence = FLOW_MIN_CONFIDENCE
};

SmartFlowController controller(config);
```

Así la demo puede usar otra config sin tragarse todo EOLO.

---

# Buscar duplicados

Comandos útiles:

```bash
grep -R "SmartFlow" src include demos
grep -R "PID" src include demos
grep -R "Flow" src/Data src/Utility include
grep -R "Manager" src include
grep -R "Handler" src include
grep -R "Helper" src include
grep -R "Utils" src include
```

Buscar especialmente:

- lógica repetida
- funciones viejas que ya no se usan
- demos con copias del controlador
- constantes duplicadas
- nombres inconsistentes
- controladores alternativos
- rutas que todavía incluyen `Context` desde `Core`

---

# Checklist EOLO refactor

Para cada cambio de arquitectura:

```txt
[ ] No agrega dependencia a Context desde Core.
[ ] No agrega dependencia innecesaria a Arduino.h.
[ ] No duplica lógica ya existente.
[ ] No crea clases Manager/Handler genéricas sin razón.
[ ] No agrega funciones wrapper que no aportan significado.
[ ] Usa inglés consistente en nombres de código.
[ ] La demo y el firmware usan el mismo controlador.
[ ] El cambio reduce acoplamiento real, no solo mueve archivos.
[ ] La configuración entra por struct/parámetros cuando corresponde.
[ ] Las funciones nuevas tienen una razón clara para existir.
```

---

# Propuesta final

La estrategia correcta para EOLO sería trabajar en dos tracks paralelos.

## Track 1: Modularización real

- mover `SmartFlowController`
- separar `Types`
- reducir dependencia de `Config.h`
- evitar `Context` en `Core`
- hacer que demos y firmware real usen el mismo código
- preparar el camino para tests

## Track 2: Limpieza de estilo

- idioma consistente
- borrar funciones decorativas
- fusionar duplicados
- renombrar clases genéricas
- comentarios solo cuando expliquen decisiones
- reducir helpers innecesarios
- evitar abstracciones falsas

---

# Decisión final

La arquitectura modular es buena dirección.

Estrategia recomendada:

1. Empezar con:
   - `include/Eolo/Types`
   - `include/Eolo/Core`
   - `include/Eolo/Hardware`
   - `include/Eolo/Services`

2. Migrar primero:
   - `SmartFlowController` a `Eolo/Core/Flow`

3. Mantener:
   - `Context` como ensamblador de aplicación

4. Migrar componentes comunes de forma incremental.

5. Solo pasar a `lib/` cuando cada módulo tenga límites claros.

6. Revisar el estilo del código en paralelo para evitar:
   - código artificial
   - funciones sin propósito
   - nombres genéricos
   - inconsistencias de idioma
   - duplicación entre demo y firmware real

En resumen:

EOLO no debería parecer un proyecto “ordenado por carpetas”.

Debería parecer un sistema donde cada pieza tiene una responsabilidad clara, dependencias limpias y una razón concreta para existir.

---

## Estado de implementación actual

Migración ejecutada en corte incremental:

- `SmartFlowController` vive en `include/Eolo/Core/Flow` y se comparte con firmware/demo/tests.
- `FlowMotorController` separa control PID/predictivo de flujo de `Context`; `MotorCaptureControl` queda como adaptador app.
- Tipos compartidos movidos a `include/Eolo/Types`: `FlowData`, `NTCData`, `PlantowerData`, `AnemometerData`, `CaptureSwitchData`, `Session`, `HeadlessSetupTypes`, `ModemTypes`, `LogTypes`.
- Matemática/parseo puro movido a Core: `PwmMath`, `NtcThermistor`, `BatteryMath`, `CaptureSwitchLogic`, `RtcTimeParser`, `CalibrationModel`.
- `SettingsStore` existe como contrato inicial en `include/Eolo/Services`, sin implementación concreta todavía.
- `Context`, `Components`, drivers Arduino, SD/WiFi/WebServer/Preferences y UI siguen fuera de Core.

Deuda explícita:

- `RtcTimeParser` y `Session` aún usan `RTClib::DateTime`; aceptado temporalmente por compatibilidad.
- `HeadlessMotorCalibration`, `CaptureController`, `LogService`, `UploadService` y hardware adapters todavía requieren cortes posteriores.
- `eolo_express_legacy` compila de nuevo y queda dentro de la validación local recomendada.
