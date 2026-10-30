#ifndef EOLO_BOARD_SOFT_SPI_DISPLAY_TRANSPORT_H
#define EOLO_BOARD_SOFT_SPI_DISPLAY_TRANSPORT_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "esp_rom_gpio.h"
#include "soc/gpio_struct.h"

// Transporte de la OLED por SPI software cuando SCK/MOSI son los mismos
// pines que la SD usa por VSPI (Express). SD.begin() enruta esos pines al
// periférico en la matriz GPIO y, desde ahí, el bit-bang de U8g2 ya no llega
// al panel: la pantalla queda congelada en el último frame previo a la SD.
// Cada transferencia toma los pines como GPIO y al terminar devuelve el
// enrutamiento que encontró, sea VSPI o GPIO. Quien dibuja ya tiene el
// SPIBus::Guard, así que la SD no puede estar transfiriendo en medio.
namespace SoftSpiDisplayTransport
{
inline uint32_t savedClockSignal = SIG_GPIO_OUT_IDX;
inline uint32_t savedDataSignal = SIG_GPIO_OUT_IDX;

inline uint32_t outputSignal(uint8_t pin)
{
    return GPIO.func_out_sel_cfg[pin].func_sel;
}

inline uint8_t byteCallback(u8x8_t *u8x8, uint8_t msg, uint8_t arg, void *ptr)
{
    const uint8_t clock = u8x8->pins[U8X8_PIN_SPI_CLOCK];
    const uint8_t data = u8x8->pins[U8X8_PIN_SPI_DATA];
    switch (msg)
    {
    case U8X8_MSG_BYTE_START_TRANSFER:
        savedClockSignal = outputSignal(clock);
        savedDataSignal = outputSignal(data);
        esp_rom_gpio_connect_out_signal(clock, SIG_GPIO_OUT_IDX, false, false);
        esp_rom_gpio_connect_out_signal(data, SIG_GPIO_OUT_IDX, false, false);
        return u8x8_byte_arduino_4wire_sw_spi(u8x8, msg, arg, ptr);
    case U8X8_MSG_BYTE_END_TRANSFER:
    {
        const uint8_t result = u8x8_byte_arduino_4wire_sw_spi(u8x8, msg, arg, ptr);
        esp_rom_gpio_connect_out_signal(clock, savedClockSignal, false, false);
        esp_rom_gpio_connect_out_signal(data, savedDataSignal, false, false);
        return result;
    }
    default:
        return u8x8_byte_arduino_4wire_sw_spi(u8x8, msg, arg, ptr);
    }
}

inline void install(u8x8_t *u8x8)
{
    u8x8->byte_cb = byteCallback;
}
}

#endif
