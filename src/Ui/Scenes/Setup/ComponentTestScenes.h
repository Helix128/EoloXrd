#ifndef EOLO_COMPONENT_TEST_SCENES_H
#define EOLO_COMPONENT_TEST_SCENES_H

#include "../../Drawing/Fonts.h"
#include "../BaseMenuScene.h"
#include "../../../Common/Data/Context.h"
#include "../../Drawing/Renderer.h"
#include "../../Drawing/Units.h"

// Pruebas de componentes. Solo se ofrece lo que la variante compilada tiene:
// las paginas de sensores salen de los FEATURE_* y los motores de MotorManager.
class ComponentSensorsScene : public IScene
{
    enum Kind : uint8_t
    {
        Flow,
        Bme,
#ifdef FEATURE_PLANTOWER
        Plant,
#endif
#ifdef FEATURE_ANEMOMETER
        Wind,
#endif
        Battery
    };
    static constexpr Kind kinds[] = {
        Flow,
        Bme,
#ifdef FEATURE_PLANTOWER
        Plant,
#endif
#ifdef FEATURE_ANEMOMETER
        Wind,
#endif
        Battery};
    static constexpr int pages = sizeof(kinds) / sizeof(kinds[0]);

    int page = 0;

    static const char *flowState(const FlowData &d) { return !d.valid ? "Sin dato" : d.stale ? "Obsoleto" : "OK"; }

public:
    static constexpr const char *Name = "test_sensors";
    void enter(Context &) override { page = 0; }
    void update(Context &ctx) override
    {
        int delta = ctx.components.input.getEncoderDelta();
        if (delta) page = constrain(page + delta, 0, pages - 1);
        if (ctx.components.input.isButtonPressed()) { SceneManager::setScene("test_components", ctx); return; }

        char state[16] = "";
        char v1[24] = "", v2[24] = "", v3[24] = "";
        const char *title = "";
        const char *l1 = nullptr, *l2 = nullptr, *l3 = nullptr;

        switch (kinds[page])
        {
        case Flow:
        {
            FlowData d; const bool ok = ctx.components.flowSensor.getData(d);
            title = "Caudal\xEDmetro";
            snprintf(state, sizeof(state), "%s", ok ? flowState(d) : "Sin dato");
            l1 = "Flujo";
            if (ok && d.valid) snprintf(v1, sizeof(v1), "%.2f " UNIT_FLOW, d.flow); else snprintf(v1, sizeof(v1), "--");
            break;
        }
        case Bme:
        {
            BME280Data d; const bool ok = ctx.components.bme.getData(d) && d.valid;
            title = "BME280";
            snprintf(state, sizeof(state), "%s", ok ? "OK" : "Sin dato");
            l1 = "Temp"; l2 = "Humedad"; l3 = "Presi\xF3n";
            if (ok)
            {
                snprintf(v1, sizeof(v1), "%.1f " UNIT_TEMP, d.temperature);
                snprintf(v2, sizeof(v2), "%.0f " UNIT_PCT, d.humidity);
                snprintf(v3, sizeof(v3), "%.0f " UNIT_PRESSURE, d.pressure);
            }
            else { snprintf(v1, sizeof(v1), "--"); snprintf(v2, sizeof(v2), "--"); snprintf(v3, sizeof(v3), "--"); }
            break;
        }
#ifdef FEATURE_PLANTOWER
        case Plant:
        {
            PlantowerData d; const bool ok = ctx.components.plantower.getData(d) && d.valid;
            title = "Plantower";
            snprintf(state, sizeof(state), "%s", ok ? "OK" : "Sin dato");
            l1 = "PM1.0"; l2 = "PM2.5"; l3 = "PM10";
            if (ok)
            {
                snprintf(v1, sizeof(v1), "%u " UNIT_PM, d.pm1_0);
                snprintf(v2, sizeof(v2), "%u " UNIT_PM, d.pm2_5);
                snprintf(v3, sizeof(v3), "%u " UNIT_PM, d.pm10_0);
            }
            else { snprintf(v1, sizeof(v1), "--"); snprintf(v2, sizeof(v2), "--"); snprintf(v3, sizeof(v3), "--"); }
            break;
        }
#endif
#ifdef FEATURE_ANEMOMETER
        case Wind:
        {
            AnemometerData d; const bool ok = ctx.components.anemometer.getData(d);
            title = "Anem\xF3metro";
            snprintf(state, sizeof(state), "%s", ok ? (d.fresh ? "OK" : "Obsoleto") : "Sin conexi\xF3n");
            l1 = "Viento"; l2 = "Direcci\xF3n";
            if (ok)
            {
                snprintf(v1, sizeof(v1), "%.1f km/h", d.windKph);
                snprintf(v2, sizeof(v2), "%d" UNIT_DEG, d.direction);
            }
            else { snprintf(v1, sizeof(v1), "--"); snprintf(v2, sizeof(v2), "--"); }
            break;
        }
#endif
        case Battery:
        {
            title = "Bater\xED" "a";
#ifdef FEATURE_DUAL_BATTERY
            bool ok = ctx.components.battery.hasValidData();
            snprintf(state, sizeof(state), "%s", ok ? (ctx.components.battery.isStale() ? "Obsoleto" : "OK") : "Sin dato");
            l1 = "Bater\xED" "a 1"; l2 = "Bater\xED" "a 2"; l3 = "Fuente DC";
            snprintf(v1, sizeof(v1), "%.1f V", ctx.components.battery.getBatteryVoltage(0));
            snprintf(v2, sizeof(v2), "%.1f V", ctx.components.battery.getBatteryVoltage(1));
            snprintf(v3, sizeof(v3), "%.1f V", ctx.components.battery.getDCVoltage());
#else
            float v = ctx.components.battery.getVoltage();
            snprintf(state, sizeof(state), "%s", v >= 0.0f ? "OK" : "Sin dato");
            l1 = "Voltaje";
            if (v >= 0.0f) snprintf(v1, sizeof(v1), "%.2f V", v); else snprintf(v1, sizeof(v1), "--");
#endif
            break;
        }
        }

        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);
        ctx.u8g2.setFont(FONT_TITLE);
        ctx.u8g2.drawStr(Layout::MarginX, Layout::KvFirstY, title);

