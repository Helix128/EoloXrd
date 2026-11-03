#ifndef EOLO_CORE_UI_DISPLAY_MIRROR_FRAME_H
#define EOLO_CORE_UI_DISPLAY_MIRROR_FRAME_H

#include <stddef.h>
#include <stdint.h>

// Trama del espejo de pantalla por serie. Sin dependencias de hardware.
//
//   magic(4) | tilesW(1) | tilesH(1) | len(2 LE) | payload(len) | crc16(2 LE)
//
// payload = framebuffer U8g2 (tilesW*tilesH*8 bytes, formato por paginas, bit 0 =
// fila superior). Si len == tamano crudo va sin comprimir; si es menor va en
// PackBits: control 0..127 = (c+1) bytes literales, 128..255 = el siguiente byte
// repetido (c-126) veces. crc16 (CCITT-FALSE) cubre desde tilesW hasta el payload.
// El primer byte del magic es 0xFF, que nunca aparece en el texto UTF-8 de los logs.
//
// Trama delta (ultimo byte del magic 0x0C): mismo encabezado y crc, payload =
// mascara de paginas (ceil(tilesH/8) bytes, bit i del byte i/8 = pagina i cambio)
// seguida de las paginas cambiadas en orden (tilesW*8 bytes c/u). Es relativa al
// ultimo cuadro enviado; el visor la ignora hasta tener uno completo del mismo
// tamano, y el firmware reenvia uno completo cada tanto para resincronizar.
namespace DisplayMirrorFrame
{
inline constexpr uint8_t Magic[4] = {0xFF, 0xE0, 0xF0, 0x0B};
inline constexpr uint8_t DeltaMagic[4] = {0xFF, 0xE0, 0xF0, 0x0C};
inline constexpr size_t HeaderSize = 8;
inline constexpr size_t CrcSize = 2;

// Peor caso de ambas tramas: RLE con un literal de 128 bytes agrega 1 de
// control; la delta agrega la mascara (<= 1 byte cada 64 de crudo + 1).
inline constexpr size_t maxFrameSize(size_t rawSize)
{
    return HeaderSize + rawSize + (rawSize + 127) / 128 + rawSize / 64 + 1 + CrcSize;
}

inline uint16_t crc16(const uint8_t *data, size_t size, uint16_t crc = 0xFFFF)
{
    for (size_t i = 0; i < size; ++i)
    {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int bit = 0; bit < 8; ++bit)
        {
            crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                                 : static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}

// Escribe el PackBits de `in` en `out`; devuelve los bytes usados, o 0 si no
// cabe en `capacity` (el llamador entonces manda el buffer crudo).
inline size_t packBits(const uint8_t *in, size_t size, uint8_t *out, size_t capacity)
{
    size_t o = 0;
    size_t i = 0;
    while (i < size)
    {
        size_t run = 1;
        while (i + run < size && run < 129 && in[i + run] == in[i])
        {
            ++run;
        }
        if (run >= 2)
        {
            if (o + 2 > capacity)
                return 0;
            out[o++] = static_cast<uint8_t>(run + 126);
            out[o++] = in[i];
            i += run;
            continue;
        }
        size_t lit = 1;
        while (i + lit < size && lit < 128 &&
               !(i + lit + 1 < size && in[i + lit] == in[i + lit + 1]))
        {
            ++lit;
        }
        if (o + 1 + lit > capacity)
            return 0;
        out[o++] = static_cast<uint8_t>(lit - 1);
        for (size_t k = 0; k < lit; ++k)
        {
            out[o++] = in[i + k];
        }
        i += lit;
    }
    return o;
}

inline size_t finishFrame(const uint8_t *magic, uint8_t tilesW, uint8_t tilesH, size_t len,
                          uint8_t *out)
{
    for (size_t i = 0; i < 4; ++i)
    {
        out[i] = magic[i];
    }
    out[4] = tilesW;
    out[5] = tilesH;
    out[6] = static_cast<uint8_t>(len & 0xFF);
    out[7] = static_cast<uint8_t>(len >> 8);
    const uint16_t crc = crc16(out + 4, 4 + len);
    out[HeaderSize + len] = static_cast<uint8_t>(crc & 0xFF);
    out[HeaderSize + len + 1] = static_cast<uint8_t>(crc >> 8);
    return HeaderSize + len + CrcSize;
}

// Arma la trama completa en `out` (capacidad >= maxFrameSize(rawSize)) y
// devuelve su tamano. rawSize debe caber en 16 bits.
inline size_t encode(const uint8_t *raw, uint8_t tilesW, uint8_t tilesH, uint8_t *out)
{
    const size_t rawSize = static_cast<size_t>(tilesW) * tilesH * 8;
    uint8_t *payload = out + HeaderSize;
    size_t len = packBits(raw, rawSize, payload, rawSize - 1);
    if (len == 0)
    {
        for (size_t i = 0; i < rawSize; ++i)
        {
            payload[i] = raw[i];
        }
        len = rawSize;
    }
    return finishFrame(Magic, tilesW, tilesH, len, out);
}

// Arma la trama delta de `raw` contra `prev` (mismo tamano). Devuelve 0 si no
// hay paginas distintas (nada que enviar).
inline size_t encodeDelta(const uint8_t *raw, const uint8_t *prev, uint8_t tilesW, uint8_t tilesH,
                          uint8_t *out)
{
    const size_t pageSize = static_cast<size_t>(tilesW) * 8;
    const size_t maskSize = (static_cast<size_t>(tilesH) + 7) / 8;
    uint8_t *mask = out + HeaderSize;
    uint8_t *pages = mask + maskSize;
    for (size_t i = 0; i < maskSize; ++i)
    {
        mask[i] = 0;
    }
    size_t changed = 0;
    for (size_t page = 0; page < tilesH; ++page)
    {
        const uint8_t *a = raw + page * pageSize;
        const uint8_t *b = prev + page * pageSize;
        size_t i = 0;
        while (i < pageSize && a[i] == b[i])
        {
            ++i;
        }
        if (i == pageSize)
            continue;
        mask[page / 8] |= static_cast<uint8_t>(1u << (page % 8));
        for (i = 0; i < pageSize; ++i)
        {
            pages[changed * pageSize + i] = a[i];
        }
        ++changed;
    }
    if (changed == 0)
        return 0;
    return finishFrame(DeltaMagic, tilesW, tilesH, maskSize + changed * pageSize, out);
}
} // namespace DisplayMirrorFrame

#endif
