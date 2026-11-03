// Simulación: controlador real (DualMotorFlowController) + planta FS3000 medida.
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <vector>
#include <Eolo/Core/Flow/DualMotorFlowController.h>
#include <Eolo/Core/Flow/DualPumpFlowConfig.h>

// Curva medida de la bomba B sola (sweep.log) + tramo supuesto para la 2a bomba.
static const FlowFeedForwardPoint kTable[] = {
    {220, 0.00f}, {384, 0.195f}, {512, 0.549f}, {640, 1.014f}, {768, 1.379f}, {896, 1.738f},
    {1024, 2.115f}, {1152, 2.379f}, {1280, 2.714f}, {1408, 3.013f}, {1536, 3.338f},
    {1664, 3.661f}, {1792, 3.932f}, {1920, 4.233f}, {2047, 4.267f},
    {2457, 5.0f}, {3276, 6.2f}, {4094, 7.4f}};
static const int kN = sizeof(kTable) / sizeof(kTable[0]);

static float tableFlow(float pwm)
{
    if (pwm <= kTable[0].pwm) return 0.0f;
    for (int i = 1; i < kN; i++)
        if (pwm <= kTable[i].pwm)
        {
            float t = (pwm - kTable[i - 1].pwm) / float(kTable[i].pwm - kTable[i - 1].pwm);
            return kTable[i - 1].flowLpm + t * (kTable[i].flowLpm - kTable[i - 1].flowLpm);
        }
    return kTable[kN - 1].flowLpm;
}

struct Rng { uint32_t s = 12345; float gauss() { float a = 0; for (int i = 0; i < 12; i++) { s = s * 1664525u + 1013904223u; a += (s >> 8) / 16777216.0f; } return a - 6.0f; } };

static float envf(const char*n,float d){const char*v=getenv(n);return v?atof(v):d;}
struct Plant
{
    float gain = 1.0f, tau = envf("TAU",0.065f); int delaySteps = (int)envf("DELAY",4); float noiseScale = envf("NOISE",1.0f);
    float flow = 0, hist[64] = {0}; int head = 0;
    float noiseState = 0; Rng rng; float avg[16] = {0}; int avgIdx = 0; bool seeded = false;
    float command = 0;
    void step()
    {
        hist[head] = command; head = (head + 1) & 63;
        float d = hist[(head - 1 - delaySteps + 128) & 63];
        float tgt = gain * tableFlow(d);
        flow += (tgt - flow) * (0.010f / tau);
    }
    // lectura instantánea con ruido medido: sd ~ 0.03+0.016*flow, autocorrelación creciente con el flujo
    float instant()
    {
        float sd = flow > 0.05f ? noiseScale * (0.03f + 0.016f * flow) : 0.0f;
        float rho = 0.4f + 0.5f * fminf(fmaxf((flow - 2.5f) / 1.5f, 0.0f), 1.0f);
        noiseState = rho * noiseState + sqrtf(1 - rho * rho) * rng.gauss();
        float y = flow + sd * noiseState;
        return fmaxf(y, 0.0f);
    }
    float avg16(float v)
    {
        if (!seeded) { for (auto &a : avg) a = v; seeded = true; }
        avg[avgIdx] = v; avgIdx = (avgIdx + 1) % 16;
        float s = 0; for (float a : avg) s += a; return s / 16;
    }
};

struct Result { float settle = -1, over = 0, errLast = 0, sdLast = 0, moveLast = 0; };

struct Run
{
    DualMotorFlowController ctl; Plant plant; uint32_t now = 0, sid = 0;
    void go(const FlowPidConfig &cfg, float target, float seconds, Result &r)
    {
        const float tol = fmaxf(0.03f * target, 0.10f);
        uint32_t start = now, end = now + uint32_t(seconds * 1000);
        float peak = 0, esum = 0, fsum = 0, fsq = 0; int ec = 0, fc = 0; float move = 0; int lastV = -1;
        uint32_t inSince = 0; bool inBand = false; r.settle = -1;
        for (; now < end; now += 10)
        {
            plant.step();
            float f16 = plant.avg16(plant.instant());
            DualMotorFlowInput in; in.nowMs = now; in.targetFlow = target; in.measuredFlow = f16;
            in.flowValid = in.flowFresh = true; in.sampleId = ++sid; in.maxPwm = 2047; in.primaryMotor = 0;
            DualMotorFlowOutput out = ctl.update(in, cfg);
            int virt = out.motor1Pwm > 0 ? 2047 + out.motor1Pwm : out.motor0Pwm;
            plant.command = virt;
            if (plant.flow > peak) peak = plant.flow;
            bool ok = fabsf(plant.flow - target) <= tol;
            if (ok && !inBand) { inBand = true; inSince = now; }
            if (!ok) inBand = false;
            if (r.settle < 0 && inBand && now - inSince >= 2000) r.settle = (inSince - start) / 1000.0f;
            if (now + 5000 >= end) { esum += plant.flow - target; ec++; }
            if (now + 15000 >= end) { fsum += plant.flow; fsq += plant.flow * plant.flow; fc++;
                if (lastV >= 0) move += abs(virt - lastV); lastV = virt; }
        }
        r.over = peak > target ? (peak - target) / target * 100 : 0; if (r.over > 60) r.over = 0; /* escalón descendente: pico previo no cuenta */
        r.errLast = ec ? esum / ec : 0;
        float m = fsum / fc; r.sdLast = sqrtf(fmaxf(fsq / fc - m * m, 0)); r.moveLast = move / 15.0f;
    }
};

