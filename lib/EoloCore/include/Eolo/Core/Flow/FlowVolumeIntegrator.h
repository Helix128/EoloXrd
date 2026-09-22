#ifndef EOLO_CORE_FLOW_FLOW_VOLUME_INTEGRATOR_H
#define EOLO_CORE_FLOW_FLOW_VOLUME_INTEGRATOR_H

#include <math.h>
#include <stdint.h>

class FlowVolumeIntegrator
{
public:
    void reset(uint32_t startedMs)
    {
        _lastMs = startedMs;
        _lastSampleId = 0;
        _lastFlowLpm = 0.0f;
        _hasSample = false;
    }

    float update(uint32_t nowMs, uint32_t sampleId, bool valid, float flowLpm)
    {
        if (!valid || sampleId == 0 || sampleId == _lastSampleId ||
            !isfinite(flowLpm) || flowLpm < 0.0f)
            return 0.0f;

        const uint32_t elapsedMs = nowMs - _lastMs;
        const float representativeFlow = _hasSample
            ? (_lastFlowLpm + flowLpm) * 0.5f
            : flowLpm;
        const float volumeL = representativeFlow *
                              (static_cast<float>(elapsedMs) / 60000.0f);
        _lastMs = nowMs;
        _lastSampleId = sampleId;
        _lastFlowLpm = flowLpm;
        _hasSample = true;
        return volumeL;
    }

private:
    uint32_t _lastMs = 0;
    uint32_t _lastSampleId = 0;
    float _lastFlowLpm = 0.0f;
    bool _hasSample = false;
};

#endif
