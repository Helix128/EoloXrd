// Stub minimo de Arduino.h para renderizar la UI en el PC (ver ../README.md).
#ifndef PREVIEW_ARDUINO_H
#define PREVIEW_ARDUINO_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <string>
#include <atomic>

#define PROGMEM
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define HIGH 1
#define LOW 0

using std::max;
using std::min;

extern unsigned long previewMillis;
inline unsigned long millis() { return previewMillis; }
inline void delay(unsigned long) {}
inline long map(long x, long inMin, long inMax, long outMin, long outMax)
{
    return (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

class String
{
    std::string s;
public:
    String() {}
    String(const char *c) : s(c ? c : "") {}
    String(float v, int dec = 2) { char b[32]; snprintf(b, sizeof(b), "%.*f", dec, v); s = b; }
    String(int v) { s = std::to_string(v); }
    const char *c_str() const { return s.c_str(); }
    String operator+(const char *c) const { String r(*this); r.s += c; return r; }
    String operator+(const String &o) const { String r(*this); r.s += o.s; return r; }
};

#endif
