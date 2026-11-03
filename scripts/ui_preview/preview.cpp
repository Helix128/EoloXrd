// Renderiza las pantallas de la UI de Eolo a buffers crudos (1024 B, formato u8g2)
// usando el codigo real de Renderer/GUI/escenas contra un Context simulado.
// Uso: preview <directorio_salida>   (build.py lo compila y convierte a PNG)
#include "prelude.h"
#include <assert.h>
#include <string.h>

#include <Eolo/Types/FlowData.h>
#include <Eolo/Types/BME280Data.h>
#include <Eolo/Types/PlantowerData.h>
#include <Eolo/Types/AnemometerData.h>
#include <Eolo/Types/Session.h>
#include <RTClib.h>
#include <Eolo/Core/Calibration/CalibrationModel.h>

unsigned long previewMillis = 0;

#ifndef PREVIEW_MOTORS
#define PREVIEW_MOTORS 2
#endif

#if defined(FEATURE_FLOW_FS3000)
  #include "Variants/Profiles/ExpressLegacy.h"
#elif PREVIEW_MOTORS == 1
  #include "Variants/Profiles/Dron.h"
#elif defined(FEATURE_DUAL_BATTERY)
  #include "Variants/Profiles/Standard.h"
#else
  #include "Variants/Profiles/Express.h"
#endif

namespace EoloConfig
{
inline constexpr bool useCalibrationSeed = Profile::kUseCalibrationSeed;
inline constexpr auto flowPid = Profile::kFlowPid;
}

constexpr int MAX_PWM = (1 << EoloConfig::Profile::kMotorPwmResolutionBits) - 1;

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
inline bool previewCalibrationSaveSucceeds = true;
inline unsigned previewCalibrationSaveCalls = 0;
class CalibrationManager
{
public:
    static constexpr int MAX_POINTS = CalibrationModel::MaxPoints;
    static constexpr int MIN_POINTS = CalibrationModel::MinPoints;
    int numPoints = 0;
    int pwm0[MAX_POINTS] = {};
    int pwm1[MAX_POINTS] = {};
    float flows[MAX_POINTS] = {};
    int weakMotor = 0;
    bool isLoaded = false;

    void sortByFlow() { CalibrationModel::sortByFlow(numPoints, pwm0, pwm1, flows); }
    bool validate() const { return CalibrationModel::validate(numPoints, flows); }
    bool save() { ++previewCalibrationSaveCalls; return previewCalibrationSaveSucceeds && validate(); }
    bool getMotorPwms(float flow, int &out0, int &out1) const
    {
        return CalibrationModel::getMotorPwms(isLoaded, numPoints, pwm0, pwm1, flows, flow, out0, out1);
    }
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
    int maxObservedPwm = 0;
    unsigned pwmCommands = 0;
    unsigned stopCommands = 0;
    bool withinPwmLimits = true;
    void setMotorPwm(int motor, int pwm)
    {
        if (motor < 0 || motor >= motorCount) return;
        record(motor, pwm);
    }
    void setMotorPwmImmediate(int motor, int pwm) { setMotorPwm(motor, pwm); }
    void setPwmImmediate(int pwm)
    {
        record(0, pwm);
        record(1, pwm);
        if (pwm == 0) ++stopCommands;
    }
    void setPwm(int pwm)
    {
        record(0, pwm);
        record(1, pwm);
        if (pwm == 0) ++stopCommands;
    }
    void setPowerPct(int) {}

