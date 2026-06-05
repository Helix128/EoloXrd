/*
 * ============================================================
 *  EOLO DRON — Demo Secuencial  v5.2
 * ============================================================
 *  Plataforma : ESP32 WROOM32D
 *
 *  Cambios v5.1 respecto a v5:
 *   - PIN_SD_CS     : 25 → 5   (ajuste hardware usuario)
 *   - AFM07_ADDR    : 0x01 → 0x02
 *   - AFM07_BAUD    : 9600 → 4800
 *   - MOTOR_MAX_LEDC: 212 → 255 (sin threshold para pruebas)
 *   - VIN_Motor     : 6V → 7.4V (cálculo V_avg actualizado)
 *   - setMotorDuty(): ahora rampa lenta para evitar brownout
 *       Sube/baja de a MOTOR_RAMP_STEP cada MOTOR_RAMP_DELAY_MS
 *       0→255: ~1 segundo. Ajustar defines según necesidad.
 *
 *  Circuito SI2300 (low-side, lógica directa):
 *   7.4V ─── Motor (+) / Motor (−) ─── Drain SI2300
 *   GPIO26 ─[1kΩ]── Gate
 *          └─[100kΩ]─ GND  (pull-down boot seguro)
 *   Source ─── GND
 *
 *  Circuito NTC (hardware pendiente):
 *   3.3V ── NTC(10kΩ@25°C) ── GPIO34 ── R(10kΩ) ── GND
 * ============================================================
 */

#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_NeoPixel.h>
#include <RTClib.h>
#include <Adafruit_BME280.h>
#include <math.h>

// ── Pines ────────────────────────────────────────────────────
#define PIN_SDA          21
#define PIN_SCL          22
#define PIN_DIP1         32
#define PIN_DIP2         33
#define PIN_DIP3         14
#define PIN_DIP4         13
#define PIN_SD_CS         5    // ← usuario
#define PIN_NEO          27
#define NEO_COUNT         1
#define PIN_RS485_RX     16
#define PIN_RS485_TX     17
#define PIN_RS485_DERE    4
#define PIN_MOTOR_PWM    26
#define PIN_NTC          34

// ── Motor ────────────────────────────────────────────────────
#define MOTOR_PWM_CH         0
#define MOTOR_PWM_FREQ   20000
#define MOTOR_PWM_RES        8
#define MOTOR_MAX_LEDC     255    // Sin threshold — ajustar si es necesario
#define MOTOR_VIN          7.4F  // ← usuario: VIN del riel del motor
#define MOTOR_RAMP_STEP      2   // Pasos LEDC por incremento de rampa
#define MOTOR_RAMP_DELAY_MS  8   // ms entre pasos (0→255 ≈ 1 segundo)

// ── AFM07 ────────────────────────────────────────────────────
#define AFM07_ADDR       0x02   // ← usuario
#define AFM07_BAUD       4800   // ← usuario

// ── NTC ──────────────────────────────────────────────────────
#define NTC_R_FIXED     10000.0F
#define NTC_R25         10000.0F
#define NTC_BETA         3950.0F
#define NTC_T25           298.15F

// ── Estado interactivo ───────────────────────────────────────
int      g_motorLEDC  = 0;
int      g_motorInput = 0;
uint32_t g_lastLectura = 0;

// ── Objetos ──────────────────────────────────────────────────
Adafruit_NeoPixel neopixel(NEO_COUNT, PIN_NEO, NEO_RGB + NEO_KHZ800);
RTC_DS3231 rtc;
Adafruit_BME280 bme;

// ============================================================
//  UTILIDADES
// ============================================================
void printHeader(const char* t) {
    Serial.println(F("\n╔══════════════════════════════════════════╗"));
    Serial.print(  F("║  ")); Serial.print(t);
    Serial.println(F("\n╚══════════════════════════════════════════╝"));
}

uint16_t crc16Modbus(const uint8_t* d, uint8_t len) {
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= d[i];
        for (uint8_t j = 0; j < 8; j++)
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    return crc;
}

