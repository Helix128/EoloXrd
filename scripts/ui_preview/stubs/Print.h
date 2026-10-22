#ifndef PREVIEW_PRINT_H
#define PREVIEW_PRINT_H
#include <stddef.h>
#include <stdint.h>
class Print
{
public:
    virtual ~Print() {}
    virtual size_t write(uint8_t) = 0;
    virtual size_t write(const uint8_t *buf, size_t n) { size_t c = 0; while (n--) c += write(*buf++); return c; }
    size_t print(const char *s) { size_t c = 0; while (s && *s) c += write((uint8_t)*s++); return c; }
    size_t print(char ch) { return write((uint8_t)ch); }
    size_t print(int v) { char b[16]; snprintf(b, sizeof(b), "%d", v); return print(b); }
    size_t println(const char *s) { size_t c = print(s); c += write('\n'); return c; }
};
#endif
