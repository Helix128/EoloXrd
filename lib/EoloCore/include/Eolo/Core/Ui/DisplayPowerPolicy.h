#ifndef EOLO_CORE_UI_DISPLAY_POWER_POLICY_H
#define EOLO_CORE_UI_DISPLAY_POWER_POLICY_H

#include <stdint.h>
#include <Eolo/Services/SettingsStore.h>

// Politica de energia de la pantalla OLED: tras inactividad baja el contraste
// con suavidad, luego lo lleva a cero y apaga el panel (power save). Cualquier
// actividad devuelve el brillo configurado de inmediato. Sin dependencias de
// hardware para poder probarla en nativo.
struct DisplayPowerConfig
{
    static constexpr uint8_t MinBrightnessPct = 10;
    static constexpr uint8_t MaxBrightnessPct = 100;
    static constexpr uint8_t DimLevelPct = 25; // del brillo configurado
    static constexpr uint32_t DimFadeMs = 2000;
    static constexpr uint32_t OffFadeMs = 2000;
    static constexpr uint16_t MinDimAfterS = 5;
    static constexpr uint16_t MaxOffAfterS = 600;

    uint8_t brightnessPct = MaxBrightnessPct;
    uint16_t dimAfterS = 15;
    uint16_t offAfterS = 30;

    // Mantiene los valores dentro de rango y el apagado despues de que
    // termine el fade de atenuacion (si chocan, cede el apagado).
    void sanitize()
    {
        constexpr uint16_t fadeS = (DimFadeMs + 999) / 1000;
        if (brightnessPct < MinBrightnessPct)
            brightnessPct = MinBrightnessPct;
        if (brightnessPct > MaxBrightnessPct)
            brightnessPct = MaxBrightnessPct;
        if (dimAfterS < MinDimAfterS)
            dimAfterS = MinDimAfterS;
        if (dimAfterS > MaxOffAfterS - fadeS)
            dimAfterS = MaxOffAfterS - fadeS;
        if (offAfterS < dimAfterS + fadeS)
            offAfterS = dimAfterS + fadeS;
        if (offAfterS > MaxOffAfterS)
            offAfterS = MaxOffAfterS;
    }

    uint8_t baseContrast() const { return (uint8_t)((uint32_t)brightnessPct * 255U / 100U); }

    static constexpr const char *KeyBrightness = "bright";
    static constexpr const char *KeyDim = "dimS";
    static constexpr const char *KeyOff = "offS";

    static DisplayPowerConfig load(SettingsStore &store)
    {
        DisplayPowerConfig cfg;
        cfg.brightnessPct = (uint8_t)store.getUInt(KeyBrightness, cfg.brightnessPct);
        cfg.dimAfterS = (uint16_t)store.getUInt(KeyDim, cfg.dimAfterS);
        cfg.offAfterS = (uint16_t)store.getUInt(KeyOff, cfg.offAfterS);
        cfg.sanitize();
        return cfg;
    }

    void save(SettingsStore &store) const
    {
        store.putUInt(KeyBrightness, brightnessPct);
        store.putUInt(KeyDim, dimAfterS);
        store.putUInt(KeyOff, offAfterS);
    }
};

struct DisplayPowerOutput
{
    uint8_t contrast;
    bool powerOn;
};

namespace DisplayPowerPolicy
{
inline uint8_t lerp(uint8_t from, uint8_t to, uint32_t elapsed, uint32_t span)
{
    if (elapsed >= span)
        return to;
    const int32_t delta = (int32_t)to - (int32_t)from;
    return (uint8_t)((int32_t)from + delta * (int32_t)elapsed / (int32_t)span);
}

// idleMs: milisegundos desde la ultima actividad.
inline DisplayPowerOutput evaluate(const DisplayPowerConfig &cfg, uint32_t idleMs)
{
    const uint8_t base = cfg.baseContrast();
    const uint8_t dim = (uint8_t)((uint32_t)base * DisplayPowerConfig::DimLevelPct / 100U);
    const uint32_t dimAt = (uint32_t)cfg.dimAfterS * 1000UL;
    const uint32_t offAt = (uint32_t)cfg.offAfterS * 1000UL;

    if (idleMs < dimAt)
        return {base, true};
    if (idleMs < dimAt + DisplayPowerConfig::DimFadeMs)
        return {lerp(base, dim, idleMs - dimAt, DisplayPowerConfig::DimFadeMs), true};
    if (idleMs < offAt)
        return {dim, true};
    if (idleMs < offAt + DisplayPowerConfig::OffFadeMs)
        return {lerp(dim, 0, idleMs - offAt, DisplayPowerConfig::OffFadeMs), true};
    return {0, false};
}
} // namespace DisplayPowerPolicy

#endif
