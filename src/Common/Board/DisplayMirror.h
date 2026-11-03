#ifndef EOLO_BOARD_DISPLAY_MIRROR_H
#define EOLO_BOARD_DISPLAY_MIRROR_H

#include <Arduino.h>
#include <Eolo/Core/Ui/DisplayMirrorFrame.h>
#ifndef FEATURE_HEADLESS
#include <U8g2lib.h>
#include "../../Variants/Legacy.h"
#endif

// Espejo de la pantalla por serie para depurar sin OLED (ver
// scripts/eolo_mirror.py). Lo activa el visor con 'D' y debe repetirlo: sin un
// 'D' en los ultimos kKeepAliveMs se apaga solo, asi un visor cerrado a la
// fuerza no deja al equipo transmitiendo. 'd' lo apaga de inmediato.
// Todo ocurre en el task del loop (la consola y la UI comparten task).
//
// Manda solo las paginas que cambiaron (trama delta) y un cuadro completo cada
// kKeyframeMs, que tambien sirve de latido. Nunca bloquea: si el buffer TX del
// UART no tiene lugar para la trama, la salta y el siguiente cuadro la recupera.
class DisplayMirror
{
public:
    static constexpr uint32_t kKeepAliveMs = 5000;
    static constexpr uint32_t kMinFrameIntervalMs = 33; // ~30 fps
    static constexpr uint32_t kKeyframeMs = 1000;
    static constexpr size_t kTxBufferBytes = 2048; // para Serial.setTxBufferSize antes de begin

    static void keepAlive()
    {
        if (!active())
        {
            sentOnce_ = false; // el visor recien conectado recibe el cuadro ya
        }
        armed_ = true;
        lastKeepAliveMs_ = millis();
    }

    static void stop() { armed_ = false; }

    static bool active() { return armed_ && millis() - lastKeepAliveMs_ < kKeepAliveMs; }

    static void emit(const uint8_t *buffer, uint8_t tilesW, uint8_t tilesH)
    {
        const size_t rawSize = static_cast<size_t>(tilesW) * tilesH * 8;
        if (!active() || rawSize == 0 || rawSize > kRawCapacity)
            return;

        const uint32_t now = millis();
        if (sentOnce_ && now - lastSentMs_ < kMinFrameIntervalMs)
            return;

        const bool keyframe = !sentOnce_ || tilesW != prevTilesW_ || tilesH != prevTilesH_ ||
                              now - lastKeyframeMs_ >= kKeyframeMs;
        const size_t size = keyframe ? DisplayMirrorFrame::encode(buffer, tilesW, tilesH, frame_)
                                     : DisplayMirrorFrame::encodeDelta(buffer, prev_, tilesW, tilesH, frame_);
        if (size == 0 || static_cast<size_t>(Serial.availableForWrite()) < size)
            return;

        Serial.write(frame_, size); // una sola llamada: no se intercala con los logs
        memcpy(prev_, buffer, rawSize);
        prevTilesW_ = tilesW;
        prevTilesH_ = tilesH;
        sentOnce_ = true;
        lastSentMs_ = now;
        if (keyframe)
            lastKeyframeMs_ = now;
    }

private:
    static constexpr size_t kRawCapacity = 1024; // 128x64
    inline static uint8_t frame_[DisplayMirrorFrame::maxFrameSize(kRawCapacity)];
    inline static uint8_t prev_[kRawCapacity];
    inline static uint8_t prevTilesW_ = 0;
    inline static uint8_t prevTilesH_ = 0;
    inline static bool armed_ = false;
    inline static bool sentOnce_ = false;
    inline static uint32_t lastKeepAliveMs_ = 0;
    inline static uint32_t lastSentMs_ = 0;
    inline static uint32_t lastKeyframeMs_ = 0;
};

#ifndef FEATURE_HEADLESS
// Pantalla de las escenas: igual al driver U8g2 salvo que cada sendBuffer()
// tambien alimenta el espejo. Todo codigo de UI debe usar este tipo (no
// EOLO_DISPLAY_MODEL) para no saltarse el hook.
template <class Driver>
class MirroredDisplay : public Driver
{
public:
    using Driver::Driver;

    void sendBuffer()
    {
        Driver::sendBuffer();
        DisplayMirror::emit(this->getBufferPtr(), this->getBufferTileWidth(),
                            this->getBufferTileHeight());
    }
};

using EoloDisplay = MirroredDisplay<EOLO_DISPLAY_MODEL>;
#endif

#endif
