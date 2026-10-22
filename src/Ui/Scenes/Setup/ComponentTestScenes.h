#ifndef EOLO_COMPONENT_TEST_SCENES_H
#define EOLO_COMPONENT_TEST_SCENES_H

#include "../../Drawing/Fonts.h"
#include "../BaseMenuScene.h"
#include "../../../Common/Data/Context.h"
#include "../../Drawing/Renderer.h"
#include "../../Drawing/Units.h"

class ComponentSensorsScene : public IScene
{
    int page = 0;
    static const char *flowState(const FlowData &d) { return !d.valid ? "SIN DATO" : d.stale ? "OBSOLETO" : "OK"; }
public:
    static constexpr const char *Name = "test_sensors";
    void enter(Context &) override { page = 0; }
    void update(Context &ctx) override {
        const int pages =
#ifdef FEATURE_PLANTOWER
            4
#else
            3
#endif
#ifdef FEATURE_ANEMOMETER
            + 1
#endif
            ;
        int delta = ctx.components.input.getEncoderDelta();
        if (delta) page = constrain(page + delta, 0, pages - 1);
        if (ctx.components.input.isButtonPressed()) { SceneManager::setScene("test_components", ctx); return; }
        ctx.u8g2.clearBuffer(); GUI::displayHeader(ctx); 
        char line[40];
        if (page == 0) {
            FlowData d; const bool ok = ctx.components.flowSensor.getData(d);
            Renderer::centeredText(ctx.u8g2, "Caudalimetro", Layout::CenterX, Layout::HeroLabelY, FONT_LABEL);
            snprintf(line, sizeof(line), "%s  %.2f L/min", ok ? flowState(d) : "SIN DATO", ok ? d.flow : 0.0f);
        } else if (page == 1) {
            BME280Data d; bool ok = ctx.components.bme.getData(d);
            Renderer::centeredText(ctx.u8g2, "BME280", Layout::CenterX, Layout::HeroLabelY, FONT_LABEL);
            snprintf(line, sizeof(line), "%s  T %.1f" UNIT_TEMP " H %.0f%%", ok ? "OK" : "SIN DATO", ok ? d.temperature : 0.0f, ok ? d.humidity : 0.0f);
#ifdef FEATURE_PLANTOWER
        } else if (page == 2) {
            PlantowerData d; bool ok = ctx.components.plantower.getData(d);
            Renderer::centeredText(ctx.u8g2, "Plantower", Layout::CenterX, Layout::HeroLabelY, FONT_LABEL);
            snprintf(line, sizeof(line), "%s PM2.5 %u", ok ? "OK" : "SIN DATO", ok ? d.pm2_5 : 0);
#endif
        }
#ifdef FEATURE_ANEMOMETER
        else if (page == pages - 1) {
            AnemometerData d; bool ok = ctx.components.anemometer.getData(d);
            Renderer::centeredText(ctx.u8g2, "Anemometro", Layout::CenterX, Layout::HeroLabelY, FONT_LABEL);
            snprintf(line, sizeof(line), "%s %.1f km/h", ok ? (d.fresh ? "OK" : "OBSOLETO") : "OFFLINE", ok ? d.windKph : 0.0f);
        }
#endif
        else {
            Renderer::centeredText(ctx.u8g2, "Bateria", Layout::CenterX, Layout::HeroLabelY, FONT_LABEL);
#ifdef FEATURE_DUAL_BATTERY
            bool ok = ctx.components.battery.hasValidData();
            snprintf(line, sizeof(line), "%s B1 %.1f B2 %.1f DC %.1f", ok ? (ctx.components.battery.isStale() ? "OBSOLETO" : "OK") : "SIN DATO", ctx.components.battery.getBatteryVoltage(0), ctx.components.battery.getBatteryVoltage(1), ctx.components.battery.getDCVoltage());
#else
            float v = ctx.components.battery.getVoltage();
            snprintf(line, sizeof(line), "%s %.2f V", v >= 0.0f ? "OK" : "SIN DATO", v < 0 ? 0.0f : v);
#endif
        }
        Renderer::centeredText(ctx.u8g2, line, Layout::CenterX, Layout::FooterLabelY, FONT_VALUE);
        snprintf(line, sizeof(line), "%d/%d  boton: volver", page + 1, pages);
        Renderer::centeredText(ctx.u8g2, line, Layout::CenterX, Layout::FooterValueY, FONT_VALUE); ctx.u8g2.sendBuffer();
    }
};

class ComponentMotorsScene : public IScene
{
    // Los índices son los del MotorManager: en Standard M1/PWM0=GPIO25 y
    // M2/PWM1=GPIO14 (el pinout de cada perfil define el destino físico).
    int focus = 0; // 0 motor 1, 1 motor 2, 2 salir
    int pwm[2] = {0, 0};
public:
    static constexpr const char *Name = "test_motors";
    uint16_t frameIntervalMs() const override { return 80; }
    void enter(Context &ctx) override { pwm[0] = pwm[1] = 0; focus = 0; ctx.components.motor.setPwmImmediate(0); }
    void exit(Context &ctx) override { ctx.components.motor.setPwmImmediate(0); }
    void update(Context &ctx) override {
        int delta = ctx.components.input.getEncoderDelta();
        if (delta && focus < 2) {
            int stepPct = abs(delta) >= 3 ? 5 : 1;
            pwm[focus] = constrain(pwm[focus] + (delta > 0 ? stepPct : -stepPct) * MAX_PWM / 100, 0, MAX_PWM);
            ctx.components.motor.setMotorPwmImmediate(focus, pwm[focus]);
        }
        if (ctx.components.input.isButtonPressed()) {
            if (focus == 2) { SceneManager::setScene("test_components", ctx); return; }
            ++focus;
            if (focus == 2) ctx.components.motor.setPwmImmediate(0);
        }
        ctx.u8g2.clearBuffer(); GUI::displayHeader(ctx);
        FlowData flow; bool ok = ctx.components.flowSensor.getData(flow);
        Renderer::centeredText(ctx.u8g2, "Prueba de motores", Layout::CenterX, Layout::KvFirstY, FONT_TITLE);
        char label1[8], label2[8], value1[16], value2[16], flowText[24];
        snprintf(label1, sizeof(label1), "%sM1", focus == 0 ? "> " : "");
        snprintf(label2, sizeof(label2), "%sM2", focus == 1 ? "> " : "");
        snprintf(value1, sizeof(value1), "%4d %3d%%", pwm[0], pwm[0] * 100 / MAX_PWM);
        snprintf(value2, sizeof(value2), "%4d %3d%%", pwm[1], pwm[1] * 100 / MAX_PWM);
        snprintf(flowText, sizeof(flowText), "%s %.2f", ok ? (flow.stale ? "OBSOLETO" : "OK") : "SIN DATO", ok ? flow.flow : 0.0f);
        const Renderer::KeyValue rows[] = {{label1, value1}, {label2, value2}, {"Flujo", flowText}};
        Renderer::keyValueRows(ctx.u8g2, rows, 3, 1);
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
        addOption("Motores", [](Context &c) { SceneManager::setScene(ComponentMotorsScene::Name, c); });
        addOption("Volver", [](Context &c) { SceneManager::setScene("inicio", c); });
    }
};

#endif
