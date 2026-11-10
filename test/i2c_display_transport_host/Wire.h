#pragma once
#include <cstddef>
#include <cstdint>

struct MockWire {
    unsigned starts = 0;
    unsigned ends = 0;
    bool shortWrite = false;
    uint8_t error = 0;
    void beginTransmission(uint8_t) { ++starts; }
    size_t write(const uint8_t *, size_t count) { return shortWrite ? count - 1 : count; }
    uint8_t endTransmission() { ++ends; return error; }
};
inline MockWire Wire;
