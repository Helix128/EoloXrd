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
    constexpr int KvFirstY = 22;
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

        constexpr Slot Time = {2, 2, 25, 9};       // x=2..26
        constexpr Slot Sd = {30, 2, 10, 9};        // x=30..39
        constexpr Slot Brand = {45, 2, 20, 9};     // x=45..64
        constexpr Slot Modem = {78, 2, 16, 9};     // x=78..93
        constexpr Slot Dc = {97, 2, 8, 9};         // x=97..104
        constexpr Slot Battery0 = {108, 2, 9, 9};  // x=108..116
        constexpr Slot Battery1 = {119, 2, 9, 9};  // x=119..127
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

        static_assert(fits(Time) && fits(Sd) && fits(Brand) && fits(Modem) &&
                          fits(Dc) && fits(Battery0) && fits(Battery1),
                      "Header slot outside the 128x64 header content area");
        static_assert(!intersects(Time, Sd) && !intersects(Time, Brand) &&
                          !intersects(Time, Modem) && !intersects(Time, Dc) &&
                          !intersects(Time, Battery0) && !intersects(Time, Battery1) &&
                          !intersects(Sd, Brand) && !intersects(Sd, Modem) &&
                          !intersects(Sd, Dc) && !intersects(Sd, Battery0) &&
                          !intersects(Sd, Battery1) && !intersects(Brand, Modem) &&
                          !intersects(Brand, Dc) && !intersects(Brand, Battery0) &&
                          !intersects(Brand, Battery1) && !intersects(Modem, Dc) &&
                          !intersects(Modem, Battery0) && !intersects(Modem, Battery1) &&
                          !intersects(Dc, Battery0) && !intersects(Dc, Battery1) &&
                          !intersects(Battery0, Battery1),
                      "Header slots must not overlap");
    }
}

#endif
