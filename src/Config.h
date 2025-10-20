#ifndef CONFIG_H
#define CONFIG_H

// CONFIGURACIONES GENERALES DEL PROGRAMA

// Habilitar pruebas de componentes al inicio (desactivar para booteo rápido)
#define CHECK_SENSORS false

// Modelo de pantalla a usar en U8G2 (default U8G2_SSD1309_128x64_NONAME2_F_HW_I2C)
#define DisplayModel U8G2_SSD1309_128X64_NONAME2_F_HW_I2C

// Pines I2C
#define SDA_PIN 21
#define SCL_PIN 22

// Dirección I2C del driver de input (ATTINY85)
#define ATTINY_ADDRESS 8

// Comandos para el ATTINY85
#define CMD_RESET_COUNTER 0x01
#define CMD_RESET_BUTTON 0x02

#define TUBE_RADIUS 0.00645f // Radio del tubo de flujo en metros (12.7mm = 1/2")

#endif // CONFIG_H
