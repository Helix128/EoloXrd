#ifndef CONFIG_H
#define CONFIG_H

// CONFIGURACIONES GENERALES DEL PROGRAMA

// Habilitar pruebas de componentes al inicio (desactivar para booteo rápido)
#define CHECK_SENSORS false

// Modo barebones (para debug sin tener el EOLO completo)
// simula sensores
#define BAREBONES true

// simula encoder
#define SERIAL_INPUT true

#define SD_CS_PIN 5
#define SD_MOSI_PIN 23
#define SD_MISO_PIN 19
#define SD_SCK_PIN 18

// Modelo de pantalla a usar en U8G2 (default U8G2_SSD1309_128x64_NONAME2_F_HW_I2C)
//#define DisplayModel U8G2_SSD1309_128X64_NONAME2_F_HW_I2C
#define DisplayModel U8G2_SSD1306_128X64_NONAME_F_HW_I2C

// Pines I2C
#define SDA_PIN 21
#define SCL_PIN 22

// Dirección I2C del driver de input (ATTINY85)
#define ATTINY_ADDRESS 8

// Comandos para el ATTINY85
#define CMD_RESET_COUNTER 0x01
#define CMD_RESET_BUTTON 0x02

#endif // CONFIG_H
