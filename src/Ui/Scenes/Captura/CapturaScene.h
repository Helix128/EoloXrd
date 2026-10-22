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
        snprintf(lminStr, sizeof(lminStr), "%.2f", lmin);
        snprintf(volStr, sizeof(volStr), "%.3f", volume*0.001);

        Renderer::metricRow(ctx.u8g2, "Flujo", lminStr, UNIT_FLOW, "Vol. captura", volStr, UNIT_VOLUME, nullptr, nullptr, nullptr, Layout::HeroLabelY, Layout::HeroValueY);
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
        case 0: // temp hum presion
        {
            float temp = snapshot.environment.temperature;
            float hum = snapshot.environment.humidity;
            float presAtm = snapshot.environment.pressure;


            char tempStr[6];
            char humStr[5];
            char presStr[5];
            snprintf(tempStr, sizeof(tempStr), "%.2f", temp);
            snprintf(humStr, sizeof(humStr), "%.1f", hum);
            snprintf(presStr, sizeof(presStr), "%.0f", presAtm);

            Renderer::metricRow(ctx.u8g2, "Temp", tempStr, UNIT_TEMP, "Hum", humStr, UNIT_PCT, "Presion", presStr, UNIT_PRESSURE);
            break;
        }
        case 1: // pm1 pm2.5 pm10
        {
            float pm1 = snapshot.airQuality.valid ? snapshot.airQuality.pm1 : 0.0f;
            float pm25 = snapshot.airQuality.valid ? snapshot.airQuality.pm25 : 0.0f;
            float pm10 = snapshot.airQuality.valid ? snapshot.airQuality.pm10 : 0.0f;


            char pm1Str[5];
            char pm25Str[5];
            char pm10Str[5];

            snprintf(pm1Str, sizeof(pm1Str), "%.0f", pm1);
            snprintf(pm25Str, sizeof(pm25Str), "%.0f", pm25);
            snprintf(pm10Str, sizeof(pm10Str), "%.0f", pm10);
            Renderer::metricRow(ctx.u8g2, "PM1.0", pm1Str, UNIT_PM, "PM2.5", pm25Str, UNIT_PM, "PM10", pm10Str, UNIT_PM);
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
            formatHms(remainingStr, sizeof(remainingStr), ctx.session.duration - ctx.session.elapsedTime);
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
                Renderer::metricRow(ctx.u8g2, "Velocidad", "--", "", "Direccion", "--", "");
                break;
            }

            char speedStr[8];
            char directionStr[5];
            snprintf(speedStr, sizeof(speedStr), "%.1f", snapshot.wind.speed);
            snprintf(directionStr, sizeof(directionStr), "%d", snapshot.wind.direction);
            Renderer::metricRow(ctx.u8g2, "Velocidad", speedStr, UNIT_WIND, "Direccion", directionStr, UNIT_DEG);
            break;
        }
#endif
        }
    }
};
#endif
