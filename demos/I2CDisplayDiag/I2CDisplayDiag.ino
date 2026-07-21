#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#if !defined(EOLO_MODEL_DRON) && !defined(EOLO_MODEL_STANDARD) && \
    !defined(EOLO_MODEL_EXPRESS) && !defined(EOLO_MODEL_EXPRESS_LEGACY)
#define EOLO_MODEL_EXPRESS
#endif
#include "../EoloDemoPinout.h"

/*
  Diagnostico I2C + Display para EOLO.
  - Escanea el bus I2C e imprime direcciones (identifica BME/RTC/ATTINY).
  - Prueba SSD1306 y luego SSD1309 dibujando un patron reconocible.
  - Reporta a que bus clock queda estable.

  Objetivo: confirmar el controlador real del panel y el mapa I2C sin
  depender del firmware completo (single-thread, sin colisiones de bus).
*/

static const uint32_t SERIAL_BAUD = 115200;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C disp1306(U8G2_R0, U8X8_PIN_NONE);
U8G2_SSD1309_128X64_NONAME0_F_HW_I2C disp1309(U8G2_R0, U8X8_PIN_NONE);

static void scanI2C() {
  Serial.println("=== SCAN I2C ===");
  uint8_t found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  0x%02X", a);
      if (a == 0x68 || a == 0x69) Serial.print("  <- DS3231 RTC");
      if (a == 0x76 || a == 0x77) Serial.print("  <- BME280");
      if (a == 0x57)              Serial.print("  <- DS3231 EEPROM");
      if (a == 0x08)              Serial.print("  <- ATTINY/ProMini encoder");
      if (a == 0x3C || a == 0x3D) Serial.print("  <- OLED SSD130x");
      Serial.println();
      found++;
    }
  }
  Serial.printf("Total I2C: %u dispositivo(s)\n", found);
}

template <typename D>
static void drawPattern(D &d, const char *label) {
  d.clearBuffer();
  d.setFont(u8g2_font_6x10_tf);
  d.drawStr(0, 9, "EOLO DIAG");
  d.drawStr(0, 22, label);
  d.drawFrame(0, 26, 128, 20);
  for (int x = 2; x < 126; x += 4) d.drawPixel(x, 36);
  d.drawStr(0, 60, "1234567890 ABCDEF");
  d.sendBuffer();
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(800);
  enableDemoPeripheralPower();
  delay(200);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);

  Serial.println();
  Serial.println("### EOLO I2C + DISPLAY DIAG ###");
  Serial.printf("SDA=%d SCL=%d\n", I2C_SDA_PIN, I2C_SCL_PIN);
  scanI2C();

  Serial.println("Probando SSD1306 (mira la pantalla)...");
  disp1306.begin();
  disp1306.setBusClock(100000);
  for (int i = 0; i < 5; i++) { drawPattern(disp1306, "SSD1306 100k"); delay(700); }

  Serial.println("Probando SSD1309 (mira la pantalla)...");
  disp1309.begin();
  disp1309.setBusClock(100000);
  for (int i = 0; i < 5; i++) { drawPattern(disp1309, "SSD1309 100k"); delay(700); }

  Serial.println("Fin de prueba. Reportar cual controlador se ve NITIDO/COMPLETO.");
}

void loop() {
  // Alterna cada 2s para comparacion visual continua
  drawPattern(disp1306, "SSD1306 (loop)");
  delay(2000);
  drawPattern(disp1309, "SSD1309 (loop)");
  delay(2000);
}
