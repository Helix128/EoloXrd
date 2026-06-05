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

#endif
