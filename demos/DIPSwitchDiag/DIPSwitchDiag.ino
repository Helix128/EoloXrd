#include <Arduino.h>

#if !defined(EOLO_MODEL_DRON) && !defined(EOLO_MODEL_STANDARD) && \
    !defined(EOLO_MODEL_EXPRESS) && !defined(EOLO_MODEL_EXPRESS_LEGACY)
#define EOLO_MODEL_DRON
#endif

#include "../EoloDemoPinout.h"
#include <Eolo/Core/Input/CaptureSwitchLogic.h>

/*
  ========================================================================
   Diagnóstico Exclusivo de DIP Switches — EOLO Dron
  ========================================================================
   Pines monitoreados ÚNICAMENTE:
     - DIP 1: GPIO 32 (WAIT_SW0)
     - DIP 2: GPIO 33 (WAIT_SW1)
     - DIP 3: GPIO 14 (DURATION_SW0)
     - DIP 4: GPIO 13 (DURATION_SW1)

   Seguridad:
     - Motor (GPIO 26) se mantiene forzado en OUTPUT LOW (apagado).

   Lógica de hardware esperada:
     - Switch en OFF (abierto) -> Pin en HIGH (3.3V) -> Bit = 0
     - Switch en ON  (cerrado) -> Pin en LOW  (0.0V) -> Bit = 1

   Comandos por Serial (115200 baud):
     'r' : Reimprime el reporte completo
     'p' : Alterna entre INPUT_PULLUP e INPUT (para probar pull-ups de la placa)
     'h' : Muestra la ayuda
  ========================================================================
*/

struct SwitchPinInfo {
    const char* name;
    int pin;
    int lastLevel;
    unsigned long lastChangeMs;
};

static SwitchPinInfo g_switches[4] = {
    {"DIP 1 (WAIT_SW0    / GPIO 32)", DIP_PIN_1, -1, 0},
    {"DIP 2 (WAIT_SW1    / GPIO 33)", DIP_PIN_2, -1, 0},
    {"DIP 3 (DURATION_SW0 / GPIO 14)", DIP_PIN_3, -1, 0},
    {"DIP 4 (DURATION_SW1 / GPIO 13)", DIP_PIN_4, -1, 0}
};

static bool g_useInternalPullup = true;
static const unsigned long DEBOUNCE_MS = 35;
static unsigned long g_lastHeartbeatMs = 0;

void configureSwitches() {
    // 1. Asegurar actuadores apagados
    pinMode(MOTOR_PWM_PIN_0, OUTPUT);
    digitalWrite(MOTOR_PWM_PIN_0, LOW);

    // 2. Configurar ÚNICAMENTE los 4 pines de los switches
    for (int i = 0; i < 4; i++) {
        if (g_switches[i].pin >= 0) {
            pinMode(g_switches[i].pin, g_useInternalPullup ? INPUT_PULLUP : INPUT);
        }
    }
}

void printHeader() {
    Serial.println();
    Serial.println(F("╔══════════════════════════════════════════════════════════════════════╗"));
    Serial.println(F("║          DIAGNÓSTICO EXCLUSIVO: DIP SWITCHES (EOLO DRON)             ║"));
    Serial.println(F("╠══════════════════════════════════════════════════════════════════════╣"));
    Serial.printf("║  DIP 1 (WAIT_SW0):     GPIO %-2d                                        ║\n", DIP_PIN_1);
    Serial.printf("║  DIP 2 (WAIT_SW1):     GPIO %-2d                                        ║\n", DIP_PIN_2);
    Serial.printf("║  DIP 3 (DURATION_SW0): GPIO %-2d                                        ║\n", DIP_PIN_3);
    Serial.printf("║  DIP 4 (DURATION_SW1): GPIO %-2d                                        ║\n", DIP_PIN_4);
    Serial.println(F("╠══════════════════════════════════════════════════════════════════════╣"));
    Serial.printf( "║  Modo pines: %-20s  (Usa 'p' para alternar)         ║\n",
                   g_useInternalPullup ? "INPUT_PULLUP" : "INPUT (simple)");
    Serial.println(F("║  Comandos: 'r' reporte | 'p' toggle pullup | 'h' ayuda               ║"));
    Serial.println(F("╚══════════════════════════════════════════════════════════════════════╝"));
    Serial.println();
}

