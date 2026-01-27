#ifndef CONFIG_H
#define CONFIG_H

// CONFIGURACIONES GENERALES DEL PROGRAMA
#define LOG_F(fmt, ...) Serial.printf("[%s] " fmt, __func__, ##__VA_ARGS__)

#define LOG_LN(msg) do { \
    Serial.print("["); \
    Serial.print(__func__); \
    Serial.print("] "); \
    Serial.println(msg); \
} while(0)

#define LOG_P(msg) do { \
    Serial.print("["); \
    Serial.print(__func__); \
    Serial.print("] "); \
    Serial.print(msg); \
} while(0)

// Habilitar pruebas de componentes al inicio (desactivar para booteo rápido)
#define CHECK_SENSORS false

// Modo barebones (para debug sin tener el EOLO completo)
// simula sensores
#define BAREBONES false

// simula encoder
#define SERIAL_INPUT false

#define SD_CS_PIN 5
#define SD_MOSI_PIN 23
#define SD_MISO_PIN 19
#define SD_SCK_PIN 18

// Modelo de pantalla a usar en U8G2 (default U8G2_SSD1309_128x64_NONAME2_F_HW_I2C)
#define DisplayModel U8G2_SSD1309_128X64_NONAME2_F_HW_I2C
//#define DisplayModel U8G2_SSD1306_128X64_NONAME_F_HW_I2C

// Pines I2C
#define SDA_PIN 21
#define SCL_PIN 22

#define I2C_CLOCK 150000 

// Dirección I2C del driver de input (ATTINY85)
#define ATTINY_ADDRESS 8

// Comandos para el ATTINY85
#define CMD_RESET_COUNTER 0x01
#define CMD_RESET_BUTTON 0x02

#define MINUTE 60UL
#define HOUR 3600UL
#define DAY 86400UL

#define FONT_BOLD u8g2_font_helvB10_tf
#define FONT_REGULAR u8g2_font_helvR10_tf

#define FONT_BOLD_S u8g2_font_helvB08_tf
#define FONT_REGULAR_S u8g2_font_helvR08_tf

#define FONT_BOLD_L u8g2_font_helvB12_tf
#define FONT_REGULAR_L u8g2_font_helvR12_tf

// extras EOLO grande
#define PPH_PWR_PIN 4 // perifericos
#define MODEM_PWR_PIN 13 // modem

#endif // CONFIG_H

