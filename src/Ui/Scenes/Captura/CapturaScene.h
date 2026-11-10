#ifndef CAPTURA_SCENE_H
#define CAPTURA_SCENE_H

#include "../../Drawing/Fonts.h"
#include "../IScene.h"
#include "../../../Common/Data/Context.h"
#include "../../Drawing/SceneManager.h"
#include "../../Drawing/GUI.h"
#include "../../Drawing/Renderer.h"
#include "../../Drawing/Units.h"
#include "../../../Variants/Legacy.h"

// Escena de logo/splash al iniciar la app
class CapturaScene : public IScene
{
private:
#ifdef FEATURE_ANEMOMETER
    static constexpr int FooterPageCount = 7;
#else
    static constexpr int FooterPageCount = 6;
#endif

public:
    static constexpr const char *Name = "captura";

    uint16_t frameIntervalMs() const override { return 250; }

    void enter(Context &ctx) override
    {
        if (ctx.isCapturePaused())
        {
            LOG_LN("Resumiendo captura.");
            ctx.resumeCapture();
            LOG_LN("Captura reanudada.");
        }
        else if (!ctx.isCaptureActive())
        {
            LOG_LN("Iniciando captura.");
            ctx.beginCapture();
            LOG_LN("Captura iniciada.");
        }
    }

    void update(Context &ctx) override
    {
        Renderer::beginFrame(ctx.u8g2);
        GUI::displayHeader(ctx);

        int delta = ctx.components.input.getEncoderDelta();
        bool button = ctx.components.input.isButtonPressed();

        if (delta != 0)
        {
            // Avanza una página por paso del encoder saltando las que no
            // aplican (la 0 siempre existe, así que el ciclo termina).
            const int direction = delta > 0 ? 1 : -1;
            for (int steps = abs(delta); steps > 0; --steps)
            {
                do
                {
                    cycleFooter = (cycleFooter + direction + FooterPageCount) % FooterPageCount;
                } while (!footerPageAvailable(ctx, cycleFooter));
            }
        }
        if (!footerPageAvailable(ctx, cycleFooter))
            cycleFooter = 0;

        if (button)
        {
            ctx.saveSession();
            SceneManager::setScene("captura_menu", ctx);
        }

        footer(ctx);
        mainPanel(ctx);

        Renderer::endFrame(ctx.u8g2);
    }

    void mainPanel(Context &ctx)
    {   
        const UiSnapshot &snapshot = ctx.getUiSnapshot();
        float lmin = snapshot.flow.valid ? snapshot.flow.flow : -1.0f;
        float volume = snapshot.flow.capturedVolume; 

        char lminStr[10];
        char volStr[10];
        if (lmin < 0) snprintf(lminStr, sizeof(lminStr), "--");
        else snprintf(lminStr, sizeof(lminStr), "%.2f", lmin);
        snprintf(volStr, sizeof(volStr), "%.3f", volume*0.001);

        // Unidades en las etiquetas: los valores conservan su tamano protagonista.
        Renderer::metricRow(ctx.u8g2, "Flujo " UNIT_FLOW, lminStr, nullptr,
                            "Vol. " UNIT_VOLUME, volStr, nullptr, nullptr, nullptr, nullptr,
                            Layout::HeroLabelY, Layout::HeroValueY, FONT_DISPLAY);
    }

    int cycleFooter = 0;

    // PM solo existe con Plantower compilado y habilitado en la sesión; sin él
    // la página quedaba en blanco.
    static bool footerPageAvailable(Context &ctx, int page)
    {
        return page != 1 || ctx.getUiSnapshot().airQuality.enabled;
    }

