#ifndef UI_RENDERER_H
#define UI_RENDERER_H

#include "Fonts.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include "../../Variants/Legacy.h"
#include "Layout.h"

class Renderer
{
public:
    static void beginFrame(EOLO_DISPLAY_MODEL &display)
    {
        display.clearBuffer();
    }

    static void endFrame(EOLO_DISPLAY_MODEL &display)
    {
        display.sendBuffer();
    }

    static void centeredText(EOLO_DISPLAY_MODEL &display, const char *text, int centerX, int baselineY, const uint8_t *font)
    {
        display.setFont(font);
        int width = display.getStrWidth(text);
        display.drawStr(centerX - width / 2, baselineY, text);
    }

    static void metricCell(EOLO_DISPLAY_MODEL &display,
                           int x,
                           int width,
                           const char *label,
                           const char *value,
                           const char *unit,
                           int labelY,
                           int valueY,
                           const uint8_t *labelFont = FONT_VALUE,
                           const uint8_t *valueFont = FONT_LABEL)
    {
        display.setFont(labelFont);
        int labelWidth = display.getStrWidth(label);
        display.drawStr(x + (width - labelWidth) / 2, labelY, label);

        display.setFont(valueFont);
        int valueWidth = display.getStrWidth(value);
        display.setFont(FONT_SMALL);
        int unitWidth = (unit && unit[0]) ? display.getStrWidth(unit) : 0;
        int totalWidth = valueWidth + (unitWidth > 0 ? unitWidth + 1 : 0);
        int valueX = x + (width - totalWidth) / 2;

        display.setFont(valueFont);
        display.drawStr(valueX, valueY, value);
        if (unitWidth > 0)
        {
            display.setFont(FONT_SMALL);
            display.drawStr(valueX + valueWidth + 1, valueY, unit);
        }
    }

    static void metricRow(EOLO_DISPLAY_MODEL &display,
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
                          int valueY = Layout::FooterValueY,
                          const uint8_t *valueFont = FONT_LABEL)
    {
        if (label3)
        {
            metricCell(display, 0, Layout::ThirdColumnW, label1, value1, unit1, labelY, valueY, FONT_VALUE, valueFont);
            metricCell(display, Layout::ThirdColumnW, Layout::ThirdColumnW, label2, value2, unit2, labelY, valueY, FONT_VALUE, valueFont);
            metricCell(display, 2 * Layout::ThirdColumnW, Layout::ScreenW - 2 * Layout::ThirdColumnW, label3, value3, unit3, labelY, valueY, FONT_VALUE, valueFont);
        }
        else
        {
            metricCell(display, 0, Layout::HalfColumnW, label1, value1, unit1, labelY, valueY, FONT_VALUE, valueFont);
            metricCell(display, Layout::HalfColumnW, Layout::HalfColumnW, label2, value2, unit2, labelY, valueY, FONT_VALUE, valueFont);
        }

        // Divisores verticales finos entre columnas.
        const int top = labelY - 6;
        const int height = valueY + 2 - top;
        if (label3)
        {
            display.drawVLine(Layout::ThirdColumnW, top, height);
            display.drawVLine(2 * Layout::ThirdColumnW, top, height);
        }
        else
        {
            display.drawVLine(Layout::HalfColumnW, top, height);
        }
    }

    // Flechas laterales animadas que indican que el valor se ajusta con el encoder.
    static void adjustChevrons(EOLO_DISPLAY_MODEL &display, int leftX, int rightX, int midY, int animationStep,
                               int spacing = 8, int tip = 3)
    {
        const int cycle = (animationStep / 4) % 3;
        const int half = 4;
        display.drawTriangle(leftX - spacing - cycle, midY - half, leftX - spacing - cycle, midY + half,
                             leftX - tip - cycle, midY);
        display.drawTriangle(rightX + spacing + cycle, midY - half, rightX + spacing + cycle, midY + half,
                             rightX + tip + cycle, midY);
    }

