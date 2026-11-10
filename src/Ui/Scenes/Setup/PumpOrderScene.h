#ifndef PUMP_ORDER_SCENE_H
#define PUMP_ORDER_SCENE_H

#include "../../Drawing/Fonts.h"
#include "../../../Variants/Legacy.h"
#include "../IScene.h"
#include "../../../Common/Data/Context.h"
#include "../../Drawing/GUI.h"
#include "../../Drawing/SceneManager.h"
#include "../../Drawing/Renderer.h"
#include <Eolo/Core/Calibration/PumpOrderModel.h>
#include <math.h>

// Prueba rápida de bombas: cada bomba a tope y sola unos segundos; la que da
// menos caudal es la débil y arranca primero en el PID (ver PumpOrderModel).
// Se abre desde el menú y al arrancar si el orden no está definido.
class PumpOrderScene : public IScene
{
private:
    enum State
    {
        WAIT_SENSOR,
        SETTLE,
        SAMPLE,
        RESULT,
        FAILED
    };

    static constexpr uint32_t WAIT_SENSOR_MS = 8000;
    static constexpr uint32_t SETTLE_MS = 3500;
    static constexpr uint32_t SAMPLE_MS = 2000;
    static constexpr int MIN_SAMPLES = 3;
    static constexpr uint32_t MAX_SAMPLE_AGE_MS = 250;

    State state = WAIT_SENSOR;
    int motor = 0;             // bomba en prueba
    float flow[2] = {0, 0};    // caudal medio a tope por bomba
    float sum = 0.0f;
    int samples = 0;
    uint32_t lastSampleId = 0;
    uint32_t stateStartMs = 0;
    int weak = -1;
    bool saved = false;
    const char *failure = "";

    static bool freshFlow(Context &ctx, FlowData &fd)
    {
        return ctx.components.flowSensor.getData(fd) && fd.valid && !fd.stale &&
               fd.ageMs <= MAX_SAMPLE_AGE_MS;
    }

    void fail(Context &ctx, const char *message)
    {
        ctx.components.motor.setPwmImmediate(0);
        failure = message;
        state = FAILED;
    }

    void startMotor(Context &ctx, int index)
    {
        motor = index;
        sum = 0.0f;
        samples = 0;
        ctx.components.motor.setMotorPwm(index, MAX_PWM);
        ctx.components.motor.setMotorPwm(1 - index, 0);
        stateStartMs = millis();
        state = SETTLE;
        LOG_F("Prueba de bombas: motor %d a tope\n", index + 1);
    }

    void finishMotor(Context &ctx)
    {
        if (samples < MIN_SAMPLES)
        {
            fail(ctx, "Sin dato de flujo");
            return;
        }
        flow[motor] = sum / samples;
        LOG_F("Prueba de bombas: motor %d = %.3f L/min (%d muestras)\n", motor + 1, flow[motor], samples);
        if (motor == 0)
        {
            startMotor(ctx, 1);
            return;
        }

        ctx.components.motor.setPwmImmediate(0);
        if (!PumpOrderModel::decide(flow[0], flow[1], weak))
        {
            fail(ctx, "Sin caudal medible");
            return;
        }
        saved = ctx.calibration.savePumpOrder(weak, flow[0], flow[1]);
        LOG_F("Prueba de bombas: motor debil %d, fuerte %d (%s)\n", weak + 1, 2 - weak,
              saved ? "guardado" : "solo en RAM");
        // La semilla de la curva solo sirve si se midió con este mismo orden.
        if (ctx.calibration.isLoaded && ctx.calibration.weakMotor != weak)
            LOG_LN("Curva medida con otro orden: se ignora su semilla hasta recalibrar");
        state = RESULT;
    }