    void footer(Context &ctx)
    {
        Renderer::separatorLine(ctx.u8g2, Layout::SeparatorY);
        const UiSnapshot &snapshot = ctx.getUiSnapshot();

        switch (cycleFooter)
        {
        case 0: // BME: temperatura, humedad y presion siempre juntas.
        {
            if (!snapshot.environment.valid)
            {
                Renderer::metricRow(ctx.u8g2, "Temp " UNIT_TEMP, "--", nullptr,
                                    "Hum " UNIT_PCT, "--", nullptr,
                                    "P " UNIT_PRESSURE, "--", nullptr);
                break;
            }
            char tempStr[8];
            char humStr[8];
            char pressureStr[8];
            snprintf(tempStr, sizeof(tempStr), "%.2f", snapshot.environment.temperature);
            snprintf(humStr, sizeof(humStr), "%.1f", snapshot.environment.humidity);
            snprintf(pressureStr, sizeof(pressureStr), "%.0f", snapshot.environment.pressure);
            // Unidades en las etiquetas para que los valores quepan en tres columnas.
            Renderer::metricRow(ctx.u8g2, "Temp " UNIT_TEMP, tempStr, nullptr,
                                "Hum " UNIT_PCT, humStr, nullptr,
                                "P " UNIT_PRESSURE, pressureStr, nullptr);
            break;
        }
        case 1: // Los tres valores PM juntos en columnas compactas.
        {
            const bool valid = snapshot.airQuality.valid;
            char pm1Str[8];
            char pm25Str[8];
            char pm10Str[8];
            if (valid)
            {
                snprintf(pm1Str, sizeof(pm1Str), "%.0f", snapshot.airQuality.pm1);
                snprintf(pm25Str, sizeof(pm25Str), "%.0f", snapshot.airQuality.pm25);
                snprintf(pm10Str, sizeof(pm10Str), "%.0f", snapshot.airQuality.pm10);
            }
            else
            {
                snprintf(pm1Str, sizeof(pm1Str), "--");
                snprintf(pm25Str, sizeof(pm25Str), "--");
                snprintf(pm10Str, sizeof(pm10Str), "--");
            }
            const int columns[] = {0, Layout::ThirdColumnW, 2 * Layout::ThirdColumnW};
            const char *labels[] = {"PM1.0", "PM2.5", "PM10"};
            const char *values[] = {pm1Str, pm25Str, pm10Str};
            for (int i = 0; i < 3; ++i)
            {
                const int width = i == 2 ? Layout::ScreenW - columns[i] : Layout::ThirdColumnW;
                ctx.u8g2.setFont(FONT_VALUE);
                ctx.u8g2.drawStr(columns[i] + (width - ctx.u8g2.getStrWidth(labels[i])) / 2, 53, labels[i]);
                const int valueWidth = ctx.u8g2.getStrWidth(values[i]);
                const int unitWidth = ctx.u8g2.getStrWidth(UNIT_PM);
                const int regularWidth = valueWidth + unitWidth + 1;
                const uint8_t *valueFont = regularWidth <= width ? FONT_VALUE : u8g2_font_4x6_tf;
                const uint8_t *unitFont = regularWidth <= width ? FONT_VALUE : u8g2_font_4x6_tf;
                const char *unitText = regularWidth <= width ? UNIT_PM : "ug/m3";
                ctx.u8g2.setFont(valueFont);
                const int compactValueWidth = ctx.u8g2.getStrWidth(values[i]);
                ctx.u8g2.setFont(unitFont);
                const int compactUnitWidth = ctx.u8g2.getStrWidth(unitText);
                const int totalWidth = compactValueWidth + compactUnitWidth + 1;
                const int valueX = columns[i] + (width - totalWidth) / 2;
                ctx.u8g2.setFont(valueFont);
                ctx.u8g2.drawStr(valueX, 61, values[i]);
                ctx.u8g2.setFont(unitFont);
                ctx.u8g2.drawStr(valueX + compactValueWidth + 1, 61, unitText);
            }
            ctx.u8g2.drawVLine(Layout::ThirdColumnW, Layout::SeparatorY + 3,
                               Layout::BodyBottom - 4 - (Layout::SeparatorY + 3) + 1);
            ctx.u8g2.drawVLine(2 * Layout::ThirdColumnW, Layout::SeparatorY + 3,
                               Layout::BodyBottom - 4 - (Layout::SeparatorY + 3) + 1);
            break;
        }
        case 2: // flujo configurado
        {
            char targetFlowStr[10];
            snprintf(targetFlowStr, sizeof(targetFlowStr), "%.1f", snapshot.flow.targetFlow);
            Renderer::metricCell(ctx.u8g2, 0, Layout::ScreenW, "Flujo configurado", targetFlowStr, UNIT_FLOW,
                                 Layout::FooterLabelY, Layout::FooterValueY);
            break;
        }
        case 3: // tiempo transcurrido
        {
            char timeStr[12];
            formatHms(timeStr, sizeof(timeStr), ctx.session.elapsedTime);
            Renderer::metricCell(ctx.u8g2, 0, Layout::ScreenW, "Tiempo transcurrido", timeStr, nullptr,
                                 Layout::FooterLabelY, Layout::FooterValueY);
            break;
        }
        case 4: // tiempo restante
        {
            char remainingStr[12];
            formatHms(remainingStr, sizeof(remainingStr),
                      ctx.session.duration > ctx.session.elapsedTime ? ctx.session.duration - ctx.session.elapsedTime : 0);
            Renderer::metricCell(ctx.u8g2, 0, Layout::ScreenW, "Tiempo restante", remainingStr, nullptr,
                                 Layout::FooterLabelY, Layout::FooterValueY);
            break;
        }
        case 5: // tiempos de inicio y fin
        {
            DateTime start(ctx.session.startUnix);
            DateTime end = start + ctx.session.duration;
            String startStr = start.timestamp(DateTime::TIMESTAMP_TIME);
            String endStr = end.timestamp(DateTime::TIMESTAMP_TIME);
            Renderer::metricRow(ctx.u8g2, "Inicio", startStr.c_str(), nullptr, "Fin", endStr.c_str(), nullptr);
            break;
        }
#ifdef FEATURE_ANEMOMETER
        case 6:
        {
            if (!snapshot.wind.valid)
            {
                Renderer::metricRow(ctx.u8g2, "Velocidad", "--", "", "Direcci\xF3n", "--", "");
                break;
            }

            char speedStr[8];
            char directionStr[5];
            snprintf(speedStr, sizeof(speedStr), "%.1f", snapshot.wind.speed);
            snprintf(directionStr, sizeof(directionStr), "%d", snapshot.wind.direction);
            Renderer::metricRow(ctx.u8g2, "Velocidad", speedStr, UNIT_WIND, "Direcci\xF3n", directionStr, UNIT_DEG);
            break;
        }
#endif
        }

        int shown = 0;
        int current = 0;
        for (int page = 0; page < FooterPageCount; page++)
        {
            if (!footerPageAvailable(ctx, page))
                continue;
            if (page == cycleFooter)
                current = shown;
            shown++;
        }
        Renderer::pageDots(ctx.u8g2, shown, current, Layout::BodyBottom - 1);
    }
};
#endif