    // Titulo pequeno en negrita y valor grande con flechas.
    static void valueSelector(EOLO_DISPLAY_MODEL &display,
                              const char *title,
                              const char *value,
                              int animationStep)
    {
        centeredText(display, title, Layout::CenterX, Layout::SelectorTitleY, FONT_LABEL);
        display.setFont(FONT_DISPLAY);
        int valueWidth = display.getStrWidth(value);
        int valueX = (Layout::ScreenW - valueWidth) / 2;
        display.drawStr(valueX, Layout::SelectorValueY, value);
        adjustChevrons(display, valueX, valueX + valueWidth, Layout::SelectorValueY - 5, animationStep);
    }

    // Selector de hora: titulo, HH:MM grande con flechas, dia relativo y,
    // si durationMinutes >= 0, la duracion de la captura.
    static void timeSelector(EOLO_DISPLAY_MODEL &display,
                             const char *title,
                             int hour,
                             int minute,
                             int dayOffset,
                             long durationMinutes,
                             int animationStep)
    {
        centeredText(display, title, Layout::CenterX, Layout::HeroLabelY, FONT_LABEL);

        const int timeY = 43;
        char timeText[8];
        snprintf(timeText, sizeof(timeText), "%02d:%02d", hour, minute);
        display.setFont(FONT_DISPLAY);
        int timeWidth = display.getStrWidth(timeText);
        int timeX = (Layout::ScreenW - timeWidth) / 2;
        display.drawStr(timeX, timeY, timeText);
        adjustChevrons(display, timeX, timeX + timeWidth, timeY - 5, animationStep, 14, 7);

        char dayText[16];
        if (dayOffset == 0)
            snprintf(dayText, sizeof(dayText), "hoy");
        else if (dayOffset == 1)
            snprintf(dayText, sizeof(dayText), "en 1 d\xED" "a");
        else
            snprintf(dayText, sizeof(dayText), "en %i d\xED" "as", dayOffset);
        centeredText(display, dayText, Layout::CenterX, 54, FONT_VALUE);

        if (durationMinutes >= 0)
        {
            char durationText[24];
            snprintf(durationText, sizeof(durationText), "Duraci\xF3n: %ldh %ldm",
                     durationMinutes / 60, durationMinutes % 60);
            centeredText(display, durationText, Layout::CenterX, 62, FONT_SMALL);
        }
    }

    // Reduce una region de un bitmap 1bpp (XBM, ancho srcStride px) a destW x destH.
    // El OR de los cuatro vecinos evita perder trazos finos al reducir.
    static void scaledBitmap(EOLO_DISPLAY_MODEL &display, const unsigned char *bitmap, int srcStride,
                             int srcX, int srcY, int srcW, int srcH,
                             int destX, int destY, int destW, int destH)
    {
        const int bytesPerRow = (srcStride + 7) / 8;
        auto pixel = [&](int x, int y)
        {
            uint8_t value = pgm_read_byte(bitmap + y * bytesPerRow + x / 8);
            return (value & (1U << (x & 7))) != 0;
        };
        for (int dy = 0; dy < destH; ++dy)
        {
            int sy = srcY + (dy * (srcH - 1)) / (destH - 1);
            int sy1 = min(sy + 1, srcY + srcH - 1);
            for (int dx = 0; dx < destW; ++dx)
            {
                int sx = srcX + (dx * (srcW - 1)) / (destW - 1);
                int sx1 = min(sx + 1, srcX + srcW - 1);
                if (pixel(sx, sy) || pixel(sx1, sy) || pixel(sx, sy1) || pixel(sx1, sy1))
                    display.drawPixel(destX + dx, destY + dy);
            }
        }
    }

    // Espiral de 5 puntos alrededor de (cx, cy); head 0..7 avanza la cabeza.
    static void spinner(EOLO_DISPLAY_MODEL &display, int cx, int cy, uint8_t head)
    {
        static const int8_t points[8][2] = {
            {0, -5}, {4, -4}, {6, 0}, {4, 4},
            {0, 5}, {-4, 4}, {-6, 0}, {-4, -4}
        };
        for (uint8_t i = 0; i < 5; ++i)
        {
            uint8_t index = (uint8_t)((head + 8U - i) & 7U);
            int x = cx + points[index][0];
            int y = cy + points[index][1];
            if (i < 2)
                display.drawBox(x - 1, y - 1, 3, 3);
            else
                display.drawPixel(x, y);
        }
    }

