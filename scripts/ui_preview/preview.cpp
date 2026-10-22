// Renderiza las pantallas de la UI de Eolo a buffers crudos (1024 B, formato u8g2)
// usando el codigo real de Renderer/GUI/escenas contra un Context simulado.
// Uso: preview <directorio_salida>   (build.py lo compila y convierte a PNG)
#include "prelude.h"

#include <Eolo/Types/FlowData.h>
#include <Eolo/Types/BME280Data.h>
#include <Eolo/Types/PlantowerData.h>
#include <Eolo/Types/AnemometerData.h>
#include <Eolo/Types/Session.h>
#include <RTClib.h>

unsigned long previewMillis = 0;
constexpr int MAX_PWM = 1023;

#ifndef PREVIEW_MOTORS
#define PREVIEW_MOTORS 2
#endif

enum SDStatus { SD_OK, SD_MISSING, SD_ERROR, SD_WRITING };
enum class CaptureEndReason : uint8_t { None, Completed, PumpFailureZeroFlow, SdUnavailable, AfmInvalid };
enum class RTCNetworkSyncStatus : uint8_t { Idle, Running, Success, Failed };

#include "Common/Data/UiSnapshot.h"

// ---- Context simulado (solo lo que usan las escenas) ----
#define CONTEXT_H
#define SCENEMANAGER_H
#define I2C_BUS_H

struct PreviewInput
{
    int delta = 0;
    bool pressed = false;
    int getEncoderDelta() { int d = delta; delta = 0; return d; }
    bool isButtonPressed() { bool p = pressed; pressed = false; return p; }
};
struct PreviewRtc
{
    DateTime current = DateTime(2026, 10, 22, 14, 35, 12);
    DateTime now() { return current; }
    void adjust(DateTime) {}
    DateTime timeForManualAdjustment() { return current; }
    const char *getBackupBatteryStatusText() { return "OK"; }
};
template <typename T> struct PreviewSensor
{
    T data;
    bool ok = true;
    bool getData(T &out) { out = data; return ok; }
};
struct PreviewBattery
{
    bool hasValidData() { return true; }
    bool isStale() { return false; }
    float getVoltage() { return 3.9f; }
    float getBatteryVoltage(int i) { return i ? 11.4f : 12.3f; }
    float getDCVoltage() { return 0.0f; }
};
struct MotorManager
{
    static constexpr int motorCount = PREVIEW_MOTORS;
    int pwmValues[2] = {512, 300};
    void setMotorPwm(int, int) {}
    void setMotorPwmImmediate(int, int) {}
    void setPwmImmediate(int) {}
    void setPwm(int) {}
    void setPowerPct(int) {}
};
class I2CBus
{
public:
    static I2CBus &getInstance() { static I2CBus b; return b; }
    uint32_t warmupRemainingMs() { return 2000; }
};
struct Components
{
    PreviewInput input;
    PreviewRtc rtc;
    PreviewSensor<FlowData> flowSensor;
    PreviewSensor<BME280Data> bme;
    PreviewSensor<PlantowerData> plantower;
    PreviewSensor<AnemometerData> anemometer;
    PreviewBattery battery;
    MotorManager motor;
};
struct Context
{
    enum class BootPhase : uint8_t { Idle, StartingServices, InitSD, WaitingI2C, Ready };
    using RTCNetworkSyncStatus = ::RTCNetworkSyncStatus;

    EOLO_DISPLAY_MODEL u8g2;
    Components components;
    Session session;
    UiSnapshot snapshot;
    std::atomic<BootPhase> bootPhase{BootPhase::Idle};
    std::atomic_bool bootInitComplete{false};
    CaptureEndReason endReason = CaptureEndReason::Completed;
    RTCNetworkSyncStatus rtcStatus = RTCNetworkSyncStatus::Idle;
    SDStatus sd = SD_OK;
    const char *rtcAdjustReturnScene = "inicio";

