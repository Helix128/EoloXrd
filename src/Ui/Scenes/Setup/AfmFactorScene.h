#ifndef AFM_FACTOR_SCENE_H
#define AFM_FACTOR_SCENE_H

#ifdef FEATURE_FLOW_AFM07

#include "../BaseMenuScene.h"

// Escala del AFM07 de este equipo: cada unidad puede entregar el registro de
// caudal dividido por 10, 100 o 1000. Menú como el resto de Opciones:
//   Caudal    lectura en vivo (solo informa)
//   Factor    clic para pasar al siguiente divisor; se guarda en NVS
//   Bomba     enciende o apaga al 50 % para poder medir
//   Detectar  elige el divisor que deja el caudal entre 1,5 y 15 L/min
// Las acciones del menú son punteros sin captura, por eso el estado es estático
// (la escena es única).
class AfmFactorScene : public BaseMenuScene
{
public:
    static constexpr const char *Name = "afm_factor";

    void enter(Context &ctx) override
    {
        pumpOn = false;
        message[0] = '\0';
        ctx.components.motor.setPwmImmediate(0);
        clearOptions();
        refreshLabels(ctx);
        addOption(labels[RowReading], [](Context &) {});
        addOption(labels[RowFactor], [](Context &c) {
            applyDivisor(c, AFM07Model::nextDivisor(c.components.flowSensor.divisor()));
        });
        addOption(labels[RowPump], [](Context &c) {
            pumpOn = !pumpOn;
            c.components.motor.setPwmImmediate(pumpOn ? MAX_PWM / 2 : 0);
        });
        addOption(labels[RowDetect], [](Context &c) { detect(c); });
        addOption("Volver", [](Context &c) { SceneManager::setScene("opciones", c); });
        _selectIndex = RowFactor; // la primera fila solo informa
    }

    void exit(Context &ctx) override { ctx.components.motor.setPwmImmediate(0); }

protected:
    void preDraw(Context &ctx) override { refreshLabels(ctx); }

private:
    enum Row { RowReading, RowFactor, RowPump, RowDetect };

    static constexpr uint32_t MessageMs = 2500;
    static inline bool pumpOn = false;
    static inline char message[20] = "";
    static inline uint32_t messageUntilMs = 0;
    char labels[RowDetect + 1][24] = {};

    static void say(const char *text)
    {
        snprintf(message, sizeof(message), "%s", text);
        messageUntilMs = millis() + MessageMs;
    }

    static void applyDivisor(Context &ctx, float divisor)
    {
        say(ctx.components.flowSensor.setDivisor(divisor) ? "Factor guardado" : "No se pudo guardar");
    }

    static void detect(Context &ctx)
    {
        if (!pumpOn)
        {
            say("Enciende la bomba");
            return;
        }
        const float divisor = AFM07Model::detectDivisor(ctx.components.flowSensor.lastRaw());
        if (divisor == 0.0f)
            say("Sin lectura clara");
        else
            applyDivisor(ctx, divisor);
    }

    void refreshLabels(Context &ctx)
    {
        if (message[0] != '\0' && (int32_t)(millis() - messageUntilMs) >= 0)
            message[0] = '\0';

        FlowData flow;
        if (ctx.components.flowSensor.getData(flow) && flow.valid && !flow.stale)
            snprintf(labels[RowReading], sizeof(labels[RowReading]), "%.2f L/min (%u)", flow.flow,
                     (unsigned)ctx.components.flowSensor.lastRaw());
        else
            snprintf(labels[RowReading], sizeof(labels[RowReading]), "Sin lectura");
        snprintf(labels[RowFactor], sizeof(labels[RowFactor]), "Factor: /%d", (int)ctx.components.flowSensor.divisor());
        snprintf(labels[RowPump], sizeof(labels[RowPump]), "Bomba: %s", pumpOn ? "encendida" : "apagada");
        snprintf(labels[RowDetect], sizeof(labels[RowDetect]), "%s", message[0] ? message : "Detectar factor");
    }
};

#endif // FEATURE_FLOW_AFM07
#endif
