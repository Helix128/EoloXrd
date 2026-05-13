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
}

#endif