    void render(Context &ctx)
    {
        Renderer::beginFrame(ctx.u8g2);
        GUI::displayHeader(ctx);
        char line[28];

        if (state == RESULT)
        {
            Renderer::centeredText(ctx.u8g2, "Orden de bombas", Layout::CenterX, 24, FONT_TITLE);
            snprintf(line, sizeof(line), "Debil: motor %d", weak + 1);
            Renderer::centeredText(ctx.u8g2, line, Layout::CenterX, 38, FONT_VALUE);
            snprintf(line, sizeof(line), "M1 %.2f  M2 %.2f", flow[0], flow[1]);
            Renderer::centeredText(ctx.u8g2, line, Layout::CenterX, 48, FONT_VALUE);
            if (!saved)
                Renderer::centeredText(ctx.u8g2, "No se guardo", Layout::CenterX, 56, FONT_VALUE);
            else
                Renderer::centeredText(ctx.u8g2, "Volver", Layout::CenterX, Layout::FooterValueY, FONT_VALUE);
        }
        else if (state == FAILED)
        {
            Renderer::centeredText(ctx.u8g2, "Prueba fallida", Layout::CenterX, 24, FONT_TITLE);
            Renderer::centeredText(ctx.u8g2, failure, Layout::CenterX, 38, FONT_VALUE);
            Renderer::centeredText(ctx.u8g2, "Se usa motor 0 primero", Layout::CenterX, 48, FONT_VALUE);
            Renderer::centeredText(ctx.u8g2, "Volver", Layout::CenterX, Layout::FooterValueY, FONT_VALUE);
        }
        else
        {
            Renderer::centeredText(ctx.u8g2, "Probando bombas", Layout::CenterX, 24, FONT_TITLE);
            if (state == WAIT_SENSOR)
            {
                Renderer::centeredText(ctx.u8g2, "Esperando caudal", Layout::CenterX, 38, FONT_VALUE);
            }
            else
            {
                snprintf(line, sizeof(line), "Motor %d a tope", motor + 1);
                Renderer::centeredText(ctx.u8g2, line, Layout::CenterX, 38, FONT_VALUE);
                const uint32_t total = 2 * (SETTLE_MS + SAMPLE_MS);
                const uint32_t done = motor * (SETTLE_MS + SAMPLE_MS) + (millis() - stateStartMs) +
                                      (state == SAMPLE ? SETTLE_MS : 0);
                Renderer::progressBar(ctx.u8g2, (Layout::ScreenW - 100) / 2, 42, 100, 7,
                                      static_cast<int>(done * 100UL / total));
            }
            Renderer::centeredText(ctx.u8g2, "Boton: cancelar", Layout::CenterX, Layout::FooterValueY, FONT_VALUE);
        }
        Renderer::endFrame(ctx.u8g2);
    }

public:
    static constexpr const char *Name = "pump_order";

    uint16_t frameIntervalMs() const override { return 100; }

    void enter(Context &ctx) override
    {
        ctx.components.motor.setPwmImmediate(0);
        flow[0] = flow[1] = 0.0f;
        weak = -1;
        saved = false;
        stateStartMs = millis();
        state = WAIT_SENSOR;
    }

    void exit(Context &ctx) override { ctx.components.motor.setPwmImmediate(0); }

    void update(Context &ctx) override
    {
        const bool pressed = ctx.components.input.isButtonPressed();
        if (state == RESULT || state == FAILED)
        {
            if (pressed)
            {
                SceneManager::setScene("inicio", ctx);
                return;
            }
            render(ctx);
            return;
        }
        if (pressed)
        {
            ctx.components.motor.setPwmImmediate(0);
            SceneManager::setScene("inicio", ctx);
            return;
        }

        FlowData fd;
        switch (state)
        {
        case WAIT_SENSOR:
            if (freshFlow(ctx, fd))
                startMotor(ctx, 0);
            else if (millis() - stateStartMs >= WAIT_SENSOR_MS)
                fail(ctx, "Sin dato de flujo");
            break;
        case SETTLE:
            if (millis() - stateStartMs >= SETTLE_MS)
            {
                lastSampleId = 0;
                stateStartMs = millis();
                state = SAMPLE;
            }
            break;
        case SAMPLE:
            if (freshFlow(ctx, fd) && fd.sampleId != lastSampleId && isfinite(fd.flow))
            {
                lastSampleId = fd.sampleId;
                sum += fd.flow;
                ++samples;
            }
            if (millis() - stateStartMs >= SAMPLE_MS)
                finishMotor(ctx);
            break;
        default:
            break;
        }
        render(ctx);
    }
};

#endif
