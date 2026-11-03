#include <Arduino.h>
#include <Wire.h>
#include <Eolo/Core/Sensors/FS3000FlowModel.h>

#if !defined(EOLO_MODEL_EXPRESS_LEGACY)
#define EOLO_MODEL_EXPRESS_LEGACY
#endif
#include "../EoloDemoPinout.h"

/*
  Caracterización del FS3000 con las dos bombas de Express Rev.1.

  Lee el sensor cada 10 ms (como el firmware) con FS3000FlowModel y maneja las
  bombas con PWM directo de 11 bits. Sirve para obtener los datos que faltan
  para afinar DualPumpFlow::makeConfig en ExpressLegacy.h:
    - curva PWM -> caudal de cada bomba sola y de las dos en serie
    - ruido (sigma) de raw, caudal instantáneo y caudal con media de 16
    - respuesta a escalón (retardo y constante de tiempo)

  Comandos por serial (terminar con Enter). Toda salida de datos es CSV.
    sweep [paso] [asentar_ms] [medir_ms]
        Barre A sola, B sola y serie (débil a tope + fuerte subiendo).
        Por defecto: 128 2000 3000 (unos 4 min).
    hold <A> <B> <seg>     fija PWM 0..2047, asienta 3 s y mide ruido
    step <A0> <B0> <A1> <B1> <ms>
        Asienta 4 s en (A0,B0), salta a (A1,B1) y vuelca muestras cada 10 ms.
    raw <seg>              vuelca muestras con las bombas como estén
    m <A> <B>              fija PWM sin medir
    x                      apaga todo (también aborta una prueba en curso)

  Arranca con las bombas apagadas. Conviene la línea de aire abierta o con el
  filtro real y el sensor montado como en el equipo.
*/

static const uint32_t SERIAL_BAUD = 115200;
static const uint32_t SAMPLE_MS = 10;
static const int PWM_MAX = (1 << EoloConfig::motorPwmResolutionBits) - 1;
static const int PWM_FREQ = 20000;
static const int CH_A = 0;
static const int CH_B = 1;
static const int AVG_N = 16; // igual que FS3K::MAX_AVG_VALUES

static int pwmA = 0;
static int pwmB = 0;
static bool abortRequested = false;

// ---------- Bombas ----------

static void writeMotors(int a, int b)
{
  pwmA = constrain(a, 0, PWM_MAX);
  pwmB = constrain(b, 0, PWM_MAX);
  ledcWrite(CH_A, EoloConfig::motorPwmInverted ? PWM_MAX - pwmA : pwmA);
  ledcWrite(CH_B, EoloConfig::motorPwmInverted ? PWM_MAX - pwmB : pwmB);
}

// ---------- Sensor ----------

struct Sample
{
    bool ok;
    uint16_t raw;
    float velocity;
    float flow;
};

static Sample readSensor()
{
  Sample s{false, 0, 0.0f, 0.0f};
  uint8_t frame[FS3000FlowModel::FrameSize];
  if (Wire.requestFrom(static_cast<int>(FS3000FlowModel::I2cAddress), static_cast<int>(sizeof(frame))) != sizeof(frame))
    return s;
  for (size_t i = 0; i < sizeof(frame); i++)
    frame[i] = Wire.read();
  if (!FS3000FlowModel::parseFrame(frame, s.raw))
    return s;
  s.velocity = FS3000FlowModel::velocityFromRaw(s.raw);
  s.flow = FS3000FlowModel::flowFromVelocity(s.velocity);
  s.ok = true;
  return s;
}

// Media móvil idéntica a la de FS3K::readData, pero sembrada con la primera
// muestra para no medir el arranque en cero.
struct Avg16
{
    float buf[AVG_N];
    int idx = 0;
    bool seeded = false;
    float push(float v)
    {
      if (!seeded)
      {
        for (int i = 0; i < AVG_N; i++)
          buf[i] = v;
        seeded = true;
      }
      buf[idx] = v;
      idx = (idx + 1) % AVG_N;
      float sum = 0.0f;
      for (int i = 0; i < AVG_N; i++)
        sum += buf[i];
      return sum / AVG_N;
    }
};

struct Welford
{
    uint32_t n = 0;
    double mean = 0.0;
    double m2 = 0.0;
    void add(double x)
    {
      n++;
      double d = x - mean;
      mean += d / n;
      m2 += d * (x - mean);
    }
    double sd() const { return n > 1 ? sqrt(m2 / (n - 1)) : 0.0; }
};

struct Stats
{
    Welford raw, velocity, flow, flow16;
    uint32_t bad = 0;
    uint32_t total = 0;
};

// ---------- Serial ----------

static String lineBuf;

// Devuelve true cuando hay una línea completa en `line`.
static bool readLine(String &line)
{
  while (Serial.available())
  {
    char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r')
    {
      if (lineBuf.length())
      {
        line = lineBuf;
        lineBuf = "";
        return true;
      }
    }
    else
    {
      lineBuf += c;
    }
  }
  return false;
}