    bool isOff() const { return pwmValues[0] == 0 && pwmValues[1] == 0; }

private:
    void record(int motor, int pwm)
    {
        if (pwm < 0 || pwm > MAX_PWM) withinPwmLimits = false;
        if (pwm > maxObservedPwm) maxObservedPwm = pwm;
        pwmValues[motor] = pwm;
        ++pwmCommands;
    }
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
    CalibrationManager calibration;
    UiSnapshot snapshot;
    std::atomic<BootPhase> bootPhase{BootPhase::Idle};
    std::atomic_bool bootInitComplete{false};
    CaptureEndReason endReason = CaptureEndReason::Completed;
    RTCNetworkSyncStatus rtcStatus = RTCNetworkSyncStatus::Idle;
    SDStatus sd = SD_OK;
    const char *rtcAdjustReturnScene = "inicio";
    const char *optionsReturnScene = "inicio";
#ifdef FEATURE_MODEM
    static constexpr bool kCanSyncRtcOverNetwork = true;
#else
    static constexpr bool kCanSyncRtcOverNetwork = false;
#endif

    const UiSnapshot &getUiSnapshot() const { return snapshot; }
    uint32_t getUnixTime() { return components.rtc.now().unixtime(); }
    SDStatus sdStatus() const { return sd; }
    CaptureEndReason captureEndReason() const { return endReason; }
    RTCNetworkSyncStatus getRTCNetworkSyncStatus() const { return rtcStatus; }
    void startRTCNetworkSync() {}
    bool canLoadSession() { return true; }
    void saveSession() {}
    void loadSession() {}
    unsigned clearSessionCalls = 0;
    unsigned resetCaptureCalls = 0;
    void clearSession() { ++clearSessionCalls; }
    void beginCapture() {}
    void updateCapture() {}
    void pauseCapture() {}
    void resumeCapture() {}
    void endCapture() {}
    void resetCapture() { ++resetCaptureCalls; session = Session(); }
    bool isCaptureActive() { return true; }
    bool isCapturePaused() { return false; }
    void startBootInitTask() {}
    void acknowledgeBootPhaseRendered() {}
};