// Rampa lenta de g_motorLEDC → target para evitar brownout
void setMotorDuty(int target) {
    target = constrain(target, 0, MOTOR_MAX_LEDC);
    g_motorInput = target;

    if (target == g_motorLEDC) return;

    Serial.printf("\n  Rampa: %d → %d  (%.2fV → %.2fV)\n",
        g_motorLEDC, target,
        MOTOR_VIN * g_motorLEDC / 255.0F,
        MOTOR_VIN * target       / 255.0F);

    int step = (target > g_motorLEDC) ? MOTOR_RAMP_STEP : -MOTOR_RAMP_STEP;

    while (g_motorLEDC != target) {
        g_motorLEDC += step;
        // No pasar el objetivo
        if (step > 0 && g_motorLEDC > target) g_motorLEDC = target;
        if (step < 0 && g_motorLEDC < target) g_motorLEDC = target;

        ledcWrite(MOTOR_PWM_CH, g_motorLEDC);
        delay(MOTOR_RAMP_DELAY_MS);
    }

    Serial.printf("  ✓ LEDC=%d  V_avg≈%.2fV\n",
        g_motorLEDC, MOTOR_VIN * g_motorLEDC / 255.0F);
    Serial.println(F("  Input | LEDC | V_motor | Flujo(L/m) | NTC(°C) | BME(°C) | RTC(°C)"));
    Serial.println(F("  ------|------|---------|------------|---------|---------|--------"));
}

// ============================================================
//  AFM07 — Modbus RTU
// ============================================================
float afm07_leerFlujo() {
    while (Serial2.available()) Serial2.read();
    uint8_t t[8] = { AFM07_ADDR, 0x03, 0x00, 0x00, 0x00, 0x01, 0, 0 };
    uint16_t crc = crc16Modbus(t, 6);
    t[6] = crc & 0xFF; t[7] = crc >> 8;

    digitalWrite(PIN_RS485_DERE, HIGH);
    delayMicroseconds(500);
    Serial2.write(t, 8); Serial2.flush();
    delayMicroseconds(500);
    digitalWrite(PIN_RS485_DERE, LOW);

    uint32_t t0 = millis();
    while (Serial2.available() < 7 && millis() - t0 < 300) delay(1);
    if (Serial2.available() < 7) return -1.0F;

    uint8_t r[7];
    for (int i = 0; i < 7; i++) r[i] = Serial2.read();
    if (crc16Modbus(r, 5) != (r[5] | (uint16_t)r[6] << 8)) return -2.0F;
    return ((uint16_t)(r[3] << 8) | r[4]) / 100.0F;
}

// ============================================================
//  NTC — Beta equation
// ============================================================
float leerNTC() {
    int raw = analogRead(PIN_NTC);
    if (raw <= 10 || raw >= 4085) return -99.0F;
    float v    = raw * 3.3F / 4095.0F;
    // NTC en lado GND: V = 3.3 * R_NTC / (R_fija + R_NTC)
    // R_NTC = R_fija * V / (3.3 - V)
    float rNTC = NTC_R_FIXED * v / (3.3F - v);
    float tK   = 1.0F / (1.0F / NTC_T25 + logf(rNTC / NTC_R25) / NTC_BETA);
    return tK - 273.15F;
}

// ============================================================
//  DEMOS 1-9
// ============================================================
void demo_scanI2C() {
    printHeader("1. SCANNER I2C");
    uint8_t found = 0;
    for (uint8_t a = 1; a < 127; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  0x%02X", a);
            if (a==0x68||a==0x69) Serial.print("  ← DS3231 RTC");
            if (a==0x76||a==0x77) Serial.print("  ← BME280");
            if (a==0x57)          Serial.print("  ← DS3231 EEPROM");
            Serial.println(); found++;
        }
        delay(5);
    }
    if (!found) Serial.println("  ⚠  Ningún dispositivo.");
    else        Serial.printf("\n  Total: %d\n", found);
}

void demo_dipSwitch() {
    printHeader("2. DIP SWITCH");
    const int pins[4] = {PIN_DIP1, PIN_DIP2, PIN_DIP3, PIN_DIP4};
    uint8_t v = 0;
    Serial.print("  [ ");
    for (int i = 0; i < 4; i++) {
        bool on = (digitalRead(pins[i]) == LOW);
        if (on) v |= (1 << i);
        Serial.printf("SW%d:%s ", i+1, on ? "ON " : "OFF");
    }
    Serial.printf("]  dec=%d  bin=0b%d%d%d%d\n",
        v, (v>>3)&1, (v>>2)&1, (v>>1)&1, v&1);
}

void demo_sdInfo() {
    printHeader("3. TARJETA SD — CARACTERÍSTICAS");
    if (!SD.begin(PIN_SD_CS)) { Serial.println("  ✗ SD no inicializada."); return; }
    const char* tp[] = {"NONE","MMC","SDSC","SDHC/SDXC","UNKNOWN"};
    uint8_t t = SD.cardType();
    Serial.printf("  Tipo:      %s\n",   t<=4?tp[t]:tp[4]);
    Serial.printf("  Capacidad: %llu MB\n", SD.cardSize()  / (1024ULL*1024));
    Serial.printf("  Usado:     %llu MB\n", SD.usedBytes() / (1024ULL*1024));
}

