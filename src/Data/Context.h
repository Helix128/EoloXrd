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
        updateCapture();
    }

    int getCurrentSeconds()
    {   
        if(components.rtc.ok==false)
            return 0;
        return components.rtc.now().unixtime();
    }

    void updateCapture()
    {
        if (!isCapturing)
            return;

        unsigned long now = getCurrentSeconds();
        if (session.startTime < now && now < session.endTime)
        {
            components.flowSensor.readData();
            // adjustMotorPower();

            if (now - lastCapture >= CAPTURE_INTERVAL)
            {
                lastCapture = now;
                elapsedTime += CAPTURE_INTERVAL;

                components.logger.capture(
                    now,
                    components.flowSensor.flow,
                    session.targetFlow,
                    components.bme.temperature,
                    components.bme.humidity,
                    components.bme.pressure,
                    components.plantower.pm1,
                    components.plantower.pm25,
                    components.plantower.pm10,
                    components.battery.getPct());
            }

            if (now > session.endTime)
            {
                isCapturing = false;
                isEnd = true;
            }
        }
    }
} Context;

#endif