class SceneManager
{
public:
    inline static const char *destination = nullptr;
    static void setScene(const char *name, Context &) { destination = name; }
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
#include "Ui/Scenes/Setup/CalibrationScene.h"

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

// Prueba de navegacion real: el encoder alcanza todas las paginas y salta
// ambas paginas PM cuando Plantower no esta habilitado en la sesion.
static void checkCapturePages()
{
    Context ctx;
    fillDefaults(ctx);
    CapturaScene scene;
    scene.enter(ctx);
#ifdef FEATURE_ANEMOMETER
    constexpr int count = 8;
#else
    constexpr int count = 7;
#endif
    for (bool pmEnabled : {true, false})
    {
        ctx.snapshot.airQuality.enabled = pmEnabled;
        for (int direction : {1, -1})
        {
            scene.cycleFooter = 0;
            bool seen[count] = {};
            const int visible = count - (pmEnabled ? 0 : 2);
            for (int i = 0; i < visible; ++i)
            {
                assert(!seen[scene.cycleFooter]);
                assert(CapturaScene::footerPageAvailable(ctx, scene.cycleFooter));
                seen[scene.cycleFooter] = true;
                ctx.components.input.delta = direction;
                scene.update(ctx);
            }
            assert(scene.cycleFooter == 0);
            for (int page = 0; page < count; ++page)
                assert(seen[page] == CapturaScene::footerPageAvailable(ctx, page));
        }
    }

    ctx.u8g2.setFont(FONT_VALUE);
    for (const char *label : {"Temp " UNIT_TEMP, "Hum " UNIT_PCT, "P " UNIT_PRESSURE})
        assert(ctx.u8g2.getStrWidth(label) < Layout::ThirdColumnW);
    ctx.u8g2.setFont(FONT_LABEL);
    for (const char *value : {"-40.00", "85.00", "100.0", "1100"})
        assert(ctx.u8g2.getStrWidth(value) < Layout::ThirdColumnW);

    ctx.u8g2.setFont(FONT_VALUE);
    assert(ctx.u8g2.getStrWidth("23:59") <= Layout::Header::Time.w);
    // Bateria: 1px + icono 5 + 2px + texto + 1px dentro del slot.
#ifdef FEATURE_DUAL_BATTERY
    assert(9 + ctx.u8g2.getStrWidth("100") <= Layout::Header::Battery0.w);
    assert(9 + ctx.u8g2.getStrWidth("100") <= Layout::Header::Battery1.w);
#else
    assert(9 + ctx.u8g2.getStrWidth("100%") <= Layout::Header::Battery.w);
    ctx.u8g2.setFont(FONT_LABEL);
    assert(ctx.u8g2.getStrWidth("EOLO") <= Layout::Header::Brand.w);
#endif
}

static bool sameSession(const Session &a, const Session &b)
{
    if (a.startUnix != b.startUnix || a.duration != b.duration ||
        a.elapsedTime != b.elapsedTime || a.lastLog != b.lastLog ||
        a.targetFlow != b.targetFlow || a.usePlantower != b.usePlantower ||
        a.capturedVolume != b.capturedVolume ||
        a.flowSectionCount != b.flowSectionCount)
        return false;

    for (int i = 0; i < MaxFlowSections; ++i)
        if (a.flowSections[i].durationSeconds != b.flowSections[i].durationSeconds ||
            a.flowSections[i].targetFlow != b.flowSections[i].targetFlow)
            return false;
    return true;
}

static void checkFinishedCaptureReturn()
{
    Context ctx;
    fillDefaults(ctx);
    ctx.session.lastLog = 123456;
    ctx.session.usePlantower = false;
    ctx.session.flowSectionCount = 1;
    ctx.session.flowSections[0] = {900, 4.5f};
    const Session original = ctx.session;

    ctx.u8g2.setFont(FONT_VALUE);
    assert(ctx.u8g2.getStrWidth("Inicio 14:05") <= Layout::HalfColumnW);
    assert(ctx.u8g2.getStrWidth("Fin 14:35") <= Layout::HalfColumnW);
    for (const char *label : {"Tiempo:", "Objetivo:", "Volumen:"})
        assert(ctx.u8g2.getStrWidth(label) <= Layout::KvLabelRightX);
    ctx.u8g2.setFont(FONT_LABEL);
    for (const char *value : {"00:30:00", "5.0 " UNIT_FLOW, "0.142 " UNIT_VOLUME})
        assert(ctx.u8g2.getStrWidth(value) <= Layout::ScreenW - Layout::KvValueX);

    FinScene fin;
    fin.enter(ctx);
    SceneManager::destination = nullptr;
    fin.update(ctx);
    uint8_t firstRender[1024];
    memcpy(firstRender, ctx.u8g2.getBufferPtr(), sizeof(firstRender));

    ctx.session.duration += 3600;
    fin.update(ctx);
    assert(memcmp(firstRender, ctx.u8g2.getBufferPtr(), sizeof(firstRender)) == 0);
    ctx.session.duration = original.duration;

    ctx.components.input.pressed = true;
    fin.update(ctx);
    assert(SceneManager::destination && strcmp(SceneManager::destination, "end_menu") == 0);
    assert(ctx.clearSessionCalls == 1);
    assert(ctx.resetCaptureCalls == 0);
    assert(sameSession(ctx.session, original));

    EndMenuScene menu;
    menu.enter(ctx);
    ctx.components.input.delta = 2;
    menu.update(ctx);
    ctx.components.input.pressed = true;
    menu.update(ctx);
    assert(SceneManager::destination && strcmp(SceneManager::destination, "end") == 0);
    assert(sameSession(ctx.session, original));

    // Simula que el reloj avanza mientras se está en el menú. Solo cambia el
    // encabezado; el resumen de la captura debe renderizar idéntico.
    ctx.components.rtc.current = DateTime(2026, 10, 22, 18, 47, 12);
    ctx.snapshot.status.hour = 18;
    ctx.snapshot.status.minute = 47;
    fin.enter(ctx);
    fin.update(ctx);
    const uint8_t *returnedRender = ctx.u8g2.getBufferPtr();
    assert(memcmp(firstRender + 2 * 128, returnedRender + 2 * 128,
                  sizeof(firstRender) - 2 * 128) == 0);
    assert(memcmp(firstRender, returnedRender, 2 * 128) != 0);
    assert(sameSession(ctx.session, original));
}

static void seedCalibration(CalibrationManager &calibration)
{
    calibration.isLoaded = true;
    calibration.weakMotor = 1;
    calibration.numPoints = CalibrationManager::MIN_POINTS;
    for (int i = 0; i < calibration.numPoints; ++i)
    {
        calibration.pwm0[i] = (i + 1) * (MAX_PWM / 8);
        calibration.pwm1[i] = 0;
        calibration.flows[i] = 0.5f + i * 0.5f;
    }
    assert(calibration.validate());
}

static bool sameCalibration(const CalibrationManager &a, const CalibrationManager &b)
{
    if (a.numPoints != b.numPoints || a.weakMotor != b.weakMotor || a.isLoaded != b.isLoaded)
        return false;
    for (int i = 0; i < CalibrationManager::MAX_POINTS; ++i)
        if (a.pwm0[i] != b.pwm0[i] || a.pwm1[i] != b.pwm1[i] || a.flows[i] != b.flows[i])
            return false;
    return true;
}

static void updateCalibrationFlow(Context &ctx)
{
    FlowData flow;
    flow.valid = true;
    flow.fresh = true;
    flow.stale = false;
    flow.ageMs = 0;
    flow.sampleId++;
    flow.flow = 4.0f * ctx.components.motor.pwmValues[0] / MAX_PWM +
                4.5f * ctx.components.motor.pwmValues[1] / MAX_PWM;
    ctx.components.flowSensor.data = flow;
}

static void tickCalibration(CalibrationScene &scene, Context &ctx, unsigned long elapsedMs = 100)
{
    previewMillis += elapsedMs;
    updateCalibrationFlow(ctx);
    scene.update(ctx);
}

static void assertCalibrationIntroLayout(Context &ctx)
{
    ctx.u8g2.setFont(FONT_VALUE);
    for (const char *label : {"Control automatico", "Volver"})
        assert(ctx.u8g2.getStrWidth(label) <= Layout::ScreenW - 4);
    if (EoloConfig::useCalibrationSeed)
    {
        assert(ctx.u8g2.getStrWidth("Curva opcional de arranque") <= Layout::ScreenW - 4);
        assert(ctx.u8g2.getStrWidth("Curva anterior intacta") <= Layout::ScreenW - 4);
        assert(ctx.u8g2.getStrWidth("Medir") <= 48);
        assert(ctx.u8g2.getStrWidth("Volver") <= 48);
    }
    else
    {
        const char *mode = EoloConfig::flowPid.feedForward.available()
                               ? "Tabla del modelo"
                               : "Ajuste por caudal";
        assert(ctx.u8g2.getStrWidth(mode) <= Layout::ScreenW - 4);
        assert(ctx.u8g2.getStrWidth("Sin barrido manual") <= Layout::ScreenW - 4);
    }
}

static void assertCalibrationSceneLayout(Context &ctx)
{
    assertCalibrationIntroLayout(ctx);
    assert(Layout::BodyTop < 24 && 60 + Layout::FontDescent <= Layout::BodyBottom);
    assert(47 + 13 <= Layout::ScreenH && 57 <= Layout::ScreenH);

    ctx.u8g2.setFont(FONT_TITLE);
    for (const char *label : {"Calibrando", "Curva guardada", "Error de curva"})
        assert(ctx.u8g2.getStrWidth(label) <= Layout::ScreenW - 4);

    ctx.u8g2.setFont(FONT_VALUE);
    for (const char *label : {"Descubrimiento", "Rampa motor debil", "Rampa motor fuerte",
                              "Guardando curva", "Bombas apagadas", "No apagues EOLO",
                              "Pausa segura", "Curva anterior se conserva", "Curva anterior intacta",
                              "Curva no guardada", "Sin dato de flujo", "Volver", "84 puntos"})
        assert(ctx.u8g2.getStrWidth(label) <= Layout::ScreenW - 4);

    ctx.u8g2.setFont(FONT_MENU_SEL);
    assert(ctx.u8g2.getStrWidth("Pausa segura") <= Layout::ScreenW - 4);
    ctx.u8g2.setFont(FONT_LABEL);
    assert(ctx.u8g2.getStrWidth("Seguir") <= 48);
    ctx.u8g2.setFont(FONT_VALUE);
    assert(ctx.u8g2.getStrWidth("Salir") <= 48);
}

static void enterCalibrationIntro(CalibrationScene &scene, Context &ctx, const char *shotName)
{
    scene.enter(ctx);
    const unsigned commandsBeforeIntro = ctx.components.motor.pwmCommands;
    const CalibrationManager calibrationBeforeIntro = ctx.calibration;
    scene.update(ctx);
    dump(ctx, shotName);
    assert(ctx.components.motor.pwmCommands == commandsBeforeIntro);
    assert(ctx.components.motor.isOff());
    assert(sameCalibration(ctx.calibration, calibrationBeforeIntro));
}

static void confirmCalibrationMeasure(CalibrationScene &scene, Context &ctx)
{
    assert(EoloConfig::useCalibrationSeed);
    // Entra con Volver seleccionado; el encoder debe elegir Medir antes de
    // confirmar. No cambia la curva ni energiza bombas durante la navegación.
    ctx.components.input.delta = 1;
    updateCalibrationFlow(ctx);
    scene.update(ctx);
    dump(ctx, "calibration_intro_medir");
    assert(ctx.components.motor.isOff());
    assert(ctx.calibration.numPoints == CalibrationManager::MIN_POINTS);

    ctx.components.input.pressed = true;
    updateCalibrationFlow(ctx);
    scene.update(ctx);
    assert(ctx.components.motor.isOff());
    assert(ctx.calibration.numPoints == 0);
    assert(!ctx.calibration.isLoaded);
    assert(previewCalibrationSaveCalls == 0);
}

static void checkCalibrationNoSeed(Context &ctx)
{
    assert(!EoloConfig::useCalibrationSeed);
    seedCalibration(ctx.calibration);
    const CalibrationManager previous = ctx.calibration;
    ctx.components.motor.setPwmImmediate(0);
    CalibrationScene scene;
    enterCalibrationIntro(scene, ctx, "calibration_intro");
    assertCalibrationSceneLayout(ctx);
    SceneManager::destination = nullptr;
    ctx.components.input.pressed = true;
    scene.update(ctx);
    assert(SceneManager::destination && strcmp(SceneManager::destination, "inicio") == 0);
    assert(ctx.components.motor.isOff());
    assert(sameCalibration(ctx.calibration, previous));
    assert(previewCalibrationSaveCalls == 0);
}

static void checkCalibrationSeedIntroCancel()
{
    Context ctx;
    fillDefaults(ctx);
    seedCalibration(ctx.calibration);
    const CalibrationManager previous = ctx.calibration;
    ctx.components.motor.setPwmImmediate(0);
    CalibrationScene scene;
    enterCalibrationIntro(scene, ctx, "calibration_intro");
    assertCalibrationSceneLayout(ctx);
    SceneManager::destination = nullptr;
    ctx.components.input.pressed = true; // default Volver.
    scene.update(ctx);
    assert(SceneManager::destination && strcmp(SceneManager::destination, "inicio") == 0);
    assert(ctx.components.motor.isOff());
    assert(sameCalibration(ctx.calibration, previous));
}

static void checkCalibrationPauseContinueCancel()
{
    Context ctx;
    fillDefaults(ctx);
    ctx.components.motor.setPwmImmediate(0);
    seedCalibration(ctx.calibration);
    const CalibrationManager previous = ctx.calibration;
    previewMillis = 0;
    previewCalibrationSaveCalls = 0;
    previewCalibrationSaveSucceeds = true;
    SceneManager::destination = nullptr;

    CalibrationScene scene;
    enterCalibrationIntro(scene, ctx, "calibration_intro");
    confirmCalibrationMeasure(scene, ctx);
    tickCalibration(scene, ctx); // INIT -> descubre motor 0.
    assert(ctx.components.motor.pwmValues[0] == MAX_PWM / 2);
    assert(ctx.components.motor.pwmValues[1] == 0);
    assert(ctx.calibration.numPoints == 0);

    ctx.components.input.pressed = true;
    tickCalibration(scene, ctx);
    assert(ctx.components.motor.isOff());
    dump(ctx, "calibration_paused");

    ctx.components.input.delta = 1; // default Salir -> Seguir.
    tickCalibration(scene, ctx);
    dump(ctx, "calibration_paused_continue");
    ctx.components.input.pressed = true;
    tickCalibration(scene, ctx);
    assert(ctx.components.motor.pwmValues[0] == MAX_PWM / 2);
    assert(ctx.components.motor.pwmValues[1] == 0);
    assert(ctx.calibration.numPoints == 0);
    assert(ctx.components.motor.withinPwmLimits);
    tickCalibration(scene, ctx);
    dump(ctx, "calibration_descubrimiento_reanudado");

    ctx.components.input.pressed = true;
    tickCalibration(scene, ctx);
    assert(ctx.components.motor.isOff());
    ctx.components.input.pressed = true; // default Salir cancels the sweep.
    tickCalibration(scene, ctx);
    assert(SceneManager::destination && strcmp(SceneManager::destination, "inicio") == 0);
    assert(ctx.components.motor.isOff());
    assert(sameCalibration(ctx.calibration, previous));
}

static void runCalibrationToTerminal(bool saveSucceeds, const char *terminalShot)
{
    Context ctx;
    fillDefaults(ctx);
    ctx.components.motor.setPwmImmediate(0);
    seedCalibration(ctx.calibration);
    const CalibrationManager previous = ctx.calibration;
    previewMillis = 0;
    previewCalibrationSaveCalls = 0;
    previewCalibrationSaveSucceeds = saveSucceeds;
    SceneManager::destination = nullptr;

    CalibrationScene scene;
    enterCalibrationIntro(scene, ctx, "calibration_intro");
    confirmCalibrationMeasure(scene, ctx);
    tickCalibration(scene, ctx); // INIT -> discovery motor 0.

    bool capturedDiscovery0 = false;
    bool capturedDiscovery1 = false;
    bool capturedWeakRamp = false;
    bool capturedStrongRamp = false;
    bool capturedKick = false;
    bool capturedSaving = false;
    unsigned ticks = 0;
    while (previewCalibrationSaveCalls == 0 && ticks++ < 50000)
    {
        const int pwm0 = ctx.components.motor.pwmValues[0];
        const int pwm1 = ctx.components.motor.pwmValues[1];
        const bool discovery0 = ctx.calibration.numPoints == 0 && pwm0 == MAX_PWM / 2 && pwm1 == 0;
        const bool discovery1 = ctx.calibration.numPoints == 0 && pwm0 == 0 && pwm1 == MAX_PWM / 2;
        const bool foundWeakMotor = ctx.calibration.weakMotor == 0 || ctx.calibration.weakMotor == 1;
        const int weak = ctx.calibration.weakMotor;
        const int strong = 1 - weak;
        const bool weakRamp = foundWeakMotor && ctx.calibration.numPoints > 0 &&
                              ctx.components.motor.pwmValues[weak] > 0 &&
                              ctx.components.motor.pwmValues[strong] == 0;
        const bool strongRamp = foundWeakMotor &&
                                ctx.components.motor.pwmValues[weak] == MAX_PWM &&
                                ctx.components.motor.pwmValues[strong] > 0;
        const bool kick = foundWeakMotor && ctx.calibration.numPoints > 0 &&
                          ctx.components.motor.pwmValues[weak] == MAX_PWM &&
                          ctx.components.motor.pwmValues[strong] == 0;

        const unsigned stopsBeforeTick = ctx.components.motor.stopCommands;
        tickCalibration(scene, ctx);
        if (discovery0 && !capturedDiscovery0)
        {
            dump(ctx, "calibration_descubrimiento_motor0");
            capturedDiscovery0 = true;
        }
        if (discovery1 && !capturedDiscovery1)
        {
            dump(ctx, "calibration_descubrimiento_motor1");
            capturedDiscovery1 = true;
        }
        if (weakRamp && !capturedWeakRamp)
        {
            dump(ctx, "calibration_rampa_debil");
            capturedWeakRamp = true;
        }
        if (strongRamp && !capturedStrongRamp)
        {
            dump(ctx, "calibration_rampa_fuerte");
            capturedStrongRamp = true;
        }
        if (kick && !capturedKick)
        {
            dump(ctx, "calibration_impulso_inicio");
            capturedKick = true;
        }
        if (previewCalibrationSaveCalls > 0 && !capturedSaving &&
            ctx.components.motor.stopCommands > stopsBeforeTick)
        {
            dump(ctx, "calibration_guardando");
            capturedSaving = true;
        }
    }

    assert(ticks < 50000);
    assert(capturedDiscovery0 && capturedDiscovery1 && capturedWeakRamp && capturedStrongRamp);
    assert(capturedKick && capturedSaving);
    assert(ctx.components.motor.withinPwmLimits);
    assert(ctx.components.motor.maxObservedPwm == MAX_PWM);
    assert(ctx.components.motor.isOff());
    assert(ctx.components.motor.stopCommands > 0);
    assert(previewCalibrationSaveCalls == 1);

    const bool completed = saveSucceeds;
    if (completed)
    {
        assert(ctx.calibration.isLoaded);
        assert(ctx.calibration.validate());
        assert(ctx.calibration.numPoints >= CalibrationManager::MIN_POINTS);
    }
    else
    {
        assert(ctx.calibration.isLoaded);
        assert(sameCalibration(ctx.calibration, previous));
    }
    tickCalibration(scene, ctx);
    assert(ctx.components.motor.isOff());
    dump(ctx, terminalShot);
}

static void checkCalibrationPreview()
{
    Context ctx;
    fillDefaults(ctx);
    assertCalibrationSceneLayout(ctx);
    checkFinishedCaptureReturn();

    if (!EoloConfig::useCalibrationSeed)
    {
        checkCalibrationNoSeed(ctx);
        return;
    }

    checkCalibrationSeedIntroCancel();
    checkCalibrationPauseContinueCancel();
    runCalibrationToTerminal(true, "calibration_completada");
    runCalibrationToTerminal(false, "calibration_error_guardada");
}

int main(int argc, char **argv)
{
    if (argc > 1) outDir = argv[1];
    if (argc > 2 && strcmp(argv[2], "--check-calibration") == 0)
    {
        checkCalibrationPreview();
        return 0;
    }
    checkCapturePages();
    checkFinishedCaptureReturn();

    headerShot("header_ok", nullptr);
    headerShot("header_sd_writing", [](UiSnapshot &s) { s.status.sdStatus = SD_WRITING; });
    headerShot("header_sd_error", [](UiSnapshot &s) { s.status.sdStatus = SD_ERROR; });
    headerShot("header_sd_missing", [](UiSnapshot &s) { s.status.sdStatus = SD_MISSING; s.status.sdReady = false; });
    headerShot("header_signal_0", [](UiSnapshot &s) { s.status.modemSignalBars = 0; });
    headerShot("header_signal_4_active", [](UiSnapshot &s) { s.status.modemSignalBars = 4; s.status.modemActive = true; });
    headerShot("header_modem_error", [](UiSnapshot &s) { s.status.modemError = true; });
    headerShot("header_modem_off", [](UiSnapshot &s) { s.status.modemPowered = false; });
    headerShot("header_battery_low", [](UiSnapshot &s) { s.power.batteryPct = 8; s.power.batteryPct0 = 8; s.power.batteryPct1 = 100; });
    headerShot("header_battery_full", [](UiSnapshot &s) { s.power.batteryPct = 100; s.power.batteryPct0 = 100; s.power.batteryPct1 = 100; s.power.activeBattery = 3; });
    headerShot("header_battery_unknown", [](UiSnapshot &s) { s.power.valid = false; s.power.batteryPct = -1; });
    headerShot("header_dc", [](UiSnapshot &s) { s.power.poweredByDc = true; });

    shot<LogoScene>("logo_boot");
    shot<LogoScene>("logo_services", 0, 1, [](Context &c) { c.bootPhase = Context::BootPhase::StartingServices; });
    shot<LogoScene>("logo_sd", 0, 1, [](Context &c) { c.bootPhase = Context::BootPhase::InitSD; });
    shot<LogoScene>("logo_i2c", 0, 1, [](Context &c) { c.bootPhase = Context::BootPhase::WaitingI2C; });
    shot<LogoScene>("logo_ready", 0, 1, [](Context &c) { c.bootPhase = Context::BootPhase::Ready; c.bootInitComplete = true; });
    shot<LogoScene>("logo_sd_missing", 0, 1, [](Context &c) { c.bootPhase = Context::BootPhase::Ready; c.sd = SD_MISSING; });
    shot<LogoScene>("logo_sd_error", 0, 1, [](Context &c) { c.bootPhase = Context::BootPhase::Ready; c.sd = SD_ERROR; });
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
    shot<WaitScene>("espera_sin_flujo", 0, 1, [](Context &c) { c.session.startUnix = c.getUnixTime() + 1; c.components.flowSensor.ok = false; });
    shot<WaitScene>("espera_larga", 0, 1, [](Context &c) { c.session.startUnix = c.getUnixTime() + 3600000UL; });
    shot<CapturaScene>("captura_p0");
    shot<CapturaScene>("captura_bme_extremos", 0, 2, [](Context &c) {
        c.snapshot.environment.temperature = -40.0f;
        c.snapshot.environment.humidity = 100.0f;
        c.snapshot.environment.pressure = 1100.0f;
    });
    shot<CapturaScene>("captura_pm", 1, 2);
    shot<CapturaScene>("captura_pm10", 2, 2);
    shot<CapturaScene>("captura_objetivo", 3, 2);
    shot<CapturaScene>("captura_transcurrido", 4, 2);
    shot<CapturaScene>("captura_restante", 5, 2);
    shot<CapturaScene>("captura_horarios", 6, 2);
#ifdef FEATURE_ANEMOMETER
    shot<CapturaScene>("captura_viento", 7, 2);
#endif
    shot<CapturaScene>("captura_pm_alto", 1, 2, [](Context &c) { c.snapshot.airQuality.pm1 = 9999; c.snapshot.airQuality.pm25 = 9999; });
    shot<CapturaScene>("captura_sin_pm", 1, 2, [](Context &c) { c.snapshot.airQuality.enabled = false; });
    shot<CapturaBombasScene>("probar_bombas");
    shot<FinScene>("fin");
    shot<FinScene>("fin_falla_bomba", 0, 1, [](Context &c) { c.endReason = CaptureEndReason::PumpFailureZeroFlow; });
    shot<MenuCapturaScene>("menu_captura");
    shot<MenuCapturaScene>("menu_captura_scroll", 3);
    shot<EndMenuScene>("menu_fin");
    shot<ComponentTestMenuScene>("test_menu");
    shot<ComponentSensorsScene>("test_sensores_flujo");
    shot<ComponentSensorsScene>("test_sensores_bme", 1);
    shot<ComponentSensorsScene>("test_sensores_ultimo", 99);
#ifdef FEATURE_PLANTOWER
    shot<ComponentSensorsScene>("test_sensores_pm", 2);
#endif
#ifdef FEATURE_ANEMOMETER
    shot<ComponentSensorsScene>("test_sensores_viento", 3);
#endif
    shot<ComponentMotorsScene>("test_motores");
    return 0;
}