void listarArchivos(File dir, uint8_t n) {
    while (true) {
        File f = dir.openNextFile(); if (!f) break;
        for (uint8_t i = 0; i < n; i++) Serial.print("    ");
        if (f.isDirectory()) {
            Serial.printf("[DIR]  %s\n", f.name());
            if (n < 2) listarArchivos(f, n+1);
        } else {
            Serial.printf("[FILE] %-28s  %8u B\n", f.name(), (uint32_t)f.size());
        }
        f.close();
    }
}

void demo_sdListar() {
    printHeader("4. TARJETA SD — ARCHIVOS");
    if (!SD.begin(PIN_SD_CS)) { Serial.println("  ✗ SD no disponible."); return; }
    File r = SD.open("/"); if (!r) return;
    listarArchivos(r, 0); r.close();
}

void demo_neopixel() {
    printHeader("5. NEOPIXEL WS2812B");
    struct { const char* n; uint8_t r,g,b; } c[] = {
        {"Rojo",255,0,0},{"Verde",0,255,0},{"Azul",0,0,255},{"Apagado",0,0,0}
    };
    for (auto& x : c) {
        Serial.printf("  → %s\n", x.n);
        neopixel.setPixelColor(0, neopixel.Color(x.r, x.g, x.b));
        neopixel.show(); delay(700);
    }
}