        Renderer::KeyValue rows[4];
        int n = 0;
        rows[n++] = {"Estado", state};
        if (l1) rows[n++] = {l1, v1};
        if (l2) rows[n++] = {l2, v2};
        if (l3) rows[n++] = {l3, v3};
        Renderer::keyValueRows(ctx.u8g2, rows, n, 1);

        char counter[8];
        snprintf(counter, sizeof(counter), "%d/%d", page + 1, pages);
        ctx.u8g2.setFont(FONT_VALUE);
        ctx.u8g2.drawStr(Layout::ScreenW - Layout::MarginX - ctx.u8g2.getStrWidth(counter), Layout::KvFirstY, counter);
        ctx.u8g2.sendBuffer();
    }
};

class ComponentMotorsScene : public IScene
{
    // Los indices son los del MotorManager (el pinout de cada perfil define el
    // destino fisico). Foco: 0..Motors-1 = motor, Motors = volver.
    static constexpr int Motors = MotorManager::motorCount;
    static_assert(Motors >= 1 && Motors <= 2, "La pantalla de prueba admite 1 o 2 motores");

    int focus = 0;
    int pwm[Motors] = {};

    // Triangulo de foco a la izquierda de la fila.
    static void focusMarker(Context &ctx, int row)
    {
        int y = Layout::KvFirstY + row * Layout::KvPitch;
        ctx.u8g2.drawTriangle(Layout::MarginX - 1, y - 6, Layout::MarginX - 1, y, Layout::MarginX + 2, y - 3);
    }

public:
    static constexpr const char *Name = "test_motors";
    uint16_t frameIntervalMs() const override { return 80; }
    void enter(Context &ctx) override { for (int i = 0; i < Motors; i++) pwm[i] = 0; focus = 0; ctx.components.motor.setPwmImmediate(0); }
    void exit(Context &ctx) override { ctx.components.motor.setPwmImmediate(0); }
    void update(Context &ctx) override
    {
        int delta = ctx.components.input.getEncoderDelta();
        if (delta && focus < Motors) {
            int stepPct = abs(delta) >= 3 ? 5 : 1;
            pwm[focus] = constrain(pwm[focus] + (delta > 0 ? stepPct : -stepPct) * MAX_PWM / 100, 0, MAX_PWM);
            ctx.components.motor.setMotorPwmImmediate(focus, pwm[focus]);
        }
        if (ctx.components.input.isButtonPressed()) {
            if (focus == Motors) { SceneManager::setScene("test_components", ctx); return; }
            ++focus;
            if (focus == Motors) ctx.components.motor.setPwmImmediate(0);
        }

        ctx.u8g2.clearBuffer();
        GUI::displayHeader(ctx);
        Renderer::centeredText(ctx.u8g2, "Prueba de motores", Layout::CenterX, Layout::KvFirstY, FONT_TITLE);

        char labels[Motors][10];
        char values[Motors][16];
        Renderer::KeyValue rows[Motors + 2];
        int n = 0;
        for (int i = 0; i < Motors; i++) {
            if (Motors == 1) snprintf(labels[i], sizeof(labels[i]), "Motor");
            else snprintf(labels[i], sizeof(labels[i]), "Motor %d", i + 1);
            snprintf(values[i], sizeof(values[i]), "%3d%% %4d", pwm[i] * 100 / MAX_PWM, pwm[i]);
            rows[n++] = {labels[i], values[i]};
        }

        FlowData flow; const bool ok = ctx.components.flowSensor.getData(flow) && flow.valid && !flow.stale;
        char flowText[16];
        if (ok) snprintf(flowText, sizeof(flowText), "%.2f " UNIT_FLOW, flow.flow); else snprintf(flowText, sizeof(flowText), "--");
        rows[n++] = {"Flujo", flowText};
        rows[n++] = {"", "Volver"};
        Renderer::keyValueRows(ctx.u8g2, rows, n, 1);

        focusMarker(ctx, 1 + (focus < Motors ? focus : Motors + 1));
        ctx.u8g2.sendBuffer();
    }
};

class ComponentTestMenuScene : public BaseMenuScene
{
public:
    static constexpr const char *Name = "test_components";
    void enter(Context &ctx) override {
        ctx.components.motor.setPwmImmediate(0); clearOptions();
        addOption("Sensores", [](Context &c) { SceneManager::setScene(ComponentSensorsScene::Name, c); });
        addOption(MotorManager::motorCount > 1 ? "Motores" : "Motor", [](Context &c) { SceneManager::setScene(ComponentMotorsScene::Name, c); });
        addOption("Volver", [](Context &c) { SceneManager::setScene("inicio", c); });
    }
};

#endif