static FlowPidConfig makeCfg(float ki, float kp, int slew, float kiStart, int slewStart, uint32_t hold, float startFrac)
{
    FlowPidConfig c = DualPumpFlow::makeConfig(0.0f);
    static FlowFeedForwardPoint pts[kN]; for (int i = 0; i < kN; i++) pts[i] = kTable[i];
    c.feedForward = {pts, (uint8_t)kN};
    c.smoothKi = ki; c.smoothKp = kp; c.smoothSlew = slew; c.smoothKiStart = kiStart;
    c.smoothSlewStart = slewStart; c.smoothHoldMs = hold; c.smoothStartFraction = startFrac;
    c.deadband = envf("DEADBAND", c.deadband); c.intervalMs = (uint32_t)envf("INTERVAL", c.intervalMs);
    return c;
}

struct Score { float settleSum = 0; int fails = 0; float overMax = 0, errMax = 0, sdMax = 0, moveSum = 0; };

static void scenario(const FlowPidConfig &c, Score &sc, bool verbose)
{
    struct Case { float g, t0, t1; } cases[] = {{1,2,2},{1,3,3},{1,4,4},{0.75f,3,3},{1.25f,3,3},{1,2,4},{1,4,2},{0.75f,4,4}};
    for (auto &cs : cases)
    {
        Run r; r.plant.gain = cs.g;
        Result a, b;
        r.go(c, cs.t0, 25, a);
        Result w = a;
        if (cs.t1 != cs.t0) { r.go(c, cs.t1, 25, b); w = b; }
        if (w.settle < 0) sc.fails++; else sc.settleSum += w.settle;
        sc.overMax = fmaxf(sc.overMax, w.over); sc.errMax = fmaxf(sc.errMax, fabsf(w.errLast));
        sc.sdMax = fmaxf(sc.sdMax, w.sdLast); sc.moveSum += w.moveLast;
        if (verbose) printf("   g=%.2f %.0f->%.0f settle=%.1fs over=%+.1f%% err=%+.3f sd=%.3f move=%.1f cnt/s\n",
                            cs.g, cs.t0, cs.t1, w.settle, w.over, w.errLast, w.sdLast, w.moveLast);
    }
}

int main(int argc, char **argv)
{
    bool grid = argc > 1 && !strcmp(argv[1], "grid");
    if (!grid)
    {
        float ki = argc > 1 ? atof(argv[1]) : 0.7f, kp = argc > 2 ? atof(argv[2]) : 0.15f;
        int slew = argc > 3 ? atoi(argv[3]) : 60; float kis = argc > 4 ? atof(argv[4]) : 1.3f;
        int sls = argc > 5 ? atoi(argv[5]) : 200; uint32_t hold = argc > 6 ? atoi(argv[6]) : 1500; float sf = argc > 7 ? atof(argv[7]) : 0.92f;
        FlowPidConfig c = makeCfg(ki, kp, slew, kis, sls, hold, sf);
        Score s; scenario(c, s, true);
        printf("fails=%d settleSum=%.1f overMax=%.1f errMax=%.3f sdMax=%.3f moveSum=%.1f\n", s.fails, s.settleSum, s.overMax, s.errMax, s.sdMax, s.moveSum);
        return 0;
    }
    float kis[] = {1.0f, 1.25f, 1.5f, 1.75f}; float kps[] = {0.0f, 0.03f, 0.05f, 0.08f};
    int slews[] = {30}; uint32_t holds[] = {250, 300, 400, 600};
    printf("ki kp slew hold | fails settleSum overMax errMax sdMax moveSum\n");
    for (float ki : kis) for (float kp : kps) for (int sl : slews) for (uint32_t h : holds)
    {
        FlowPidConfig c = makeCfg(ki, kp, sl, ki * 1.857f, 200, h, 0.92f); // kiStart conserva la razón 1.3/0.7
        Score s; scenario(c, s, false);
        printf("%.1f %.2f %d %u | %d %.1f %.1f %.3f %.3f %.1f\n", ki, kp, sl, h, s.fails, s.settleSum, s.overMax, s.errMax, s.sdMax, s.moveSum);
    }
}