    const UiSnapshot &getUiSnapshot() const { return snapshot; }
    uint32_t getUnixTime() { return components.rtc.now().unixtime(); }
    SDStatus sdStatus() const { return sd; }
    CaptureEndReason captureEndReason() const { return endReason; }
    RTCNetworkSyncStatus getRTCNetworkSyncStatus() const { return rtcStatus; }
    void startRTCNetworkSync() {}
    bool canLoadSession() { return true; }
    void saveSession() {}
    void loadSession() {}
    void clearSession() {}
    void beginCapture() {}
    void updateCapture() {}
    void pauseCapture() {}
    void resumeCapture() {}
    void endCapture() {}
    void resetCapture() {}
    bool isCaptureActive() { return true; }
    bool isCapturePaused() { return false; }
    void startBootInitTask() {}
    void acknowledgeBootPhaseRendered() {}
};

class SceneManager
{
public:
    static void setScene(const char *, Context &) {}
};

#include "Ui/Scenes/IScene.h"
#include "Ui/Scenes/BaseMenuScene.h"
#include "Ui/Drawing/GUI.h"
#include "Ui/Scenes/LogoScene.h"
#include "Ui/Scenes/InicioScene.h"
#include "Ui/Scenes/AcercaDeScene.h"
#include "Ui/Scenes/Setup/PlantowerScene.h"
#include "Ui/Scenes/Setup/FlujoScene.h"
#include "Ui/Scenes/Setup/InstantFlujoScene.h"
#include "Ui/Scenes/Setup/TimeScene.h"
#include "Ui/Scenes/Setup/RTCAdjustMenuScene.h"
#include "Ui/Scenes/Setup/ComponentTestScenes.h"
#include "Ui/Scenes/Captura/WaitScene.h"
#include "Ui/Scenes/Captura/CapturaScene.h"
#include "Ui/Scenes/Captura/CapturaFlujoScene.h"
#include "Ui/Scenes/Captura/CapturaBombasScene.h"
#include "Ui/Scenes/Captura/FinScene.h"
#include "Ui/Scenes/Captura/HoraFinScene.h"
#include "Ui/Scenes/Captura/MenuCapturaScene.h"
#include "Ui/Scenes/Captura/MenuFinScene.h"

static const char *outDir = ".";

static void dump(Context &ctx, const char *name)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.bin", outDir, name);
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "no se pudo escribir %s\n", path); return; }
    fwrite(ctx.u8g2.getBufferPtr(), 1, 1024, f);
    fclose(f);
}

static void fillDefaults(Context &ctx)
{
    ctx.session.startUnix = ctx.components.rtc.now().unixtime() - 1800;
    ctx.session.duration = 3 * 3600;
    ctx.session.elapsedTime = 1800;
    ctx.session.targetFlow = 5.0f;
    ctx.session.capturedVolume = 142.0f;

    FlowData fd; fd.valid = true; fd.flow = 4.98f; ctx.components.flowSensor.data = fd;
    BME280Data bd; bd.valid = true; bd.temperature = 24.3f; bd.humidity = 46.0f; bd.pressure = 1012.0f;
    ctx.components.bme.data = bd;
    PlantowerData pd; pd.valid = true; pd.pm1_0 = 8; pd.pm2_5 = 14; pd.pm10_0 = 22;
    ctx.components.plantower.data = pd;
    AnemometerData ad; ad.valid = true; ad.fresh = true; ad.windKph = 12.4f; ad.direction = 245; ad.speed = 3.4f;
    ctx.components.anemometer.data = ad;

    UiSnapshot &s = ctx.snapshot;
    s.flow.valid = true; s.flow.flow = 4.98f; s.flow.targetFlow = 5.0f; s.flow.capturedVolume = 142.0f;
    s.environment.valid = true; s.environment.temperature = 24.3f; s.environment.humidity = 46.0f; s.environment.pressure = 1012.0f;
    s.airQuality.enabled = true; s.airQuality.valid = true; s.airQuality.pm1 = 8; s.airQuality.pm25 = 14; s.airQuality.pm10 = 22;
    s.wind.valid = true; s.wind.speed = 3.4f; s.wind.windKph = 12.4f; s.wind.direction = 245;
    s.status.hour = 14; s.status.minute = 35; s.status.sdStatus = SD_OK; s.status.sdReady = true;
    s.status.modemPowered = true; s.status.modemSignalKnown = true; s.status.modemSignalBars = 3;
    s.power.valid = true; s.power.batteryPct = 72; s.power.batteryPct0 = 85; s.power.batteryPct1 = 40;
    s.power.poweredByDc = false; s.power.activeBattery = 2;
}

