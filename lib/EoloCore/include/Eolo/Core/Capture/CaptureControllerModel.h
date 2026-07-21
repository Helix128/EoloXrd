#ifndef EOLO_CORE_CAPTURE_CONTROLLER_MODEL_H
#define EOLO_CORE_CAPTURE_CONTROLLER_MODEL_H

#include <stdint.h>

// Máquina de estados de captura sin efectos laterales. El adaptador de
// aplicación traduce las acciones a motor, UI, logging y módem.
enum class CapturePhase : uint8_t
{
    Idle,
    Waiting,
    Running,
    Paused,
    Finished
};

struct CaptureMachineState
{
    CapturePhase phase = CapturePhase::Idle;
    uint32_t startUnix = 0;
    uint32_t durationSeconds = 0;
    uint32_t elapsedSeconds = 0;
    uint32_t pauseStartedUnix = 0;
    uint32_t lastLogUnix = 0;
};

struct CaptureMachineInput
{
    uint32_t nowUnix = 0;
    bool begin = false;
    bool pause = false;
    bool resume = false;
    bool end = false;
};

struct CaptureMachineOutput
{
    bool changed = false;
    bool startMotor = false;
    bool stopMotor = false;
    bool writeLog = false;
    bool finished = false;
};

class CaptureControllerModel
{
public:
    static constexpr uint32_t InfiniteDuration = UINT32_MAX;

    static bool expired(const CaptureMachineState &state, uint32_t nowUnix)
    {
        return state.durationSeconds != InfiniteDuration &&
               nowUnix >= state.startUnix &&
               nowUnix - state.startUnix >= state.durationSeconds;
    }

    static CaptureMachineOutput update(CaptureMachineState &state,
                                       const CaptureMachineInput &input,
                                       uint32_t logIntervalSeconds)
    {
        CaptureMachineOutput output;

        if (input.end && state.phase != CapturePhase::Idle && state.phase != CapturePhase::Finished)
        {
            state.phase = CapturePhase::Finished;
            output.changed = true;
            output.stopMotor = true;
            output.finished = true;
            return output;
        }

        if (input.begin && state.phase == CapturePhase::Idle)
        {
            state.phase = input.nowUnix < state.startUnix ? CapturePhase::Waiting : CapturePhase::Running;
            state.elapsedSeconds = 0;
            state.pauseStartedUnix = 0;
            output.changed = true;
            output.startMotor = state.phase == CapturePhase::Running;
        }

        if (input.pause && state.phase == CapturePhase::Running)
        {
            state.phase = CapturePhase::Paused;
            state.pauseStartedUnix = input.nowUnix;
            output.changed = true;
            output.stopMotor = true;
        }
        else if (input.resume && state.phase == CapturePhase::Paused)
        {
            const uint32_t pausedSeconds = input.nowUnix >= state.pauseStartedUnix
                                                ? input.nowUnix - state.pauseStartedUnix
                                                : 0;
            if (state.durationSeconds != InfiniteDuration)
            {
                const uint32_t remaining = UINT32_MAX - state.durationSeconds;
                state.durationSeconds += pausedSeconds > remaining ? remaining : pausedSeconds;
            }
            state.phase = CapturePhase::Running;
            state.pauseStartedUnix = 0;
            output.changed = true;
            output.startMotor = true;
        }

        if (state.phase == CapturePhase::Waiting && input.nowUnix >= state.startUnix)
        {
            state.phase = CapturePhase::Running;
            output.changed = true;
            output.startMotor = true;
        }

        if (state.phase == CapturePhase::Running)
        {
            state.elapsedSeconds = input.nowUnix >= state.startUnix
                                       ? input.nowUnix - state.startUnix
                                       : 0;
            if (expired(state, input.nowUnix))
            {
                state.phase = CapturePhase::Finished;
                output.changed = true;
                output.stopMotor = true;
                output.finished = true;
            }
            else if (logIntervalSeconds > 0 &&
                     (state.lastLogUnix == 0 || input.nowUnix - state.lastLogUnix >= logIntervalSeconds))
            {
                state.lastLogUnix = input.nowUnix;
                output.writeLog = true;
            }
        }

        return output;
    }
};

#endif
