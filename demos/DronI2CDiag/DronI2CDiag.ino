// Dron base (V2): diagnóstico del bus I2C principal (Wire: SDA=21, SCL=22) con
// OLED SH1106, RTC DS3231 y BME280. Usa Wire y U8g2 directos, sin I2CBus ni el
// resto del firmware, para separar problemas de hardware de la lógica del
// firmware. Bomba apagada salvo en la fase opt-in 'p'.
//
// Cada fase dura 10 s y lee RTC y BME280 cada 100 ms con las mismas rutinas.
// Por fase imprime una línea por dispositivo, fáciles de pegar:
//   n=transferencias ok  nack_a=NACK de dirección  nack_d=NACK de dato
//   tmo=timeout(5)  other=códigos 1/4  short=lectura corta  empty=lectura vacía
//   inval=datos corruptos sin NACK (BCD inválido / chip ID distinto de 0x60)
//   busy=mutex no obtenido en 120 ms  slow5=transferencias de más de 5 ms
//   avg/max en µs. OLED: frames, skipped (mutex no obtenido en 1 ms) y tiempo
//   que el frame (dibujo + sendBuffer) retiene el bus.
//
// Fases por defecto (Serial 115200, 'r' repite):
//   P0 pull-ups y scan | P1 solo sensores | P2 OLED + sensores secuenciales
//   P4 OLED en core 1 + sensores en core 0 con mutex (camino del firmware)
//   P5 P1 y P4 a 50 kHz y 400 kHz
// Opt-in: 'u' P3 (como P4 sin mutex; puede colgar Wire), 'p' bomba al 50 % y
// 100 % con solo sensores, 's' escrituras a la SD con solo sensores.

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <U8g2lib.h>
#include "esp_system.h"

#define SDA_PIN 21
#define SCL_PIN 22
#define PUMP_PIN 26
#define SD_CS_PIN 5

static const uint8_t kRtcAddr = 0x68;
static const uint8_t kOledAddr = 0x3C;
static const uint32_t kFirmwareClockHz = 100000UL;
static const uint32_t kPhaseMs = 10000UL;
static const uint32_t kCycleMs = 100UL;
static const uint32_t kBusLockMs = 120UL;   // I2CBus::LOCK_TIMEOUT_MS con OLED I2C
static const uint32_t kUiLockMs = 1UL;      // I2CBus::Guard displayGuard(1)
static const uint16_t kTimeoutMs = 20;      // kI2cTransactionTimeoutMs de Dron

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

struct Stat
{
    uint32_t n, ok, nackA, nackD, tmo, other, shortRd, empty, inval, busy, slow5;
    uint64_t sumUs;
    uint32_t maxUs;
};

struct FrameStat
{
    uint32_t frames, skipped;
    uint64_t sumUs;
    uint32_t minUs, maxUs;
};

static Stat rtcStat, bmeStat;
static FrameStat frameStat;
static SemaphoreHandle_t busMutex;
static uint8_t bmeAddr = 0;
static volatile bool useMutex = false;
static volatile bool workerRun = false;
static volatile bool workerIdle = true;
static uint32_t frameCounter = 0;
static volatile uint32_t workerLockMs = kBusLockMs;

// Transporte fino: el bus se toma por transferencia de U8g2 (<=32 bytes, ~3 ms
// a 100 kHz) en vez de por frame completo. fineMode 1 = sin cesion explicita,
// 2 = cede un tick tras cada transferencia para que el worker del otro core
// pueda tomar el mutex antes de que la UI lo retome.
static uint8_t (*const origByteCb)(u8x8_t *, uint8_t, uint8_t, void *) = u8x8_byte_arduino_hw_i2c;
static volatile int fineMode = 0;
static volatile bool fineSkip = false;
static volatile uint32_t fineDrops = 0;
static volatile uint32_t fineXfers = 0, fineHoldMaxUs = 0, fineStartUs = 0, fineBytes = 0;

