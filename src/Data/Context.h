#ifndef CONTEXT_H
#define CONTEXT_H

#include "Session.h"
#include "Components.h"
#include "../Config.h"
#include "../Drawing/SceneManager.h"

typedef struct Context
{   
    DisplayModel &u8g2; 
    Components components;
    Session session;

    const int CAPTURE_INTERVAL = 10;

    bool isCapturing = false;

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

    
    void updateCapture()
    {   
        Context &ctx = *this;
        
        if (!ctx.isCapturing)
            return;

        unsigned long now = ctx.getCurrentSeconds();
        ctx.session.elapsedTime = now - ctx.session.startTime;

        if (now > ctx.session.endTime)
        {
            ctx.isCapturing = false;
            ctx.isEnd = true;
            SceneManager::setScene("end", ctx);
            return;
        }

        # if !BAREBONES
        ctx.components.flowSensor.readData();
        // adjustMotorPower();
        #else
        ctx.components.flowSensor.flow = ctx.session.targetFlow+millis()%2;
        # endif

        if (now - ctx.session.lastLog >= ctx.CAPTURE_INTERVAL)
        {
            ctx.session.lastLog = now;

            # if !BAREBONES
            ctx.components.logger.capture(
                now,
                ctx.components.flowSensor.flow,
                ctx.session.targetFlow,
                ctx.components.bme.temperature,
                ctx.components.bme.humidity,
                ctx.components.bme.pressure,
                ctx.components.plantower.pm1,
                ctx.components.plantower.pm25,
                ctx.components.plantower.pm10,
                ctx.components.battery.getPct());
            # else
            Serial.println("Capturando datos... (modo barebones, solo serial)");
            # endif
        }
    }
    
} Context;

#endif