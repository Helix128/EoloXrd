#ifndef CONTEXT_H
#define CONTEXT_H

#include "Session.h"
#include "Components.h"
#include "../Config.h"

typedef struct Context
{   
    DisplayModel &u8g2; 
    Components components;
    Session session;

    const int CAPTURE_INTERVAL = 10;

    bool isCapturing = false;
    unsigned long elapsedTime = 0;
    unsigned long lastCapture = 0;

    bool isPaused = false;
    unsigned long remainingTime = 0;

    bool isEnd = false;

public:
    Context(DisplayModel &display) : u8g2(display) {}

    void begin()
    {
        u8g2.begin();
        components.begin();
        Serial.println("Contexto inicializado");
    }

    void update()
    {
        components.input.poll();
    }

    uint32_t getCurrentSeconds()
    {   
        if(components.rtc.ok==false)
            return 0;
        return components.rtc.now().unixtime();
    }
    
} Context;

#endif