static uint8_t fineByteCb(u8x8_t *u8x8, uint8_t msg, uint8_t arg, void *ptr)
{
    if (fineMode == 0)
        return origByteCb(u8x8, msg, arg, ptr);
    switch (msg)
    {
    case U8X8_MSG_BYTE_START_TRANSFER:
        if (xSemaphoreTake(busMutex, pdMS_TO_TICKS(20)) != pdTRUE)
        {
            fineSkip = true;
            ++fineDrops;
            return 1;
        }
        fineSkip = false;
        fineStartUs = micros();
        return origByteCb(u8x8, msg, arg, ptr);
    case U8X8_MSG_BYTE_SEND:
        fineBytes += arg;
        return fineSkip ? 1 : origByteCb(u8x8, msg, arg, ptr);
    case U8X8_MSG_BYTE_END_TRANSFER:
    {
        if (fineSkip)
        {
            fineSkip = false;
            return 1;
        }
        uint8_t r = origByteCb(u8x8, msg, arg, ptr);
        {
            uint32_t held = micros() - fineStartUs;
            ++fineXfers;
            if (held > fineHoldMaxUs)
                fineHoldMaxUs = held;
        }
        xSemaphoreGive(busMutex);
        if (fineMode == 2)
            vTaskDelay(1);
        return r;
    }
    default:
        return origByteCb(u8x8, msg, arg, ptr);
    }
}

static void resetStats()
{
    memset(&rtcStat, 0, sizeof(rtcStat));
    memset(&bmeStat, 0, sizeof(bmeStat));
    memset(&frameStat, 0, sizeof(frameStat));
    frameStat.minUs = UINT32_MAX;
}

static bool lockBus(uint32_t ms)
{
    return !useMutex || xSemaphoreTake(busMutex, pdMS_TO_TICKS(ms)) == pdTRUE;
}
static bool lockBusDevice() { return lockBus(workerLockMs); }

static void unlockBus()
{
    if (useMutex)
        xSemaphoreGive(busMutex);
}

// 0 ok, 1..5 códigos de endTransmission, 10 lectura corta, 11 lectura vacía.
static int transfer(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len)
{
    Wire.beginTransmission(addr);
    Wire.write(reg);
    uint8_t err = Wire.endTransmission(false);
    if (err != 0)
        return err;
    size_t got = Wire.requestFrom(addr, len);
    if (got != len)
    {
        while (Wire.available())
            Wire.read();
        return got == 0 ? 11 : 10;
    }
    for (size_t i = 0; i < len; ++i)
        buf[i] = Wire.read();
    return 0;
}

static bool bcdOk(uint8_t v) { return (v & 0x0F) <= 9 && ((v >> 4) & 0x0F) <= 9; }

static bool rtcPlausible(const uint8_t *r)
{
    return bcdOk(r[0] & 0x7F) && bcdOk(r[1] & 0x7F) && bcdOk(r[2] & 0x3F) &&
           bcdOk(r[4] & 0x3F) && bcdOk(r[5] & 0x1F) && bcdOk(r[6]) &&
           (r[5] & 0x1F) >= 1 && (r[5] & 0x1F) <= 0x12 && (r[4] & 0x3F) >= 1;
}

// Una transferencia con bloqueo, tiempos y clasificación. valid() decide si los
// datos recibidos son coherentes.
static void sample(Stat &s, uint8_t addr, uint8_t reg, uint8_t len,
                   bool (*valid)(const uint8_t *))
{
    uint8_t buf[8] = {};
    ++s.n;
    if (!lockBusDevice())
    {
        ++s.busy;
        return;
    }
    uint32_t t0 = micros();
    int code = transfer(addr, reg, buf, len);
    uint32_t us = micros() - t0;
    unlockBus();

    s.sumUs += us;
    if (us > s.maxUs)
        s.maxUs = us;
    if (us > 5000)
        ++s.slow5;
    switch (code)
    {
    case 0:
        if (valid && !valid(buf))
            ++s.inval;
        else
            ++s.ok;
        break;
    case 2: ++s.nackA; break;
    case 3: ++s.nackD; break;
    case 5: ++s.tmo; break;
    case 10: ++s.shortRd; break;
    case 11: ++s.empty; break;
    default: ++s.other; break;
    }
}