void demo_rtc() {
    printHeader("6. RTC DS3231");
    if (!rtc.begin(&Wire)) { Serial.println("  ✗ DS3231 no encontrado."); return; }
    if (rtc.lostPower()) {
        Serial.println("  ⚠  Ajustando con hora de compilación...");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    DateTime now = rtc.now();
    const char* d[] = {"Dom","Lun","Mar","Mie","Jue","Vie","Sab"};
    Serial.printf("  Fecha:    %s %04d-%02d-%02d\n",
        d[now.dayOfTheWeek()], now.year(), now.month(), now.day());
    Serial.printf("  Hora:     %02d:%02d:%02d\n",
        now.hour(), now.minute(), now.second());
    Serial.printf("  Temp RTC: %.2f °C\n", rtc.getTemperature());
}

void demo_bme280() {
    printHeader("7. SENSOR BME280");
    if (!bme.begin(0x76) && !bme.begin(0x77)) {
        Serial.println("  ✗ BME280 no encontrado."); return;
    }
    Serial.printf("  Temperatura:  %.2f °C\n",  bme.readTemperature());
    Serial.printf("  Humedad:      %.2f %%\n",  bme.readHumidity());
    Serial.printf("  Presión:      %.2f hPa\n", bme.readPressure() / 100.0F);
    Serial.printf("  Altitud est.: %.1f m\n",   bme.readAltitude(1013.25F));
}

void demo_afm07() {
    printHeader("8. SENSOR DE FLUJO AFM07 (RS485 Modbus RTU)");
    Serial.printf("  Slave: 0x%02X  |  Baud: %d  |  8N1\n", AFM07_ADDR, AFM07_BAUD);
    for (int i = 1; i <= 3; i++) {
        float f = afm07_leerFlujo();
        Serial.printf("  Lectura #%d: ", i);
        if      (f == -1.0F) Serial.println("TIMEOUT");
        else if (f == -2.0F) Serial.println("ERROR CRC");
        else                 Serial.printf("%.1f L/min\n", f);
        delay(200);
    }
}

void demo_ntc() {
    printHeader("9. NTC TERMISTOR MF52HT10K");
    Serial.println(F("  Circuito: 3.3V─R(10k)─GPIO34─NTC(10k@25°C)─GND"));
    Serial.println(F("  Beta: 3950 | R25: 10kΩ\n"));
    for (int i = 1; i <= 5; i++) {
        int   raw = analogRead(PIN_NTC);
        float v   = raw * 3.3F / 4095.0F;
        float t   = leerNTC();
        Serial.printf("  Muestra #%d: ADC=%4d  V=%.3fV  ", i, raw, v);
        if (t == -99.0F) {
            Serial.println("⚠  NTC no conectado o cortocircuito");
        } else {
            float rNTC = NTC_R_FIXED * v / (3.3F - v);
            Serial.printf("R=%.0fΩ  T=%.1f°C\n", rNTC, t);
        }
        delay(300);
    }
}

// ============================================================
//  MODO INTERACTIVO
// ============================================================
void imprimirAyuda() {
    Serial.println(F("\n════════════════════════════════════════════════════════════════════════"));
    Serial.println(F("  MODO INTERACTIVO — Motor + Flujo + Temperaturas"));
    Serial.println(F("════════════════════════════════════════════════════════════════════════"));
    Serial.println(F("  Envía un entero (0-255) por Serial:"));
    Serial.printf ("    0   → motor PARADO  (0.00V)\n");
    Serial.printf ("    255 → motor FULL    (%.2fV)\n", MOTOR_VIN);
    Serial.printf ("  Rampa: paso=%d, delay=%dms → 0→255 ≈ %.0fms\n",
        MOTOR_RAMP_STEP, MOTOR_RAMP_DELAY_MS,
        (255.0F / MOTOR_RAMP_STEP) * MOTOR_RAMP_DELAY_MS);
    Serial.println(F("════════════════════════════════════════════════════════════════════════\n"));
    Serial.println(F("  Input | LEDC | V_motor | Flujo(L/m) | NTC(°C) | BME(°C) | RTC(°C)"));
    Serial.println(F("  ------|------|---------|------------|---------|---------|--------"));
}

void procesarSerialInput() {
    if (!Serial.available()) return;
    String s = Serial.readStringUntil('\n');
    s.trim();
    if (s.length() == 0) return;
    int val = constrain(s.toInt(), 0, 255);
    setMotorDuty(val);  // Incluye rampa y reimprime encabezado
}

void leerYMostrar() {
    if (millis() - g_lastLectura < 1000) return;
    g_lastLectura = millis();

    float flujo    = afm07_leerFlujo();
    float ntcTemp  = leerNTC();
    float bmeTemp  = bme.readTemperature();
    float rtcTemp  = rtc.getTemperature();
    float vMotor   = MOTOR_VIN * g_motorLEDC / 255.0F;

    // Input | LEDC | V_motor
    Serial.printf("  %5d | %4d | %5.2fV  | ",
        g_motorInput, g_motorLEDC, vMotor);

    // Flujo
    if      (flujo == -1.0F) Serial.print("  TIMEOUT   | ");
    else if (flujo == -2.0F) Serial.print(" ERROR CRC  | ");
    else                     Serial.printf("   %6.2f   | ", flujo);

    // NTC
    if (ntcTemp == -99.0F)   Serial.print("  n/c    | ");
    else                     Serial.printf("  %5.1f  | ", ntcTemp);

    // BME280
    Serial.printf("  %5.1f  | ", bmeTemp);

    // RTC
    Serial.printf("  %5.1f\n", rtcTemp);
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(1500);

    Serial.println(F("\n╔══════════════════════════════════════════╗"));
    Serial.println(F("║   EOLO DRON — Demo Secuencial v5.2      ║"));
    Serial.println(F("╚══════════════════════════════════════════╝"));

    pinMode(PIN_MOTOR_PWM, OUTPUT);
    digitalWrite(PIN_MOTOR_PWM, LOW);   // Motor parado en boot

    Wire.begin(PIN_SDA, PIN_SCL);

    pinMode(PIN_DIP1, INPUT); pinMode(PIN_DIP2, INPUT);
    pinMode(PIN_DIP3, INPUT); pinMode(PIN_DIP4, INPUT);

    neopixel.begin(); neopixel.setBrightness(60);
    neopixel.clear(); neopixel.show();

    pinMode(PIN_RS485_DERE, OUTPUT);
    digitalWrite(PIN_RS485_DERE, LOW);
    Serial2.begin(AFM07_BAUD, SERIAL_8N1, PIN_RS485_RX, PIN_RS485_TX);

    pinMode(PIN_NTC, INPUT);

    ledcSetup(MOTOR_PWM_CH, MOTOR_PWM_FREQ, MOTOR_PWM_RES);
    ledcAttachPin(PIN_MOTOR_PWM, MOTOR_PWM_CH);
    ledcWrite(MOTOR_PWM_CH, 0);

    analogReadResolution(12);
    delay(500);

    demo_scanI2C();     delay(600);
    demo_dipSwitch();   delay(400);
    demo_sdInfo();      delay(400);
    demo_sdListar();    delay(400);
    demo_neopixel();    delay(500);
    demo_rtc();         delay(400);
    demo_bme280();      delay(400);
    demo_afm07();       delay(400);
    demo_ntc();         delay(400);

    imprimirAyuda();
    g_lastLectura = millis();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
    procesarSerialInput();
    leerYMostrar();
}