    // Barra cuadrada: marco de 1 px, 1 px de aire y relleno solido.
    static void progressBar(EOLO_DISPLAY_MODEL &display, int x, int y, int width, int height, int pct)
    {
        pct = constrain(pct, 0, 100);
        display.drawFrame(x, y, width, height);
        int fillW = (pct * (width - 4)) / 100;
        if (fillW > 0)
            display.drawBox(x + 2, y + 2, fillW, height - 4);
    }

    static void separatorLine(EOLO_DISPLAY_MODEL &display, int y)
    {
        display.drawHLine(0, y, Layout::ScreenW);
    }

    // Indicador de pagina: un punto por pagina y uno alargado para la activa.
    static void pageDots(EOLO_DISPLAY_MODEL &display, int count, int index, int y)
    {
        if (count <= 1)
            return;
        const int dot = 2;
        const int gap = 3;
        const int activeW = 6;
        int total = (count - 1) * (dot + gap) + activeW;
        int x = (Layout::ScreenW - total) / 2;
        for (int i = 0; i < count; i++)
        {
            int w = (i == index) ? activeW : dot;
            display.drawBox(x, y, w, dot);
            x += w + gap;
        }
    }

    // Lista de menu: ventana de 3 filas, texto centrado, fila activa invertida.
    static void menuList(EOLO_DISPLAY_MODEL &display, const char *const *labels, int count, int selectIndex)
    {
        if (count <= 0)
            return;
        selectIndex = constrain(selectIndex, 0, count - 1);

        int offset = 0;
        if (count > Layout::MenuRows)
        {
            offset = constrain(selectIndex - 1, 0, count - Layout::MenuRows);
        }

        const bool scrolls = count > Layout::MenuRows;
        const int boxW = scrolls ? Layout::MenuBoxW : Layout::ScreenW - 2 * Layout::MenuBoxX;
        int visible = min(count - offset, Layout::MenuRows);
        for (int e = 0; e < visible; e++)
        {
            int i = offset + e;
            int baseline = Layout::MenuFirstY + e * Layout::MenuPitch;
            bool highlight = (selectIndex == i);
            if (highlight)
            {
                display.drawBox(Layout::MenuBoxX, baseline - 12, boxW, Layout::MenuPitch);
                display.setDrawColor(0);
                display.setFont(FONT_MENU_SEL);
            }
            else
            {
                display.setDrawColor(1);
                display.setFont(FONT_MENU);
            }
            int width = display.getStrWidth(labels[i]);
            display.drawStr(Layout::MenuBoxX + (boxW - width) / 2, baseline, labels[i]);
            display.setDrawColor(1);
        }

        if (!scrolls)
            return;

        // Flechas de "hay mas" arriba/abajo (con vaiven) y barra de scroll punteada
        // entre ellas, todo en el margen derecho.
        const int arrowX = Layout::ScreenW - 5;
        const int bob = ((millis() / 200) % 2);
        if (offset > 0)
            display.drawTriangle(arrowX - 2, 18 + bob, arrowX + 2, 18 + bob, arrowX, 14 + bob);
        if (offset + Layout::MenuRows < count)
            display.drawTriangle(arrowX - 2, 59 - bob, arrowX + 2, 59 - bob, arrowX, 63 - bob);

        const int trackX = arrowX;
        const int trackTop = 22;
        const int trackHeight = 36;
        for (int y = trackTop; y < trackTop + trackHeight; y += 2)
            display.drawPixel(trackX, y);
        int knobHeight = max(trackHeight / count, 5);
        int knobY = trackTop;
        if (count > 1)
        {
            knobY = trackTop + ((trackHeight - knobHeight) * selectIndex) / (count - 1);
        }
        display.drawBox(trackX - 1, knobY, 3, knobHeight);
    }

