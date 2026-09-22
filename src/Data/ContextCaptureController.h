#ifndef EOLO_DATA_CONTEXT_CAPTURE_CONTROLLER_H
#define EOLO_DATA_CONTEXT_CAPTURE_CONTROLLER_H

// Este enlace se incluye sólo después de declarar Context por completo. Las
// transiciones que conocen Context quedan aquí para mantener independiente la
// cabecera pública de CaptureController y evitar el ciclo de reinclusión.
#include "CaptureController.h"
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
    endReason = CaptureEndReason::None;
    failurePwm = 0;
    failureConfirmations = 0;
    ctx.session.capturedVolume = 0.0;
    volumeIntegrator.reset(millis());
    ctx.resetMotorFlowControllerForCaptureStart();
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
    if (!isCapturing || isPaused)
        return;
    LOG_LN("Pausando captura...");
    isPaused = true;
    pauseTime = ctx.getUnixTime();
}

inline void CaptureController::resume(Context &ctx)
{
    if (!isCapturing || !isPaused)
        return;
    LOG_LN("Resumiendo captura...");
    isPaused = false;
    const unsigned long now = ctx.getUnixTime();
    const unsigned long pauseDelta = now - pauseTime;
    ctx.session.duration += pauseDelta;
    volumeIntegrator.reset(millis());
}

inline void CaptureController::end(Context &ctx, CaptureEndReason reason)
{
    if (isEnd)
        return;

    // Los cierres manual y temporizado usan una lectura RTC nueva. El trabajo
    // final permanece en la cola SD hasta que el mismo worker escribe de forma
    // segura la fila de captura y la entrada del índice maestro.
    ctx.enqueueFinalLogData();
    isCapturing = false;
    isEnd = true;
    endReason = reason;
    failurePwm = 0;
    failureConfirmations = 0;

    LOG_OUT("Captura finalizada: ");
    LOG_LN(captureEndReasonText(reason));
    ctx.resetMotorFlowController();
    ctx.components.motor.setPowerPct(0);
#ifdef FEATURE_MODEM
    ctx.components.modemService.shutdownWhenIdle();
#endif
#ifndef FEATURE_HEADLESS
    SceneManager::setScene("end", ctx);
    ctx.enableDisplay();
#endif
}

inline void CaptureController::abort(Context &ctx, CaptureEndReason reason,
                                     int abortedPwm, uint8_t confirmations)
{
    if (isEnd && !isCapturing)
        return;

    isCapturing = false;
    isPaused = false;
    isEnd = true;
    endReason = reason;
    failurePwm = abortedPwm;
    failureConfirmations = confirmations;
    ctx.components.motor.setPwmImmediate(0);
    ctx.resetMotorFlowController();
    LOG_OUT("Captura abortada: ");
    LOG_LN(captureEndReasonText(reason));
    if (reason == CaptureEndReason::PumpFailureZeroFlow)
    {
        LOG_OUT("pump_failure_zero_flow pwm=");
        LOG_OUT(abortedPwm);
        LOG_OUT(" confirm=");
        LOG_LN(confirmations);
    }

#ifdef FEATURE_MODEM
    ctx.components.modemService.shutdownWhenIdle();
#endif
#ifndef FEATURE_HEADLESS
    SceneManager::setScene("end", ctx);
    ctx.enableDisplay();
#endif
}

inline void CaptureController::reset(Context &ctx)
{
    isCapturing = false;
    isPaused = false;
    endReason = CaptureEndReason::None;
    failurePwm = 0;
    failureConfirmations = 0;
    ctx.session = Session();
    ctx.resetMotorFlowController();
    ctx.components.motor.setPowerPct(0);
#ifdef FEATURE_MODEM
    ctx.components.modemService.shutdownWhenIdle();
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

    const unsigned long now = ctx.getUnixTime();
    const bool infiniteDuration = ctx.session.duration == DRONE_DURATION_INFINITE;
    if (now >= ctx.session.startUnix)
    {
        const unsigned long delta = now - ctx.session.startUnix;
        // Si ocurrió un salto de época (> 1 año) al inicio de la captura, re-anclamos startUnix
        if (delta > 86400UL * 365UL && ctx.session.duration < 86400UL * 365UL && ctx.session.elapsedTime < 60UL)
        {
            LOG_F("Ajuste de época temporal detectado: re-anclando startUnix a %lu\n", now);
            ctx.session.startUnix = now;
            ctx.session.elapsedTime = 0;
        }
        else
        {
            ctx.session.elapsedTime = delta;
        }
    }
    else
    {
        ctx.session.elapsedTime = 0;
    }

    if (!infiniteDuration)
    {
        const DateTime endTime = DateTime(ctx.session.startUnix + ctx.session.duration);
        if (now >= endTime.unixtime())
        {
            LOG_LN("Duración de captura alcanzada.");
            LOG_OUT("Tiempo transcurrido: ");
            LOG_OUT_LN(ctx.session.elapsedTime);

            LOG_OUT("Duración establecida: ");
            LOG_OUT_LN(ctx.session.duration);

            end(ctx, CaptureEndReason::Completed);
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
    if (ctx.hasMotorCaptureFailure())
    {
        abort(ctx, CaptureEndReason::PumpFailureZeroFlow,
              ctx.motorCaptureFailurePwm(),
              ctx.motorCaptureFailureConfirmations());
        return;
    }
    // El volumen sigue cada muestra física nueva del caudalímetro. La escritura
    // CSV conserva su cadencia de 10 s y sólo publica el acumulado actual.
    FlowData volumeFlow;
    if (ctx.components.flowSensor.getData(volumeFlow))
    {
        ctx.session.capturedVolume += volumeIntegrator.update(
            millis(), volumeFlow.sampleId,
            volumeFlow.valid && volumeFlow.fresh && !volumeFlow.stale,
            volumeFlow.flow);
    }
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

#endif // EOLO_DATA_CONTEXT_CAPTURE_CONTROLLER_H