void printFullReport() {
    int levels[4];
    uint8_t bits[4];
    for (int i = 0; i < 4; i++) {
        levels[i] = (g_switches[i].pin >= 0) ? digitalRead(g_switches[i].pin) : HIGH;
        bits[i] = (levels[i] == LOW) ? 1 : 0;
    }

    uint8_t waitCode = (bits[0] << 0) | (bits[1] << 1);
    uint8_t durationCode = (bits[2] << 0) | (bits[3] << 1);
    CaptureSwitchSelection sel = CaptureSwitchLogic::decode(waitCode, durationCode);

    Serial.println(F("┌──────────────────────────────────┬──────┬───────┬─────┬──────────────────────────┐"));
    Serial.println(F("│ Switch                           │ GPIO │ Nivel │ Bit │ Estado Físico            │"));
    Serial.println(F("├──────────────────────────────────┼──────┼───────┼─────┼──────────────────────────┤"));
    for (int i = 0; i < 4; i++) {
        Serial.printf( "│ %-32s │ %4d │ %-5s │  %d  │ %-24s │\n",
                       g_switches[i].name,
                       g_switches[i].pin,
                       levels[i] == LOW ? "LOW" : "HIGH",
                       bits[i],
                       levels[i] == LOW ? "ON  / CERRADO (GND)" : "OFF / ABIERTO (3.3V)");
    }
    Serial.println(F("├──────────────────────────────────┴──────┴───────┴─────┴──────────────────────────┤"));
    Serial.printf( "│ Código Espera:   0b%d%d (dec %u) -> %-49s│\n",
                   bits[1], bits[0], waitCode, CaptureSwitchLogic::waitDescription(waitCode));
    Serial.printf( "│ Código Duración: 0b%d%d (dec %u) -> %-49s│\n",
                   bits[3], bits[2], durationCode, CaptureSwitchLogic::durationDescription(durationCode));
    Serial.printf( "│ Modo Resultante: %-64s│\n", CaptureSwitchLogic::modeDescription(sel));
    Serial.printf( "│ shouldStart=%-3s | wait=%lus | dur=%lus%s│\n",
                   sel.shouldStart ? "SI" : "NO",
                   (unsigned long)sel.waitSeconds,
                   (unsigned long)(sel.infiniteDuration ? 999999UL : sel.durationSeconds),
                   sel.infiniteDuration ? " (infinita)       " : "                 ");
    Serial.println(F("└──────────────────────────────────────────────────────────────────────────────────┘"));
    Serial.println();
}

void checkSwitches() {
    unsigned long now = millis();
    bool anyChanged = false;

    for (int i = 0; i < 4; i++) {
        if (g_switches[i].pin < 0) continue;
        int currentLevel = digitalRead(g_switches[i].pin);

        if (currentLevel != g_switches[i].lastLevel) {
            if (now - g_switches[i].lastChangeMs >= DEBOUNCE_MS) {
                int oldLevel = g_switches[i].lastLevel;
                g_switches[i].lastLevel = currentLevel;
                g_switches[i].lastChangeMs = now;
                anyChanged = true;

                uint8_t oldBit = (oldLevel == LOW) ? 1 : 0;
                uint8_t newBit = (currentLevel == LOW) ? 1 : 0;

                Serial.println(F("────────────────────────────────────────────────────────────────────────────────"));
                Serial.printf( "[CAMBIO] %s:\n", g_switches[i].name);
                Serial.printf( "         Nivel: %s -> %s  |  Bit: %d -> %d  (%s)\n",
                               oldLevel == LOW ? "LOW (0.0V / GND)" : "HIGH (3.3V)",
                               currentLevel == LOW ? "LOW (0.0V / GND)" : "HIGH (3.3V)",
                               oldBit, newBit,
                               newBit == 1 ? "ON / CERRADO" : "OFF / ABIERTO");
                Serial.println(F("────────────────────────────────────────────────────────────────────────────────"));
            }
        }
    }

    if (anyChanged) {
        delay(10);
        printFullReport();
    }
}

void printHeartbeat() {
    int levels[4];
    for (int i = 0; i < 4; i++) {
        levels[i] = (g_switches[i].pin >= 0) ? digitalRead(g_switches[i].pin) : HIGH;
    }
    int s1 = levels[0] == LOW ? 1 : 0;
    int s2 = levels[1] == LOW ? 1 : 0;
    int s3 = levels[2] == LOW ? 1 : 0;
    int s4 = levels[3] == LOW ? 1 : 0;

    uint8_t waitCode = (s1 << 0) | (s2 << 1);
    uint8_t durationCode = (s3 << 0) | (s4 << 1);
    CaptureSwitchSelection sel = CaptureSwitchLogic::decode(waitCode, durationCode);

    Serial.printf("[ESTADO] SW1(G%d)=%s SW2(G%d)=%s SW3(G%d)=%s SW4(G%d)=%s | Modo: %s\n",
                  DIP_PIN_1, s1 ? "ON " : "OFF",
                  DIP_PIN_2, s2 ? "ON " : "OFF",
                  DIP_PIN_3, s3 ? "ON " : "OFF",
                  DIP_PIN_4, s4 ? "ON " : "OFF",
                  CaptureSwitchLogic::modeDescription(sel));
}

void processSerial() {
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == 'r' || c == 'R') {
            printFullReport();
        } else if (c == 'p' || c == 'P') {
            g_useInternalPullup = !g_useInternalPullup;
            configureSwitches();
            Serial.printf("\n>> Modo de pines cambiado a: %s\n\n",
                          g_useInternalPullup ? "INPUT_PULLUP (pull-up interno ESP32)" : "INPUT (simple / requiere pull-up externo)");
            printFullReport();
        } else if (c == 'h' || c == 'H' || c == '?') {
            printHeader();
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    configureSwitches();
    delay(50);

    // Leer estado inicial
    for (int i = 0; i < 4; i++) {
        if (g_switches[i].pin >= 0) {
            g_switches[i].lastLevel = digitalRead(g_switches[i].pin);
            g_switches[i].lastChangeMs = millis();
        }
    }

    printHeader();
    printFullReport();
    g_lastHeartbeatMs = millis();
}

void loop() {
    processSerial();
    checkSwitches();

    if (millis() - g_lastHeartbeatMs >= 2000) {
        g_lastHeartbeatMs = millis();
        printHeartbeat();
    }
}