static bool chipIdOk(const uint8_t *b) { return b[0] == 0x60; }

static void sensorCycle()
{
    sample(rtcStat, kRtcAddr, 0x00, 7, rtcPlausible);
    if (bmeAddr)
    {
        sample(bmeStat, bmeAddr, 0xD0, 1, chipIdOk);
        sample(bmeStat, bmeAddr, 0xF7, 8, nullptr);
    }
}

static void oledFrame()
{
    const bool whole = fineMode == 0;
    if (whole && !lockBus(kUiLockMs))
    {
        ++frameStat.skipped;
        return;
    }
    uint32_t t0 = micros();
    char line[24];
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_helvB08_tf);
    u8g2.drawStr(2, 10, "DronI2CDiag");
    u8g2.drawHLine(0, 13, 128);
    snprintf(line, sizeof(line), "frame %lu", (unsigned long)frameCounter++);
    u8g2.setFont(u8g2_font_helvR08_tf);
    u8g2.drawStr(2, 30, line);
    u8g2.drawFrame(2, 40, 124, 10);
    u8g2.drawBox(3, 41, (frameCounter * 3) % 122, 8);
    u8g2.sendBuffer();
    uint32_t us = micros() - t0;
    if (whole)
        unlockBus();

    ++frameStat.frames;
    frameStat.sumUs += us;
    if (us < frameStat.minUs)
        frameStat.minUs = us;
    if (us > frameStat.maxUs)
        frameStat.maxUs = us;
}

static void workerTask(void *)
{
    while (true)
    {
        if (!workerRun)
        {
            workerIdle = true;
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }
        workerIdle = false;
        uint32_t t0 = millis();
        sensorCycle();
        uint32_t spent = millis() - t0;
        vTaskDelay(pdMS_TO_TICKS(spent >= kCycleMs ? 1 : kCycleMs - spent));
    }
}

static void setClockAll(uint32_t hz)
{
    Wire.setClock(hz);
    u8g2.setBusClock(hz);
}

static void printStat(const char *name, const Stat &s)
{
    uint32_t done = s.n - s.busy;
    Serial.printf("  %-4s n=%lu ok=%lu nack_a=%lu nack_d=%lu tmo=%lu other=%lu short=%lu empty=%lu "
                  "inval=%lu busy=%lu slow5=%lu avg=%luus max=%luus\n",
                  name, (unsigned long)s.n, (unsigned long)s.ok, (unsigned long)s.nackA,
                  (unsigned long)s.nackD, (unsigned long)s.tmo, (unsigned long)s.other,
                  (unsigned long)s.shortRd, (unsigned long)s.empty, (unsigned long)s.inval,
                  (unsigned long)s.busy, (unsigned long)s.slow5,
                  (unsigned long)(done ? s.sumUs / done : 0), (unsigned long)s.maxUs);
}

struct Phase
{
    const char *name;
    bool oled;      // dibuja frames
    bool worker;    // sensores en core 0 (si no, secuenciales en esta tarea)
    bool mutex;     // bus protegido como I2CBus::Guard
    void (*load)(); // carga extra en esta tarea (SD)
    int fine = 0;   // transporte fino de la OLED: 0 off, 1 por transferencia, 2 + cesion
};

