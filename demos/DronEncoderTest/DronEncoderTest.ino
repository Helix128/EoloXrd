// Dron base (V2): prueba de concurrencia OLED + encoder + sensores.
//
// Reproduce el modelo de hilos del firmware (worker I2C en core 0, UI en core 1,
// mutex del bus principal, frame omitido si el worker lo tiene) y compara, sobre
// el MISMO flujo de lecturas del encoder, dos formas de convertir el contador
// absoluto de 8 bits en movimiento:
//   A) actual (Input.h): delta de un solo ciclo, filtro de 50 ms, hasChanged al
//      cambiar el contador crudo, lectura sin consumir.
//   B) propuesta: delta acumulado en un atomic y consumido una sola vez.
// "Real" es la suma de las diferencias con signo entre lecturas consecutivas.
//
// Serial 115200, una linea por segundo. Comandos:
//   r  reinicia contadores        o  alterna dibujo del OLED
//   s  alterna trafico de sensores (RTC/BME280/bateria) en el worker
// Girar el encoder a distintas velocidades; si A < Real hay perdida de eventos.

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define SDA1 21
#define SCL1 22
#define SDA2 32
#define SCL2 33
#define ENC_ADDR 0x08
#define BATTERY_ADC_PIN 35
#define RTC_ADDR 0x68
#define BME_ADDR 0x76

#define WORKER_LOCK_TIMEOUT_MS 120  // igual que I2CBus con OLED por I2C
#define ENC_POLL_MS 8
#define A_DEBOUNCE_MS 50
#define SCENE_INTERVAL_MS 180

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, SCL1, SDA1);
static SemaphoreHandle_t busMutex;

// --- estadisticas (escritas por un solo hilo salvo donde es atomic) ---
static std::atomic<long> realTotal{0};
static long seenA = 0, seenB = 0;
static std::atomic<int> encDeltaA{0};
static std::atomic<bool> hasChangedA{false};
static std::atomic<int> accB{0};
static std::atomic<uint32_t> encReads{0}, encFails{0}, encMaxGapMs{0};
static std::atomic<uint32_t> lockTimeouts{0}, lockMaxWaitMs{0};
static std::atomic<uint8_t> lastCnt{0}, lastDir{0}, lastBtn{0};
static uint32_t framesDrawn = 0, framesSkipped = 0, skippedWithEvent = 0;
static std::atomic<bool> oledOn{true}, sensorsOn{true};

static bool lockBus(uint32_t timeoutMs) {
  uint32_t t0 = millis();
  bool ok = xSemaphoreTakeRecursive(busMutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
  uint32_t waited = millis() - t0;
  if (waited > lockMaxWaitMs) lockMaxWaitMs = waited;
  if (!ok) lockTimeouts++;
  return ok;
}
static void unlockBus() { xSemaphoreGiveRecursive(busMutex); }

// Lectura minima de un registro (genera trafico real en Wire, sin librerias).
static void touchDevice(uint8_t addr, uint8_t reg, uint8_t n) {
  if (!lockBus(WORKER_LOCK_TIMEOUT_MS)) return;
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) == 0) {
    Wire.requestFrom(addr, n);
    while (Wire.available()) Wire.read();
  }
  unlockBus();
}

static bool readEncoder(uint8_t &dir, uint8_t &cnt, uint8_t &btn) {
  Wire1.requestFrom((uint8_t)ENC_ADDR, (size_t)3);
  if (Wire1.available() < 3) {
    while (Wire1.available()) Wire1.read();
    return false;
  }
  dir = Wire1.read();
  cnt = Wire1.read();
  btn = Wire1.read();
  return true;
}

// Worker I2C (core 0): encoder cada 8 ms, sensores a su ritmo, todo en una tarea
// como en Components::i2cWorker.
static void workerTask(void *) {
  uint8_t prevCnt = 0, prevRead = 0;
  bool first = true;
  short prevRawA = 0;
  uint32_t lastEncMs = 0, lastPollMs = 0;
  uint32_t nextEnc = millis(), nextRtc = millis(), nextBme = millis(), nextBatt = millis();

  for (;;) {
    uint32_t now = millis();

    if ((int32_t)(now - nextEnc) >= 0) {
      nextEnc = now + ENC_POLL_MS;
      uint8_t dir, cnt, btn;
      if (readEncoder(dir, cnt, btn)) {
        encReads++;
        if (lastPollMs) {
          uint32_t gap = now - lastPollMs;
          if (gap > encMaxGapMs) encMaxGapMs = gap;
        }
        lastPollMs = now;
        lastCnt = cnt; lastDir = dir; lastBtn = btn;

        if (first) {
          prevCnt = prevRead = cnt;
          prevRawA = cnt;
          first = false;
        } else {
          int8_t diff = (int8_t)(cnt - prevCnt);  // modulo 256 con signo
          prevCnt = cnt;
          realTotal += diff;
          accB += diff;  // (B)

          // (A) copia de Input::readEncoderData + debounce
          encDeltaA = 0;
          if (cnt != prevRead) hasChangedA = true;
          prevRead = cnt;
          if (cnt != (uint8_t)prevRawA) {
            if (now - lastEncMs > A_DEBOUNCE_MS) {
              int d = (int)cnt - (int)(uint8_t)prevRawA;
              if (d > 127) d -= 256;
              else if (d < -127) d += 256;
              encDeltaA = d;
              prevRawA = cnt;
              lastEncMs = now;
            }
          }
        }
      } else {
        encFails++;
      }
    }

    if (sensorsOn) {
      if ((int32_t)(now - nextRtc) >= 0) { nextRtc = now + 100; touchDevice(RTC_ADDR, 0x00, 7); }
      if ((int32_t)(now - nextBme) >= 0) { nextBme = now + 250; touchDevice(BME_ADDR, 0xF7, 8); }
      if ((int32_t)(now - nextBatt) >= 0) { nextBatt = now + 1000; (void)analogReadMilliVolts(BATTERY_ADC_PIN); }
    }
    vTaskDelay(1);
  }
}