// Renderiza una escena: enter + frames; el ultimo frame recibe el giro de encoder.
template <typename SceneT>
static void shot(const char *name, int encoderDelta = 0, int frames = 1, void (*tweak)(Context &) = nullptr)
{
    Context *ctx = new Context();
    fillDefaults(*ctx);
    if (tweak) tweak(*ctx);
    SceneT scene;
    scene.enter(*ctx);
    for (int i = 0; i < frames; i++)
    {
        previewMillis += 100;
        if (i == frames - 1) ctx->components.input.delta = encoderDelta;
        scene.update(*ctx);
    }
    dump(*ctx, name);
    delete ctx;
}

// Un frame de header con un estado de snapshot dado.
static void headerShot(const char *name, void (*tweak)(UiSnapshot &))
{
    Context *ctx = new Context();
    fillDefaults(*ctx);
    if (tweak) tweak(ctx->snapshot);
    ctx->u8g2.clearBuffer();
    GUI::displayHeader(*ctx);
    dump(*ctx, name);
    delete ctx;
}

int main(int argc, char **argv)
{
    if (argc > 1) outDir = argv[1];

    headerShot("header_ok", nullptr);
    headerShot("header_sd_writing", [](UiSnapshot &s) { s.status.sdStatus = SD_WRITING; });
    headerShot("header_sd_error", [](UiSnapshot &s) { s.status.sdStatus = SD_ERROR; });
    headerShot("header_sd_missing", [](UiSnapshot &s) { s.status.sdStatus = SD_MISSING; s.status.sdReady = false; });
    headerShot("header_signal_0", [](UiSnapshot &s) { s.status.modemSignalBars = 0; });
    headerShot("header_signal_4_active", [](UiSnapshot &s) { s.status.modemSignalBars = 4; s.status.modemActive = true; });
    headerShot("header_modem_error", [](UiSnapshot &s) { s.status.modemError = true; });
    headerShot("header_modem_off", [](UiSnapshot &s) { s.status.modemPowered = false; });
    headerShot("header_battery_low", [](UiSnapshot &s) { s.power.batteryPct = 8; s.power.batteryPct0 = 8; s.power.batteryPct1 = 100; });
    headerShot("header_battery_unknown", [](UiSnapshot &s) { s.power.valid = false; });
    headerShot("header_dc", [](UiSnapshot &s) { s.power.poweredByDc = true; });

    shot<LogoScene>("logo_boot");
    shot<InicioScene>("inicio");
    shot<InicioScene>("inicio_scroll", 4);
    shot<AcercaDeScene>("acerca_de");
    shot<PlantowerScene>("plantower_si");
    shot<PlantowerScene>("plantower_no", 1, 2);
    shot<FlujoScene>("flujo");
    shot<TimeScene>("tiempo_inicio");
    shot<TimeEndScene>("hora_fin");
    shot<RTCAdjustMenuScene>("rtc_menu");
    shot<RTCAdjustMenuScene>("rtc_sync", 0, 1, [](Context &c) { c.rtcStatus = RTCNetworkSyncStatus::Running; });
    shot<WaitScene>("espera", 0, 1, [](Context &c) { c.session.startUnix = c.components.rtc.now().unixtime() + 754; });
    shot<CapturaScene>("captura_p0");
    shot<CapturaScene>("captura_p1", 1, 2);
    shot<CapturaBombasScene>("probar_bombas");
    shot<FinScene>("fin");
    shot<FinScene>("fin_falla_bomba", 0, 1, [](Context &c) { c.endReason = CaptureEndReason::PumpFailureZeroFlow; });
    shot<MenuCapturaScene>("menu_captura");
    shot<MenuCapturaScene>("menu_captura_scroll", 3);
    shot<EndMenuScene>("menu_fin");
    shot<ComponentTestMenuScene>("test_menu");
    shot<ComponentSensorsScene>("test_sensores_flujo");
    shot<ComponentSensorsScene>("test_sensores_bme", 1);
    shot<ComponentMotorsScene>("test_motores");
    return 0;
}