static void runPhase(const Phase &p)
{
    resetStats();
    useMutex = p.mutex;
    fineMode = p.fine;
    fineDrops = 0;
    fineXfers = fineHoldMaxUs = fineBytes = 0;
    workerLockMs = p.fine ? 20UL : kBusLockMs;
    Serial.printf("== %s clk=%lu oled=%d worker=%d mutex=%d fine=%d ==\n", p.name,
                  (unsigned long)Wire.getClock(), p.oled, p.worker, p.mutex, p.fine);
    if (p.worker)
    {
        workerRun = true;
    }
    uint32_t end = millis() + kPhaseMs;
    while ((int32_t)(end - millis()) > 0)
    {
        uint32_t t0 = millis();
        if (p.oled)
            oledFrame();
        if (p.load)
            p.load();
        if (!p.worker)
            sensorCycle();
        uint32_t spent = millis() - t0;
        if (spent < kCycleMs)
            delay(kCycleMs - spent);
    }
    workerRun = false;
    while (!workerIdle)
        delay(5);
    useMutex = false;
    fineMode = 0;

    printStat("RTC", rtcStat);
    if (bmeAddr)
        printStat("BME", bmeStat);
    if (p.oled)
        Serial.printf("  OLED frames=%lu skipped=%lu held_us min=%lu avg=%lu max=%lu\n",
                      (unsigned long)frameStat.frames, (unsigned long)frameStat.skipped,
                      (unsigned long)(frameStat.frames ? frameStat.minUs : 0),
                      (unsigned long)(frameStat.frames ? frameStat.sumUs / frameStat.frames : 0),
                      (unsigned long)frameStat.maxUs);
    if (p.fine)
        Serial.printf("  OLED transferencias=%lu descartadas=%lu bytes=%lu retencion_max=%luus\n",
                      (unsigned long)fineXfers, (unsigned long)fineDrops, (unsigned long)fineBytes,
                      (unsigned long)fineHoldMaxUs);
    Serial.printf("  idle SDA=%d SCL=%d\n", digitalRead(SDA_PIN), digitalRead(SCL_PIN));
}

static void pumpOff()
{
    ledcWrite(0, 0);
    ledcDetachPin(PUMP_PIN);
    pinMode(PUMP_PIN, OUTPUT);
    digitalWrite(PUMP_PIN, LOW);
}

static void pumpPhase(const char *name, uint32_t duty)
{
    ledcSetup(0, 20000, 11);
    ledcAttachPin(PUMP_PIN, 0);
    ledcWrite(0, duty);
    runPhase({name, false, true, false, nullptr});
    pumpOff();
}

static File sdFile;
static uint8_t sdBlock[4096];
static uint32_t sdBlocks = 0;

static void sdBurst()
{
    if (!sdFile)
        return;
    sdFile.write(sdBlock, sizeof(sdBlock));
    if ((++sdBlocks & 3) == 0)
        sdFile.flush();
}

static void sdPhase()
{
    if (!SD.begin(SD_CS_PIN))
    {
        Serial.println("== P7 SD: SD.begin fallo; fase omitida ==");
        return;
    }
    sdFile = SD.open("/i2cdiag.bin", FILE_WRITE);
    sdBlocks = 0;
    runPhase({"P7 sensores durante escrituras SD", false, true, false, sdBurst});
    Serial.printf("  SD bloques de 4 KB escritos=%lu\n", (unsigned long)sdBlocks);
    sdFile.close();
    SD.remove("/i2cdiag.bin");
    SD.end();
}