static void drawFrame() {
  char l[28];
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, "ENC TEST");
  u8g2.drawHLine(0, 12, 128);
  snprintf(l, sizeof(l), "Real:%ld  cnt:%u", (long)realTotal, lastCnt.load());
  u8g2.drawStr(0, 23, l);
  snprintf(l, sizeof(l), "A(actual):%ld", seenA);
  u8g2.drawStr(0, 34, l);
  snprintf(l, sizeof(l), "B(propu.):%ld", seenB);
  u8g2.drawStr(0, 45, l);
  snprintf(l, sizeof(l), "skip:%lu/%lu", (unsigned long)framesSkipped, (unsigned long)(framesDrawn + framesSkipped));
  u8g2.drawStr(0, 56, l);
  u8g2.sendBuffer();
}

static void resetStats() {
  realTotal = 0; seenA = seenB = 0; accB = 0;
  framesDrawn = framesSkipped = skippedWithEvent = 0;
  encReads = encFails = encMaxGapMs = 0; lockTimeouts = lockMaxWaitMs = 0;
}

void setup() {
  Serial.begin(115200);
  busMutex = xSemaphoreCreateRecursiveMutex();
  Wire.begin(SDA1, SCL1, 100000);
  Wire1.begin(SDA2, SCL2, 10000);
  Wire1.setTimeOut(20);
  u8g2.setBusClock(100000);
  u8g2.begin();
  u8g2.setContrast(255);
  xTaskCreatePinnedToCore(workerTask, "worker", 4096, nullptr, 2, nullptr, 0);
  Serial.println("DronEncoderTest listo. r=reset o=OLED s=sensores");
}

void loop() {
  static uint32_t lastFrame = 0, lastSceneMs = 0, lastReport = 0;
  static bool dirty = false;
  uint32_t now = millis();

  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'r') resetStats();
    else if (c == 'o') { oledOn = !oledOn; Serial.printf("OLED %s\n", oledOn ? "ON" : "OFF"); }
    else if (c == 's') { sensorsOn = !sensorsOn; Serial.printf("sensores %s\n", sensorsOn ? "ON" : "OFF"); }
  }

  if (now - lastFrame >= 8) {
    lastFrame = now;

    // (B) se consume siempre, una sola vez, sin depender del guard.
    seenB += accB.exchange(0);

    // (A) Context::update: hasChanged se baja aqui, antes de pintar.
    bool event = hasChangedA.exchange(false);
    if (event) dirty = true;

    bool due = dirty || (now - lastSceneMs >= SCENE_INTERVAL_MS);
    if (due) {
      // Guard de 1 ms: si el worker tiene el bus, el frame se omite.
      if (xSemaphoreTakeRecursive(busMutex, pdMS_TO_TICKS(1)) == pdTRUE) {
        seenA += encDeltaA.load();  // lectura sin consumir, como getEncoderDelta
        dirty = false;
        lastSceneMs = now;
        if (oledOn) drawFrame();
        framesDrawn++;
        unlockBus();
      } else {
        framesSkipped++;
        if (event) skippedWithEvent++;
        dirty = false;  // en el firmware la senal ya se consumio
      }
    }
  }

  if (now - lastReport >= 1000) {
    lastReport = now;
    long real = realTotal;
    Serial.printf("real=%ld A=%ld(lost %ld) B=%ld(lost %ld) | cnt=%u dir=%u btn=%u | "
                  "frames ok=%lu skip=%lu skipConEvento=%lu | enc reads=%lu fail=%lu maxGap=%lums | "
                  "lock timeouts=%lu maxWait=%lums\n",
                  real, seenA, real - seenA, seenB, real - seenB,
                  lastCnt.load(), lastDir.load(), lastBtn.load(),
                  (unsigned long)framesDrawn, (unsigned long)framesSkipped, (unsigned long)skippedWithEvent,
                  (unsigned long)encReads.load(), (unsigned long)encFails.load(), (unsigned long)encMaxGapMs.load(),
                  (unsigned long)lockTimeouts.load(), (unsigned long)lockMaxWaitMs.load());
  }
}
