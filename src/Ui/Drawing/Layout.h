#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

namespace Layout
{
    constexpr int ScreenW = 128;
    constexpr int ScreenH = 64;

    constexpr int HeaderTop = 0;
    constexpr int HeaderBottom = 13;
    constexpr int BodyTop = 14;
    constexpr int BodyBottom = 63;

    constexpr int CenterX = ScreenW / 2;
    constexpr int MainPanelTop = 15;
    constexpr int MainPanelBottom = 40;
    constexpr int FooterTop = 40;
    constexpr int FooterLabelY = 50;
    constexpr int FooterValueY = 60;

    constexpr int HalfColumnW = ScreenW / 2;
    constexpr int ThirdColumnW = ScreenW / 3;

    // Cuadricula del cuerpo (y=14..63). Todas las baselines estan aqui.
    constexpr int MarginX = 4;
    constexpr int FontDescent = 2;  // descendentes de las fuentes helv

    // Layout de metricas: heroe + separador + pie.
    constexpr int HeroLabelY = 24;
    constexpr int HeroValueY = 38;
    constexpr int SeparatorY = 40;

    // Selector de valor.
    constexpr int SelectorTitleY = 26;
    constexpr int SelectorValueY = 50;

    // Lista clave/valor: paso 9, hasta 5 baselines.
    constexpr int KvPitch = 9;
    constexpr int KvFirstY = 24;
    constexpr int KvMaxRows = 5;
    constexpr int KvLabelRightX = 63;  // etiqueta alineada a la derecha
    constexpr int KvValueX = 66;       // valor alineado a la izquierda
    constexpr int KvLastY = KvFirstY + (KvMaxRows - 1) * KvPitch;

    // Menu: ventana de 3 filas, paso 14.
    constexpr int MenuPitch = 14;
    constexpr int MenuRows = 3;
    constexpr int MenuFirstY = 30;
    constexpr int MenuBoxX = 2;
    constexpr int MenuBoxW = 118;  // 2..119; 122..127 queda para la barra de scroll
    constexpr int MenuLastY = MenuFirstY + (MenuRows - 1) * MenuPitch;

    static_assert(HeroLabelY + FontDescent <= BodyBottom, "Hero label baseline outside body");
    static_assert(HeroValueY + FontDescent <= BodyBottom, "Hero value baseline outside body");
    static_assert(SeparatorY <= BodyBottom, "Separator outside body");
    static_assert(FooterLabelY + FontDescent <= BodyBottom, "Footer label baseline outside body");
    static_assert(FooterValueY + FontDescent <= BodyBottom, "Footer value baseline outside body");
    static_assert(SelectorTitleY + FontDescent <= BodyBottom, "Selector title baseline outside body");
    static_assert(SelectorValueY + FontDescent <= BodyBottom, "Selector value baseline outside body");
    static_assert(KvFirstY > BodyTop && KvLastY + FontDescent <= BodyBottom, "Key/value rows outside body");
    static_assert(MenuLastY + FontDescent <= BodyBottom, "Menu rows outside body");
    static_assert(MenuBoxX >= 0 && MenuBoxX + MenuBoxW <= ScreenW, "Menu highlight outside screen");

    // Header: contenido y=0..12 y separador en y=13. Los slots evitan que
    // los widgets dependan de cursores o de la presencia de otros indicadores.
    namespace Header
    {
        struct Slot { int x; int y; int w; int h; };

        // Bateria = icono vertical 5x9 + porcentaje en FONT_VALUE, con 1px de
        // margen a cada lado; el subrayado de bateria activa cubre el slot.
        constexpr Slot Time = {2, 1, 29, 11};      // x=2..30
        constexpr Slot Sd = {32, 2, 10, 9};        // x=32..41
#ifdef FEATURE_DUAL_BATTERY
        // Sin marca: el espacio va a los dos porcentajes ("100" sin signo %).
        constexpr Slot Modem = {44, 2, 16, 9};     // x=44..59
        constexpr Slot Dc = {62, 2, 8, 9};         // x=62..69
        constexpr Slot Battery0 = {72, 1, 26, 11}; // x=72..97
        constexpr Slot Battery1 = {102, 1, 26, 11};// x=102..127
#else
        constexpr Slot Brand = {51, 1, 26, 11};    // x=51..76, centrada en CenterX
        constexpr Slot Modem = {77, 2, 16, 9};     // x=77..92
        constexpr Slot Battery = {94, 1, 34, 11};  // x=94..127 ("100%")
#endif
        constexpr int ActiveSourceY = 12;

        constexpr bool fits(const Slot &slot)
        {
            return slot.x >= 0 && slot.y >= HeaderTop &&
                   slot.x + slot.w <= ScreenW &&
                   slot.y + slot.h <= ActiveSourceY - HeaderTop + 1;
        }

        constexpr bool intersects(const Slot &a, const Slot &b)
        {
            return a.x < b.x + b.w && b.x < a.x + a.w &&
                   a.y < b.y + b.h && b.y < a.y + a.h;
        }

#ifdef FEATURE_DUAL_BATTERY
        constexpr Slot Slots[] = {Time, Sd, Modem, Dc, Battery0, Battery1};
#else
        constexpr Slot Slots[] = {Time, Sd, Brand, Modem, Battery};
        static_assert(Brand.x + Brand.w / 2 == CenterX, "Brand must be centered");
#endif
        constexpr int SlotCount = sizeof(Slots) / sizeof(Slots[0]);

        constexpr bool allFit()
        {
            for (int i = 0; i < SlotCount; ++i)
                if (!fits(Slots[i])) return false;
            return true;
        }

        constexpr bool anyOverlap()
        {
            for (int i = 0; i < SlotCount; ++i)
                for (int j = i + 1; j < SlotCount; ++j)
                    if (intersects(Slots[i], Slots[j])) return true;
            return false;
        }

        static_assert(allFit(), "Header slot outside the 128x64 header content area");
        static_assert(!anyOverlap(), "Header slots must not overlap");
    }
}

#endif