static void probePullups()
{
    Serial.println("== P0 pull-ups y scan ==");
    Wire.end();
    const int pins[2] = {SDA_PIN, SCL_PIN};
    const char *names[2] = {"SDA", "SCL"};
    for (int i = 0; i < 2; ++i)
    {
        pinMode(pins[i], INPUT_PULLDOWN);
        delay(5);
        int down = digitalRead(pins[i]);
        pinMode(pins[i], INPUT_PULLUP);
        delay(5);
        int up = digitalRead(pins[i]);
        Serial.printf("  %s con pull-down interno=%d, con pull-up interno=%d -> %s\n", names[i], down,
                      up,
                      (down == 1 && up == 1)   ? "pull-up externo presente"
                      : (down == 0 && up == 0) ? "LINEA A MASA o sin pull-up externo y algo la baja"
                      : (down == 0 && up == 1) ? "sin pull-up externo (solo interno)"
                                               : "inconsistente");
    }
    Wire.begin(SDA_PIN, SCL_PIN, kFirmwareClockHz);
    Wire.setTimeOut(kTimeoutMs);

    bmeAddr = 0;
    Serial.print("  scan:");
    for (uint8_t a = 1; a < 127; ++a)
    {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0)
        {
            Serial.printf(" 0x%02X", a);
            if (a == 0x76 || a == 0x77)
                bmeAddr = a;
        }
    }
    Serial.println();
    Serial.printf("  esperados: OLED 0x%02X %s, RTC 0x%02X, BME280 0x76/0x77 %s\n", kOledAddr, "(u8g2 usa 0x3C)",
                  kRtcAddr, bmeAddr ? "(detectado)" : "(NO detectado: sin estadisticas BME)");
}

static void runFine()
{
    Serial.println("---- transporte fino ----");
    setClockAll(kFirmwareClockHz);
    runPhase({"P8a fino por transferencia", true, true, true, nullptr, 1});
    runPhase({"P8b fino + cesion de 1 tick", true, true, true, nullptr, 2});
    Serial.println("---- fin transporte fino ----");
}

static void runSuite(bool unsafe, bool pump, bool sd)
{
    Serial.println("---- DronI2CDiag inicio ----");
    probePullups();
    setClockAll(kFirmwareClockHz);
    runPhase({"P1 solo sensores", false, false, false, nullptr});
    runPhase({"P2 OLED + sensores secuenciales", true, false, false, nullptr});
    runPhase({"P4 OLED core1 + sensores core0 con mutex (firmware)", true, true, true, nullptr});
    if (unsafe)
        runPhase({"P3 OLED core1 + sensores core0 SIN mutex", true, true, false, nullptr});
    const uint32_t clocks[2] = {50000UL, 400000UL};
    for (uint32_t hz : clocks)
    {
        setClockAll(hz);
        runPhase({"P5 solo sensores", false, false, false, nullptr});
        runPhase({"P5 firmware (P4)", true, true, true, nullptr});
    }
    setClockAll(kFirmwareClockHz);
    if (pump)
    {
        pumpPhase("P6 bomba 50%", 1023);
        pumpPhase("P6 bomba 100%", 2047);
    }
    if (sd)
        sdPhase();
    Serial.println("---- DronI2CDiag fin; 'r' repite, 'u' sin mutex, 'p' bomba, 's' SD ----");
}

void setup()
{
    pumpOff();
    Serial.begin(115200);
    delay(1500);
    busMutex = xSemaphoreCreateMutex();
    Serial.printf("DronI2CDiag | SDK %s | reset=%d | cpu=%luMHz | tarea sensores core0 prio2, UI core%d\n",
                  ESP.getSdkVersion(), (int)esp_reset_reason(), (unsigned long)getCpuFrequencyMhz(),
                  xPortGetCoreID());

    u8g2.begin();
    u8g2.setBusClock(kFirmwareClockHz);
    u8g2.getU8x8()->byte_cb = fineByteCb;
    xTaskCreatePinnedToCore(workerTask, "I2CDiagWorker", 4096, nullptr, 2, nullptr, 0);
    runSuite(false, false, false);
}

void loop()
{
    while (Serial.available())
    {
        switch (Serial.read())
        {
        case 'r': runSuite(false, false, false); break;
        case 'u': runSuite(true, false, false); break;
        case 'p': runSuite(false, true, false); break;
        case 's': runSuite(false, false, true); break;
        case 'f': runFine(); break;
        default: break;
        }
    }
    delay(20);
}
