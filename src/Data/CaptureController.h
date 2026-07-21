#ifndef EOLO_CAPTURE_CONTROLLER_H
#define EOLO_CAPTURE_CONTROLLER_H

#include <Arduino.h>

struct Context;

class CaptureController
{
    unsigned long int pauseTime = 0;

public:
    static constexpr int CAPTURE_INTERVAL = 10;

    bool isCapturing = false;
    bool isPaused = false;
    bool isEnd = false;

    void begin(Context &ctx);
    void pause(Context &ctx);
    void resume(Context &ctx);
    void end(Context &ctx);
    void reset(Context &ctx);
    void update(Context &ctx);
};

#endif // EOLO_CAPTURE_CONTROLLER_H


#if defined(CONTEXT_CLASS_DEFINED) && !defined(EOLO_CAPTURE_CONTROLLER_IMPL_DONE)
#define EOLO_CAPTURE_CONTROLLER_IMPL_DONE

#include "Context.h"
#include "Session.h"
#include "../Config/Legacy.h"
#include <Eolo/Core/Flow/FlowSchedule.h>

#ifndef FEATURE_HEADLESS
#include "../Drawing/SceneManager.h"
#endif

inline void CaptureController::begin(Context &ctx)
{
    if (ctx.isHeadlessCalibrationRunning())
    {
        LOG_LN("Captura bloqueada: calibracion headless en curso.");
        return;
    }
    ctx.session.elapsedTime = 0;
    isCapturing = true;
    isPaused = false;
    isEnd = false;
    ctx.session.capturedVolume = 0.0;
    ctx.resetMotorFlowController();
    bool motorOverheat = ctx.updateMotorThermalProtection();
#if defined(FEATURE_FLOW_PID) && defined(EOLO_TARGET_DRON)
    if (!motorOverheat)
        ctx.components.motor.setPwmImmediate(FLOW_PID_BASE_PWM);
#endif
    LOG_LN("Iniciando captura...");
#ifdef FEATURE_MODEM
    ctx.components.modemService.warmUp();
#endif
#ifndef FEATURE_HEADLESS
    SceneManager::setScene("captura", ctx);
    ctx.enableDisplay();
#endif
}

inline void CaptureController::pause(Context &ctx)
{
#ifndef FEATURE_HEADLESS
    ctx.components.input.resetCounter();
#endif
    if (!isCapturing || isPaused)
        return;
    LOG_LN("Pausando captura...");
    isPaused = true;
    pauseTime = ctx.getUnixTime();
}

inline void CaptureController::resume(Context &ctx)
{
#ifndef FEATURE_HEADLESS
    ctx.components.input.resetCounter();
#endif
    if (!isCapturing || !isPaused)
        return;
    LOG_LN("Resumiendo captura...");
    isPaused = false;
    unsigned long now = ctx.getUnixTime();
    unsigned long pauseDelta = now - pauseTime;
    ctx.session.duration += pauseDelta;
}

inline void CaptureController::end(Context &ctx)
{
    isCapturing = false;
    isEnd = true;

    LOG_LN("Captura finalizada.");
    ctx.resetMotorFlowController();
    ctx.components.motor.setPowerPct(0);
#ifdef FEATURE_MODEM
    ctx.components.modemService.shutdownWhenIdle();
#endif
#ifndef FEATURE_HEADLESS
    ctx.components.input.resetCounter();
    SceneManager::setScene("end", ctx);
    ctx.enableDisplay();
#endif
}

inline void CaptureController::reset(Context &ctx)
{
    isCapturing = false;
    isPaused = false;
    ctx.session = Session();
    ctx.resetMotorFlowController();
    ctx.components.motor.setPowerPct(0);
#ifdef FEATURE_MODEM
    ctx.components.modemService.shutdownWhenIdle();
#endif
#ifndef FEATURE_HEADLESS
    ctx.components.input.resetCounter();
#endif
    ctx.session.elapsedTime = 0;
    LOG_LN("Estado de captura reiniciado.");
}

inline void CaptureController::update(Context &ctx)
{
    if (!isCapturing || isPaused)
        return;

    if (ctx.isHeadlessCalibrationRunning())
        return;

    if (ctx.updateMotorThermalProtection())
        return;

    unsigned long now = ctx.getUnixTime();
    bool infiniteDuration = ctx.session.duration == DRONE_DURATION_INFINITE;
    if (now >= ctx.session.startUnix)
        ctx.session.elapsedTime = now - ctx.session.startUnix;
    else
        ctx.session.elapsedTime = 0;

    if (!infiniteDuration)
    {
        DateTime endTime = DateTime(ctx.session.startUnix + ctx.session.duration);
        if (now >= endTime.unixtime())
        {
            LOG_LN("Duración de captura alcanzada.");
            LOG_OUT("Tiempo transcurrido: ");
            LOG_OUT_LN(ctx.session.elapsedTime);

            LOG_OUT("Duración establecida: ");
            LOG_OUT_LN(ctx.session.duration);

            ctx.processCaptureSample();
            end(ctx);
            return;
        }
    }

    ctx.session.targetFlow = FlowSchedule::targetAtElapsed(
        ctx.session.targetFlow,
        ctx.session.flowSectionCount,
        ctx.session.flowSections,
        ctx.session.elapsedTime);

#if !BAREBONES
    ctx.updateMotors();
#else
    ctx.components.flowSensor.flow = ctx.session.targetFlow + millis() % 2;
#endif
    if (now - ctx.session.lastLog >= CAPTURE_INTERVAL)
    {
        ctx.session.lastLog = now;
        if (EoloDebug::verboseLogsEnabled())
        {
            LOG_LN("Leyendo datos de sensores...");
            LOG_LN("Leyendo flujo...");
#if !BAREBONES
            LOG_LN("Leyendo BME280...");
#endif
        }

#if !BAREBONES
        // BME280 se sondea en el worker I2C; aquí solo se consume el último
        // snapshot para no bloquear el ciclo de captura/UI.
        BME280Data bmeData;
        (void)ctx.components.bme.getData(bmeData);
        FlowData flowData;
        if (ctx.components.flowSensor.getData(flowData))
        {
            ctx.session.capturedVolume += (flowData.flow / 60.0f) * CAPTURE_INTERVAL;
        }
        else
        {
            LOG_LN("Error al leer sensor de flujo para volumen capturado");
        }
#endif
        if (EoloDebug::verboseLogsEnabled())
        {
            LOG_LN("Registrando datos...");
        }
        ctx.enqueueLogData();
#ifdef FEATURE_MODEM
        // El registro ya viaja como DTO en la cola de SD. El envío remoto se
        // dispara desde el composition root para que el worker de logging no
        // tenga que conocer Context ni el módem.
        ctx.uploadData();
#endif
    }
}

#endif
