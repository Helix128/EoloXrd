#ifndef UI_RENDERER_H
#define UI_RENDERER_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "../Config/Legacy.h"
#include "Layout.h"

class Renderer
{
public:
    static void beginFrame(DisplayModel &display)
    {
        display.clearBuffer();
    }

    static void endFrame(DisplayModel &display)
    {
        display.sendBuffer();
    }

    static void centeredText(DisplayModel &display, const char *text, int centerX, int baselineY, const uint8_t *font)
    {
        display.setFont(font);
        int width = display.getStrWidth(text);
        display.drawStr(centerX - width / 2, baselineY, text);
    }

    static void metricCell(DisplayModel &display,
                           int x,
                           int width,
                           const char *label,
                           const char *value,
                           const char *unit,
                           int labelY,
                           int valueY,
                           const uint8_t *labelFont = FONT_BOLD_S,
                           const uint8_t *valueFont = FONT_REGULAR_S)
    {
        display.setFont(labelFont);
        int labelWidth = display.getStrWidth(label);
        display.drawStr(x + (width - labelWidth) / 2, labelY, label);

        display.setFont(valueFont);
        int valueWidth = display.getStrWidth(value);
        display.setFont(u8g2_font_4x6_tf);
        int unitWidth = (unit && unit[0]) ? display.getStrWidth(unit) : 0;
        int totalWidth = valueWidth + (unitWidth > 0 ? unitWidth + 1 : 0);
        int valueX = x + (width - totalWidth) / 2;

        display.setFont(valueFont);
        display.drawStr(valueX, valueY, value);
        if (unitWidth > 0)
        {
            display.setFont(u8g2_font_4x6_tf);
            display.drawStr(valueX + valueWidth + 1, valueY, unit);
        }
    }

    static void metricRow(DisplayModel &display,
                          const char *label1,
                          const char *value1,
                          const char *unit1,
                          const char *label2,
                          const char *value2,
                          const char *unit2,
                          const char *label3 = nullptr,
                          const char *value3 = nullptr,
                          const char *unit3 = nullptr,
                          int labelY = Layout::FooterLabelY,
                          int valueY = Layout::FooterValueY)
    {
        if (label3)
        {
            metricCell(display, 0, Layout::ThirdColumnW, label1, value1, unit1, labelY, valueY);
            metricCell(display, Layout::ThirdColumnW, Layout::ThirdColumnW, label2, value2, unit2, labelY, valueY);
            metricCell(display, 2 * Layout::ThirdColumnW, Layout::ScreenW - 2 * Layout::ThirdColumnW, label3, value3, unit3, labelY, valueY);
        }
        else
        {
            metricCell(display, 0, Layout::HalfColumnW, label1, value1, unit1, labelY, valueY);
            metricCell(display, Layout::HalfColumnW, Layout::HalfColumnW, label2, value2, unit2, labelY, valueY);
        }
    }

    static void valueSelector(DisplayModel &display,
                              const char *title,
                              const char *value,
                              int animationStep)
    {
        centeredText(display, title, Layout::CenterX, 30, FONT_BOLD_L);
        display.setFont(FONT_REGULAR);
        int valueWidth = display.getStrWidth(value);
        int valueX = (Layout::ScreenW - valueWidth) / 2;
        display.drawStr(valueX, 50, value);

        int cycle = (animationStep / 4) % 3;
        const int midY = 44;
        const int topY = 40;
        const int bottomY = 48;
        const int spacing = 8;
        const int offset = 3;
        display.drawTriangle(valueX - spacing - cycle, topY, valueX - spacing - cycle, bottomY, valueX - offset - cycle, midY);
        display.drawTriangle(valueX + valueWidth + spacing + cycle, topY, valueX + valueWidth + spacing + cycle, bottomY, valueX + valueWidth + offset + cycle, midY);
    }

    static void progressBar(DisplayModel &display, int x, int y, int width, int height, int pct)
    {
        pct = constrain(pct, 0, 100);
        display.drawRFrame(x, y, width, height, 3);
        int fillW = (pct * (width - 2)) / 100;
        if (fillW > 0)
        {
            display.drawBox(x + 1, y + 1, fillW, height - 2);
        }
    }

    static void separatorLine(DisplayModel &display, int y)
    {
        display.drawHLine(0, y, Layout::ScreenW);
    }
};

#endif
