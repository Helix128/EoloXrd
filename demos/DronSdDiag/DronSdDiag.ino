// Dron base (V2): diagnóstico de la microSD por SPI (CS=5, MOSI=23, MISO=19,
// SCK=18), sin bomba ni otros periféricos activos.
//
// Cada ronda (cada 5 s, o al enviar 'r' por serial) hace, en orden:
//   1. Niveles de MISO con CS alto y pull-up / pull-down internos: si sigue al
//      pull en ambos casos la línea flota (normal con CS alto); si queda fija
//      en 0 o 1 algo la está forzando (corto, otro dispositivo).
//   2. CMD0 (GO_IDLE_STATE) manual a 400 kHz tras 80 ciclos con CS alto. La
//      respuesta R1 esperada es 0x01. Todo 0xFF = la tarjeta no contesta
//      (alimentación, contacto, MISO abierto); todo 0x00 = MISO a masa.
//   3. CMD8 (SEND_IF_COND) si CMD0 respondió: R7 esperado 0x01 .. 0x1AA.
//   4. SD.begin a 400 kHz, 1, 4 (por defecto del firmware) y 10 MHz: tipo,
//      tamaño y listado de la raíz.
// Serial 115200.

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "driver/gpio.h"

#define SD_CS_PIN 5
#define SD_MOSI_PIN 23
#define SD_MISO_PIN 19
#define SD_SCK_PIN 18

static const uint32_t kRoundIntervalMs = 5000;
static uint32_t roundCount = 0;
static uint32_t lastRoundMs = 0;

static const char *cardTypeName(uint8_t type)
{
    switch (type)
    {
    case CARD_MMC: return "MMC";
    case CARD_SD: return "SDSC";
    case CARD_SDHC: return "SDHC/SDXC";
    case CARD_NONE: return "ninguna";
    default: return "desconocida";
    }
}

static void probeMisoLevelsWithCs(int csLevel)
{
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, csLevel);
    pinMode(SD_MISO_PIN, INPUT_PULLUP);
    delay(2);
    const int withPullUp = digitalRead(SD_MISO_PIN);
    pinMode(SD_MISO_PIN, INPUT_PULLDOWN);
    delay(2);
    const int withPullDown = digitalRead(SD_MISO_PIN);
    pinMode(SD_MISO_PIN, INPUT);
    digitalWrite(SD_CS_PIN, HIGH);
    Serial.printf("[1] MISO con CS %s: pull-up=%d pull-down=%d -> %s\n",
                  csLevel ? "alto" : "BAJO", withPullUp, withPullDown,
                  (withPullUp == 1 && withPullDown == 0) ? "flota (normal)"
                  : (withPullUp == 0 && withPullDown == 0) ? "FORZADA A 0 (corto a masa u otro dispositivo)"
                  : (withPullUp == 1 && withPullDown == 1) ? "forzada a 1 (pull-up externo o dispositivo; puede ser normal)"
                                                           : "inconsistente");
}

static void probeMisoLevels()
{
    SD.end();
    SPI.end();
    probeMisoLevelsWithCs(HIGH);
    // Seleccionada, una tarjeta alimentada que no recibió comandos suele dejar
    // DO en alto o flotando; si sigue a ambos pulls nadie maneja la línea.
    probeMisoLevelsWithCs(LOW);
}

static uint8_t sendCommand(uint8_t cmd, uint32_t arg, uint8_t crc, uint8_t *extra, size_t extraLen,
                           uint8_t *raw, size_t &rawLen)
{
    SPI.transfer(0x40 | cmd);
    SPI.transfer(static_cast<uint8_t>(arg >> 24));
    SPI.transfer(static_cast<uint8_t>(arg >> 16));
    SPI.transfer(static_cast<uint8_t>(arg >> 8));
    SPI.transfer(static_cast<uint8_t>(arg));
    SPI.transfer(crc);
    uint8_t r1 = 0xFF;
    rawLen = 0;
    for (int i = 0; i < 10; ++i)
    {
        r1 = SPI.transfer(0xFF);
        raw[rawLen++] = r1;
        if ((r1 & 0x80) == 0) break;
    }
    for (size_t i = 0; i < extraLen; ++i)
        extra[i] = SPI.transfer(0xFF);
    return r1;
}

static void printRaw(const uint8_t *raw, size_t len)
{
    for (size_t i = 0; i < len; ++i)
        Serial.printf(" %02X", raw[i]);
}

