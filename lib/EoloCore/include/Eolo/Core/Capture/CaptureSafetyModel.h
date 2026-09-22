#ifndef EOLO_CORE_CAPTURE_CAPTURE_SAFETY_MODEL_H
#define EOLO_CORE_CAPTURE_CAPTURE_SAFETY_MODEL_H

#include <stdint.h>

// Decisión pura de seguridad para que el orden de las comprobaciones sea el
// mismo en firmware, pruebas nativas y cualquier futura interfaz de captura.
enum class CaptureSafetyFault : uint8_t
{
    None,
    SdUnavailable,
    AfmInvalid,
    AfmStale,
    AfmDiagnostic,
    NtcInvalid
};

struct CaptureSafetyInput
{
    bool sdReady = false;
    bool afmValid = false;
    bool afmFresh = false;
    bool afmSafetyBlocked = false;
    bool ntcValid = false;
};

struct CaptureSafetyOutput
{
    bool startAllowed = false;
    bool motorAllowed = false;
    bool abortCapture = false;
    CaptureSafetyFault fault = CaptureSafetyFault::None;
};

class CaptureSafetyModel
{
public:
    static CaptureSafetyOutput evaluateStart(const CaptureSafetyInput &input)
    {
        CaptureSafetyOutput output;
        output.fault = firstFault(input);
        output.startAllowed = output.fault == CaptureSafetyFault::None;
        output.motorAllowed = output.startAllowed;
        return output;
    }

    static CaptureSafetyOutput evaluateRun(const CaptureSafetyInput &input)
    {
        CaptureSafetyOutput output;
        output.fault = firstFault(input);
        output.motorAllowed = output.fault == CaptureSafetyFault::None;
        output.abortCapture = output.fault != CaptureSafetyFault::None;
        return output;
    }

    static const char *faultName(CaptureSafetyFault fault)
    {
        switch (fault)
        {
        case CaptureSafetyFault::None: return "none";
        case CaptureSafetyFault::SdUnavailable: return "sd_unavailable";
        case CaptureSafetyFault::AfmInvalid: return "afm_invalid";
        case CaptureSafetyFault::AfmStale: return "afm_stale";
        case CaptureSafetyFault::AfmDiagnostic: return "afm_diagnostic_blocked";
        case CaptureSafetyFault::NtcInvalid: return "ntc_invalid";
        }
        return "unknown";
    }

private:
    static CaptureSafetyFault firstFault(const CaptureSafetyInput &input)
    {
        if (!input.sdReady)
            return CaptureSafetyFault::SdUnavailable;
        if (!input.afmValid)
            return CaptureSafetyFault::AfmInvalid;
        if (input.afmSafetyBlocked)
            return CaptureSafetyFault::AfmDiagnostic;
        if (!input.afmFresh)
            return CaptureSafetyFault::AfmStale;
        if (!input.ntcValid)
            return CaptureSafetyFault::NtcInvalid;
        return CaptureSafetyFault::None;
    }
};

#endif // EOLO_CORE_CAPTURE_CAPTURE_SAFETY_MODEL_H
