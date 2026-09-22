#ifndef EOLO_CAPTURE_CONTROLLER_H
#define EOLO_CAPTURE_CONTROLLER_H

#include <Arduino.h>
#include <stdint.h>
#include <Eolo/Core/Flow/FlowVolumeIntegrator.h>

struct Context;

enum class CaptureEndReason : uint8_t
{
    None,
    Completed,
    PumpFailureZeroFlow,
    SdUnavailable,
    AfmInvalid,
    AfmStale,
    AfmDiagnostic,
    NtcInvalid,
    Safety
};

inline const char *captureEndReasonText(CaptureEndReason reason)
{
    switch (reason)
    {
    case CaptureEndReason::Completed: return "completed";
    case CaptureEndReason::PumpFailureZeroFlow: return "pump_failure_zero_flow";
    case CaptureEndReason::SdUnavailable: return "sd_unavailable";
    case CaptureEndReason::AfmInvalid: return "afm_invalid";
    case CaptureEndReason::AfmStale: return "afm_stale";
    case CaptureEndReason::AfmDiagnostic: return "afm_diagnostic_blocked";
    case CaptureEndReason::NtcInvalid: return "ntc_invalid";
    case CaptureEndReason::Safety: return "safety";
    case CaptureEndReason::None:
    default: return "none";
    }
}

class CaptureController
{
    unsigned long int pauseTime = 0;
    FlowVolumeIntegrator volumeIntegrator;

public:
    static constexpr int CAPTURE_INTERVAL = 10;

    bool isCapturing = false;
    bool isPaused = false;
    bool isEnd = false;
    CaptureEndReason endReason = CaptureEndReason::None;
    int failurePwm = 0;
    uint8_t failureConfirmations = 0;

    void begin(Context &ctx);
    void pause(Context &ctx);
    void resume(Context &ctx);
    void end(Context &ctx, CaptureEndReason reason = CaptureEndReason::Completed);
    void abort(Context &ctx, CaptureEndReason reason, int failurePwm = 0,
               uint8_t failureConfirmations = 0);
    void reset(Context &ctx);
    void update(Context &ctx);
};

#endif // EOLO_CAPTURE_CONTROLLER_H