static void probeRawCommands()
{
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, -1);
    // Pull-up interno: sin él, una línea que nadie maneja se lee como 0x00 y se
    // confunde con un corto a masa.
    gpio_pullup_en(static_cast<gpio_num_t>(SD_MISO_PIN));
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
    SPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
    for (int i = 0; i < 10; ++i) SPI.transfer(0xFF); // >= 74 ciclos con CS alto

    uint8_t raw[10];
    size_t rawLen = 0;
    digitalWrite(SD_CS_PIN, LOW);
    const uint8_t r1 = sendCommand(0, 0, 0x95, nullptr, 0, raw, rawLen);
    digitalWrite(SD_CS_PIN, HIGH);
    SPI.transfer(0xFF);

    bool allFF = true, all00 = true;
    for (size_t i = 0; i < rawLen; ++i)
    {
        if (raw[i] != 0xFF) allFF = false;
        if (raw[i] != 0x00) all00 = false;
    }
    Serial.printf("[2] CMD0 R1=0x%02X bytes:", r1);
    printRaw(raw, rawLen);
    Serial.printf(" -> %s\n",
                  r1 == 0x01 ? "OK (tarjeta en idle)"
                  : allFF    ? "SIN RESPUESTA (MISO en reposo con pull-up: nadie contesta; alimentación, contacto, CS o MOSI/SCK)"
                  : all00    ? "MISO SIEMPRE 0 (corto a masa o tarjeta trabada)"
                             : "respuesta inesperada (ruido o tarjeta en otro estado)");

    if (r1 == 0x01)
    {
        uint8_t r7[4] = {};
        digitalWrite(SD_CS_PIN, LOW);
        const uint8_t r8 = sendCommand(8, 0x000001AA, 0x87, r7, sizeof(r7), raw, rawLen);
        digitalWrite(SD_CS_PIN, HIGH);
        SPI.transfer(0xFF);
        Serial.printf("[3] CMD8 R1=0x%02X R7=%02X %02X %02X %02X -> %s\n", r8, r7[0], r7[1], r7[2], r7[3],
                      (r8 == 0x01 && r7[2] == 0x01 && r7[3] == 0xAA) ? "OK (SD v2, 2.7-3.6 V)"
                                                                     : "respuesta inválida");
    }
    SPI.endTransaction();
    SPI.end();
}

static void listRoot()
{
    File root = SD.open("/");
    if (!root)
    {
        Serial.println("    no se pudo abrir /");
        return;
    }
    int shown = 0;
    for (File entry = root.openNextFile(); entry && shown < 20; entry = root.openNextFile(), ++shown)
    {
        Serial.printf("    %s%s %llu\n", entry.name(), entry.isDirectory() ? "/" : "",
                      static_cast<unsigned long long>(entry.size()));
        entry.close();
    }
    root.close();
}

static void probeMount(uint32_t frequency)
{
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, -1);
    const uint32_t startMs = millis();
    const bool ok = SD.begin(SD_CS_PIN, SPI, frequency);
    const uint32_t elapsedMs = millis() - startMs;
    if (!ok)
    {
        Serial.printf("[4] SD.begin %lu Hz: FALLA (%lu ms)\n", static_cast<unsigned long>(frequency),
                      static_cast<unsigned long>(elapsedMs));
    }
    else
    {
        Serial.printf("[4] SD.begin %lu Hz: OK (%lu ms) tipo=%s tamano=%llu MB usado=%llu MB\n",
                      static_cast<unsigned long>(frequency), static_cast<unsigned long>(elapsedMs),
                      cardTypeName(SD.cardType()),
                      static_cast<unsigned long long>(SD.cardSize() / (1024ULL * 1024ULL)),
                      static_cast<unsigned long long>(SD.usedBytes() / (1024ULL * 1024ULL)));
        listRoot();
    }
    SD.end();
    SPI.end();
}

static void runRound()
{
    ++roundCount;
    Serial.printf("\n=== Ronda %lu (t=%lu ms) ===\n", static_cast<unsigned long>(roundCount),
                  static_cast<unsigned long>(millis()));
    probeMisoLevels();
    probeRawCommands();
    const uint32_t frequencies[] = {400000UL, 1000000UL, 4000000UL, 10000000UL};
    for (uint32_t frequency : frequencies)
        probeMount(frequency);
    lastRoundMs = millis();
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("DronSdDiag: diagnóstico microSD (CS=5 MOSI=23 MISO=19 SCK=18). 'r' = nueva ronda.");
    runRound();
}

void loop()
{
    if (Serial.available())
    {
        const int c = Serial.read();
        if (c == 'r' || c == 'R')
            runRound();
    }
    if (millis() - lastRoundMs >= kRoundIntervalMs)
        runRound();
}
