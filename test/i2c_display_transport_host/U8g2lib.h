#pragma once
#include <cstdint>

struct u8x8_t;
using u8x8_msg_cb = uint8_t (*)(u8x8_t *, uint8_t, uint8_t, void *);
struct u8x8_t { u8x8_msg_cb byte_cb = nullptr; };
inline uint8_t u8x8_GetI2CAddress(u8x8_t *) { return 0x78; }
enum {
    U8X8_MSG_BYTE_INIT = 1,
    U8X8_MSG_BYTE_SET_DC,
    U8X8_MSG_BYTE_START_TRANSFER,
    U8X8_MSG_BYTE_SEND,
    U8X8_MSG_BYTE_END_TRANSFER
};
