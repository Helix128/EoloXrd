#include "CaptureController.h"
#include "Context.h"
#include "Session.h"
#include "../Config.h"

void CaptureController::begin(Context &ctx)
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
    ctx.updateMotorThermalProtection();
    LOG_LN("Iniciando captura...");
#ifdef FEATURE_MODEM
    ctx.components.modemService.warmUp();
#endif
#ifndef FEATURE_HEADLESS
    SceneManager::setScene("captura", ctx);
    ctx.enableDisplay();
#endif
}

void CaptureController::pause(Context &ctx)
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

void CaptureController::resume(Context &ctx)
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

void CaptureController::end(Context &ctx)
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

void CaptureController::reset(Context &ctx)
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

void CaptureController::update(Context &ctx)
{
    if (!isCapturing || isPaused)
        return;

    if (ctx.isHeadlessCalibrationRunning())
        return;

    if (ctx.updateMotorThermalProtection())
        return;

    unsigned long now = ctx.getUnixTime();
    bool infiniteDuration = ctx.session.duration == DRONE_DURATION_INFINITE;
    if (now >= ctx.session.startDate.unixtime())
        ctx.session.elapsedTime = now - ctx.session.startDate.unixtime();
    else
        ctx.session.elapsedTime = 0;

    if (!infiniteDuration)
    {
        DateTime endTime = DateTime(ctx.session.startDate.unixtime() + ctx.session.duration);
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

#if !BAREBONES
    ctx.updateMotors();
#else
    ctx.components.flowSensor.flow = ctx.session.targetFlow + millis() % 2;
#endif
    if (now - ctx.session.lastLog >= CAPTURE_INTERVAL)
    {
        ctx.session.lastLog = now;
        LOG_LN("Leyendo datos de sensores...");
        LOG_LN("Leyendo flujo...");

#if !BAREBONES
        LOG_LN("Leyendo BME280...");
        ctx.components.bme.readData();
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
        LOG_LN("Registrando datos...");
        ctx.enqueueLogData();
    }
}