    // Variante para arrays de structs con miembro `label` (p.ej. MenuOptionAction).
    template <typename T>
    static void menuListOf(EOLO_DISPLAY_MODEL &display, const T *items, int count, int selectIndex)
    {
        const char *labels[16];
        if (count > 16)
            count = 16;
        for (int i = 0; i < count; i++)
            labels[i] = items[i].label;
        menuList(display, labels, count, selectIndex);
    }

    struct KeyValue
    {
        const char *label;
        const char *value;
    };

    // Filas clave/valor: etiqueta a la derecha hasta x=63, valor en negrita desde x=66.
    // firstBaselineIndex: 0..Layout::KvMaxRows-1 (baseline = KvFirstY + idx * KvPitch).
    static void keyValueRows(EOLO_DISPLAY_MODEL &display, const KeyValue *rows, int count, int firstBaselineIndex = 0)
    {
        for (int i = 0; i < count && firstBaselineIndex + i < Layout::KvMaxRows; i++)
        {
            int y = Layout::KvFirstY + (firstBaselineIndex + i) * Layout::KvPitch;
            display.setFont(FONT_VALUE);
            display.drawStr(Layout::KvLabelRightX - display.getStrWidth(rows[i].label), y, rows[i].label);
            display.setFont(FONT_LABEL);
            display.drawStr(Layout::KvValueX, y, rows[i].value);
        }
    }

    // Eleccion Si/No (o dos etiquetas): titulo, dos cajas 48x15 (la activa invertida) y pista.
    static void yesNoChoice(EOLO_DISPLAY_MODEL &display, const char *title, bool yesSelected, const char *hint,
                            const char *yesLabel = "S\xED", const char *noLabel = "No")
    {
        centeredText(display, title, Layout::CenterX, 28, FONT_MENU_SEL);

        const int boxW = 48;
        const int boxH = 15;
        const int gap = 8;
        const int startX = (Layout::ScreenW - (boxW * 2 + gap)) / 2;
        const int boxY = 35;
        const int textY = 47;

        for (int side = 0; side < 2; side++)
        {
            bool isYes = (side == 0);
            bool selected = (isYes == yesSelected);
            int x = startX + side * (boxW + gap);
            const char *text = isYes ? yesLabel : noLabel;
            display.setDrawColor(1);
            if (selected)
            {
                display.setFont(FONT_LABEL);
                display.drawBox(x, boxY, boxW, boxH);
                display.setDrawColor(0);
            }
            else
            {
                display.setFont(FONT_VALUE);
                display.drawFrame(x, boxY, boxW, boxH);
            }
            display.drawStr(x + (boxW - display.getStrWidth(text)) / 2, textY, text);
            display.setDrawColor(1);
        }

        if (hint && hint[0])
        {
            centeredText(display, hint, Layout::CenterX, Layout::FooterValueY, FONT_VALUE);
        }
    }

    // Dos valores grandes lado a lado con etiqueta pequena encima (Inicio / Restante).
    static void heroPair(EOLO_DISPLAY_MODEL &display,
                         const char *label1, const char *value1,
                         const char *label2, const char *value2)
    {
        const int space = 10;
        const int labelY = 47;
        const int valueY = Layout::FooterValueY;
        display.setFont(FONT_HERO);
        int w1 = display.getStrWidth(value1);
        int w2 = display.getStrWidth(value2);
        int x1 = (Layout::ScreenW - (w1 + w2 + space)) / 2;
        int x2 = x1 + w1 + space;

        display.setFont(FONT_VALUE);
        display.drawStr(x1 + (w1 - display.getStrWidth(label1)) / 2, labelY, label1);
        display.drawStr(x2 + (w2 - display.getStrWidth(label2)) / 2, labelY, label2);
        display.setFont(FONT_HERO);
        display.drawStr(x1, valueY, value1);
        display.drawStr(x2, valueY, value2);
    }
};

#endif