// Durante una prueba solo 'x' tiene efecto.
static void pollAbort()
{
  while (Serial.available())
  {
    char c = static_cast<char>(Serial.read());
    if (c == 'x' || c == 'X')
      abortRequested = true;
  }
}

// ---------- Muestreo ----------

// Espera `ms` leyendo el sensor cada SAMPLE_MS. Acumula en `stats` si no es
// null y llama a `onSample` por muestra si no es null.
static void runFor(uint32_t ms, Stats *stats, Avg16 &avg,
                   void (*onSample)(uint32_t tMs, const Sample &s, float flow16))
{
  const uint32_t start = millis();
  uint32_t next = start;
  while (!abortRequested && static_cast<uint32_t>(millis() - start) < ms)
  {
    pollAbort();
    if (static_cast<int32_t>(millis() - next) < 0)
      continue;
    next += SAMPLE_MS;
    Sample s = readSensor();
    if (stats)
      stats->total++;
    if (!s.ok)
    {
      if (stats)
        stats->bad++;
      continue;
    }
    float f16 = avg.push(s.flow);
    if (stats)
    {
      stats->raw.add(s.raw);
      stats->velocity.add(s.velocity);
      stats->flow.add(s.flow);
      stats->flow16.add(f16);
    }
    if (onSample)
      onSample(millis() - start, s, f16);
  }
}

static void printStatsRow(const char *tag, const Stats &st)
{
  Serial.printf("%s,%d,%d,%.1f,%.2f,%.3f,%.3f,%.4f,%.4f,%u,%u\n", tag, pwmA, pwmB,
                st.raw.mean, st.raw.sd(), st.velocity.mean, st.flow.mean,
                st.flow.sd(), st.flow16.sd(), static_cast<unsigned>(st.raw.n),
                static_cast<unsigned>(st.bad));
}

static const char *STATS_HEADER = "tipo,pwmA,pwmB,raw_media,raw_sd,vel_media,flujo_media,flujo_sd,flujo16_sd,n,fallos";

static Stats measurePoint(int a, int b, uint32_t settleMs, uint32_t measureMs)
{
  writeMotors(a, b);
  Avg16 avg;
  runFor(settleMs, nullptr, avg, nullptr);
  Stats st;
  runFor(measureMs, &st, avg, nullptr);
  return st;
}

// ---------- Comandos ----------

static void cmdHold(int a, int b, int seconds)
{
  Serial.println(STATS_HEADER);
  Stats st = measurePoint(a, b, 3000, static_cast<uint32_t>(seconds) * 1000UL);
  printStatsRow("hold", st);
}

static void printSampleRow(uint32_t tMs, const Sample &s, float f16)
{
  Serial.printf("%u,%u,%.3f,%.3f,%.3f\n", static_cast<unsigned>(tMs), s.raw, s.velocity, s.flow, f16);
}

static void cmdStep(int a0, int b0, int a1, int b1, uint32_t ms)
{
  writeMotors(a0, b0);
  Avg16 avg;
  runFor(4000, nullptr, avg, nullptr);
  Serial.printf("# escalon (%d,%d)->(%d,%d)\n", a0, b0, a1, b1);
  Serial.println("t_ms,raw,vel,flujo,flujo16");
  writeMotors(a1, b1);
  runFor(ms, nullptr, avg, printSampleRow);
}

static void cmdRaw(int seconds)
{
  Avg16 avg;
  Serial.println("t_ms,raw,vel,flujo,flujo16");
  runFor(static_cast<uint32_t>(seconds) * 1000UL, nullptr, avg, printSampleRow);
}

struct PumpSummary
{
    float maxFlow = 0.0f;
    int startPwm = -1; // primer PWM con flujo > 0
    uint16_t maxRaw = 0;
};

static void sweepPump(const char *tag, bool pumpA, int step, uint32_t settleMs,
                      uint32_t measureMs, PumpSummary &out, int fixedOther)
{
  for (int pwm = 0; !abortRequested; pwm += step)
  {
    if (pwm > PWM_MAX)
      pwm = PWM_MAX;
    Stats st = measurePoint(pumpA ? pwm : fixedOther, pumpA ? fixedOther : pwm, settleMs, measureMs);
    printStatsRow(tag, st);
    if (st.flow.mean > out.maxFlow)
      out.maxFlow = static_cast<float>(st.flow.mean);
    if (st.raw.mean > out.maxRaw)
      out.maxRaw = static_cast<uint16_t>(st.raw.mean);
    if (out.startPwm < 0 && st.flow.mean > 0.0)
      out.startPwm = pwm;
    if (pwm == PWM_MAX)
      break;
  }
}

