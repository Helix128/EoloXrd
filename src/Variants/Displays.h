#ifndef EOLO_CONFIG_DISPLAYS_H
#define EOLO_CONFIG_DISPLAYS_H

#include <stdint.h>

// Los nombres son macros numéricas para que la selección también funcione en
// expresiones del preprocesador (#if) antes de declarar el tipo C++ asociado.
#define SSD1306_I2C 1
#define SSD1309_I2C 2
#define SH1106_I2C 3
#define SSD1309_SW_SPI 4
#define SSD1306_HW_SPI 5

#ifndef EOLO_DISPLAY
  #define EOLO_DISPLAY EOLO_DEFAULT_DISPLAY
#endif

#if EOLO_DISPLAY == SSD1306_I2C
  #ifndef EOLO_DISPLAY_MODEL
    #define EOLO_DISPLAY_MODEL U8G2_SSD1306_128X64_NONAME_F_HW_I2C
  #endif
  #define EOLO_DISPLAY_SPI 0
  #define EOLO_DISPLAY_HW_SPI 0
  #define EOLO_DISPLAY_SPI_CLOCK_HZ 0UL
#elif EOLO_DISPLAY == SSD1309_I2C
  #ifndef EOLO_DISPLAY_MODEL
    #define EOLO_DISPLAY_MODEL U8G2_SSD1309_128X64_NONAME0_F_HW_I2C
  #endif
  #define EOLO_DISPLAY_SPI 0
  #define EOLO_DISPLAY_HW_SPI 0
  #define EOLO_DISPLAY_SPI_CLOCK_HZ 0UL
#elif EOLO_DISPLAY == SH1106_I2C
  #ifndef EOLO_DISPLAY_MODEL
    #define EOLO_DISPLAY_MODEL U8G2_SH1106_128X64_NONAME_F_HW_I2C
  #endif
  #define EOLO_DISPLAY_SPI 0
  #define EOLO_DISPLAY_HW_SPI 0
  #define EOLO_DISPLAY_SPI_CLOCK_HZ 0UL
#elif EOLO_DISPLAY == SSD1309_SW_SPI
  #ifndef EOLO_DISPLAY_MODEL
    #define EOLO_DISPLAY_MODEL U8G2_SSD1309_128X64_NONAME2_F_4W_SW_SPI
  #endif
  #define EOLO_DISPLAY_SPI 1
  #define EOLO_DISPLAY_HW_SPI 0
  #define EOLO_DISPLAY_SPI_CLOCK_HZ 8000000UL
#elif EOLO_DISPLAY == SSD1306_HW_SPI
  #ifndef EOLO_DISPLAY_MODEL
    #define EOLO_DISPLAY_MODEL U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI
  #endif
  #define EOLO_DISPLAY_SPI 1
  #define EOLO_DISPLAY_HW_SPI 1
  #define EOLO_DISPLAY_SPI_CLOCK_HZ 4000000UL
#else
  #error "EOLO_DISPLAY debe ser SSD1306_I2C, SSD1309_I2C, SH1106_I2C, SSD1309_SW_SPI o SSD1306_HW_SPI"
#endif

namespace EoloConfig
{
enum class DisplayKind : uint8_t
{
    SSD1306I2c = SSD1306_I2C,
    SSD1309I2c = SSD1309_I2C,
    SH1106I2c = SH1106_I2C,
    SSD1309SoftwareSpi = SSD1309_SW_SPI,
    SSD1306HardwareSpi = SSD1306_HW_SPI
};

struct DisplayDescriptor
{
    DisplayKind kind;
    bool spi;
    bool hardwareSpi;
    uint32_t spiClockHz;
};

inline constexpr DisplayDescriptor selectedDisplay{
    static_cast<DisplayKind>(EOLO_DISPLAY), EOLO_DISPLAY_SPI != 0,
    EOLO_DISPLAY_HW_SPI != 0, EOLO_DISPLAY_SPI_CLOCK_HZ};
}

#endif
