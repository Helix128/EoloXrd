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
        if (components.rtc.ok == false)
            return 0;
        return components.rtc.now().unixtime();
    }
    
    void beginCapture(){
        Context &ctx = *this;
        ctx.session.elapsedTime = 0;
        ctx.isCapturing = true;
        ctx.session.capturedVolume = 0.0; 
        
        SceneManager::setScene("captura", ctx);
    }

    void pauseCapture()
    {
        Context &ctx = *this;
         
        ctx.components.input.resetCounter();
        if (!ctx.isCapturing || ctx.isPaused)
            return;
        ctx.isPaused = true;
        unsigned long now = ctx.getCurrentSeconds();
        ctx.remainingTime = ctx.session.endTime - now;
    }

    void resumeCapture()
    {
        Context &ctx = *this;
         
        ctx.components.input.resetCounter();
        if (!ctx.isCapturing || !ctx.isPaused)
            return;
        ctx.isPaused = false;
        unsigned long now = ctx.getCurrentSeconds();
        ctx.session.endTime = now + ctx.remainingTime;
    }

    void endCapture()
    {
        Context &ctx = *this;
        ctx.isCapturing = false;
        ctx.isEnd = true;
        ctx.resetCapture();
         
        ctx.components.input.resetCounter();
        SceneManager::setScene("end", ctx);
    }

    void resetCapture(){
        Context &ctx = *this;
        ctx.isCapturing = false;
        ctx.isPaused = false;
        ctx.remainingTime = 0;
        ctx.session = Session();
    }

    void updateMotors(){
        Context &ctx = *this;
        float currentFlow = ctx.components.flowSensor.flow;
        float targetFlow = ctx.session.targetFlow;
        float error = targetFlow - currentFlow;
        ctx.components.motor.setPowerPct(ctx.components.motor.getPowerPct() + error * 0.1f);
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
            endCapture();
            return;
        }

#if !BAREBONES
        ctx.components.flowSensor.readData();
        updateMotors();
#else
        ctx.components.flowSensor.flow = ctx.session.targetFlow + millis() % 2;
#endif

        if (now - ctx.session.lastLog >= ctx.CAPTURE_INTERVAL)
        {
            ctx.session.lastLog = now;

#if !BAREBONES
            ctx.components.bme.readData();

            ctx.components.plantower.setPower(ctx.session.usePlantower);
            ctx.components.plantower.readData();

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
#else
            Serial.println("Capturando datos... (modo barebones, solo serial)");
#endif
        }
    }

} Context;

#endif