static void cmdSweep(int step, uint32_t settleMs, uint32_t measureMs)
{
  Serial.println(STATS_HEADER);
  PumpSummary a, b, serie;
  sweepPump("A", true, step, settleMs, measureMs, a, 0);
  writeMotors(0, 0);
  sweepPump("B", false, step, settleMs, measureMs, b, 0);
  if (abortRequested)
  {
    writeMotors(0, 0);
    return;
  }
  // La débil es la de menor caudal a tope: la ley la llena primero.
  const bool weakIsA = a.maxFlow <= b.maxFlow;
  Serial.printf("# debil=%c maxA=%.2f maxB=%.2f\n", weakIsA ? 'A' : 'B', a.maxFlow, b.maxFlow);
  // Serie: débil a tope y la fuerte sube (PWM virtual 2047..4094).
  sweepPump("serie", !weakIsA, step, settleMs, measureMs, serie, PWM_MAX);
  writeMotors(0, 0);
  Serial.printf("# resumen A: arranque_pwm=%d max=%.2f raw_max=%u\n", a.startPwm, a.maxFlow, a.maxRaw);
  Serial.printf("# resumen B: arranque_pwm=%d max=%.2f raw_max=%u\n", b.startPwm, b.maxFlow, b.maxRaw);
  Serial.printf("# resumen serie: max=%.2f raw_max=%u (raw>=3686 o flujo>=11 indica saturacion del FS3000)\n",
                serie.maxFlow, serie.maxRaw);
}

static void handleCommand(const String &line)
{
  abortRequested = false;
  int a = 0, b = 0, c = 0, d = 0, e = 0;
  if (line == "x" || line == "X")
  {
    writeMotors(0, 0);
    Serial.println("# bombas apagadas");
  }
  else if (line.startsWith("sweep"))
  {
    int step = 128, settle = 2000, meas = 3000;
    sscanf(line.c_str(), "sweep %d %d %d", &step, &settle, &meas);
    if (step < 16 || settle < 0 || meas < 500)
      Serial.println("# uso: sweep [paso>=16] [asentar_ms] [medir_ms>=500]");
    else
      cmdSweep(step, static_cast<uint32_t>(settle), static_cast<uint32_t>(meas));
    writeMotors(0, 0);
  }
  else if (line.startsWith("hold") && sscanf(line.c_str(), "hold %d %d %d", &a, &b, &c) == 3 && c > 0)
  {
    cmdHold(a, b, c);
    writeMotors(0, 0);
  }
  else if (line.startsWith("step") &&
           sscanf(line.c_str(), "step %d %d %d %d %d", &a, &b, &c, &d, &e) == 5 && e > 0)
  {
    cmdStep(a, b, c, d, static_cast<uint32_t>(e));
    writeMotors(0, 0);
  }
  else if (line.startsWith("raw") && sscanf(line.c_str(), "raw %d", &a) == 1 && a > 0)
  {
    cmdRaw(a);
  }
  else if (line.startsWith("m ") && sscanf(line.c_str(), "m %d %d", &a, &b) == 2)
  {
    writeMotors(a, b);
    Serial.printf("# PWM A=%d B=%d\n", pwmA, pwmB);
  }
  else
  {
    Serial.println("# comandos: sweep [paso] [asentar_ms] [medir_ms] | hold A B seg | step A0 B0 A1 B1 ms | raw seg | m A B | x");
  }
  if (abortRequested)
  {
    writeMotors(0, 0);
    Serial.println("# abortado, bombas apagadas");
  }
  Serial.println("# listo");
}

void setup()
{
  Serial.begin(SERIAL_BAUD);
  delay(600);

  enableDemoPeripheralPower();

  // Estado seguro antes de configurar el PWM.
  pinMode(MOTOR_PWM_PIN_A, OUTPUT);
  digitalWrite(MOTOR_PWM_PIN_A, EoloConfig::motorPwmInverted ? HIGH : LOW);
  pinMode(MOTOR_PWM_PIN_B, OUTPUT);
  digitalWrite(MOTOR_PWM_PIN_B, EoloConfig::motorPwmInverted ? HIGH : LOW);
  ledcSetup(CH_A, PWM_FREQ, EoloConfig::motorPwmResolutionBits);
  ledcSetup(CH_B, PWM_FREQ, EoloConfig::motorPwmResolutionBits);
  ledcAttachPin(MOTOR_PWM_PIN_A, CH_A);
  ledcAttachPin(MOTOR_PWM_PIN_B, CH_B);
  writeMotors(0, 0);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(EoloConfig::i2cClockHz);

  Serial.println();
  Serial.println("### FS3000FlowTune (Express Rev.1) ###");
  Wire.beginTransmission(FS3000FlowModel::I2cAddress);
  if (Wire.endTransmission() != 0)
    Serial.println("# ERROR: FS3000 no responde en 0x28. Revisar I2C, energia y GND.");
  else
    Serial.printf("# FS3000 OK | PWM 0..%d | motores GPIO %d y %d\n", PWM_MAX, MOTOR_PWM_PIN_A, MOTOR_PWM_PIN_B);
  Serial.println("# escribe 'help' para ver los comandos. Bombas apagadas.");
}

void loop()
{
  String line;
  if (readLine(line))
  {
    line.trim();
    handleCommand(line);
  }
}
