#ifndef ANEMOMETER_H
#define ANEMOMETER_H

#include <SoftwareSerial.h>
#include "../Config.h"

#define ANEM_RE 33
#define ANEM_DE 32

// RX, TX para SoftwareSerial (igual que el sketch original)
static const uint8_t ANEM_RX_PIN = 25;
static const uint8_t ANEM_TX_PIN = 35;

class Anemometer
{
private:
    // Petición Modbus/Anemómetro (igual que el sketch)
    const uint8_t requestFrame[8] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B};
    uint8_t response[9] = {0};

    SoftwareSerial modem{ANEM_RX_PIN, ANEM_TX_PIN};

public:
    float velocity = -1.0f; // m/s
    float windKph = -1.0f;  // km/h
    int direction = -1;     // grados
    bool isReady = false;

    void begin()
    {
        if (isReady) {
            Serial.println("Anemometer ya inicializado, skipping...");
            return;
        }

        pinMode(ANEM_RE, OUTPUT);
        pinMode(ANEM_DE, OUTPUT);
        digitalWrite(ANEM_RE, LOW);
        digitalWrite(ANEM_DE, LOW);

        modem.begin(4800);
        delay(100);
        Serial.println("Anemometer inicializado");
        isReady = true;
#if CHECK_SENSORS
        testSensor();
#endif
    }

    void readData()
    {
        if (!isReady)
        {
            velocity = -1.0f;
            windKph = -1.0f;
            direction = -1;
            return;
        }

        // Transmitir petición
        digitalWrite(ANEM_DE, HIGH);
        digitalWrite(ANEM_RE, HIGH);
        delay(10);
        modem.write(requestFrame, sizeof(requestFrame));
        modem.flush();
        digitalWrite(ANEM_DE, LOW);
        digitalWrite(ANEM_RE, LOW);
        delay(10);

        // Esperar respuesta (9 bytes) con timeout
        unsigned long start = millis();
        while (modem.available() < 9 && (millis() - start) < 1000)
        {
            delay(1);
        }

        if (modem.available() >= 9)
        {
            uint8_t idx = 0;
            while (modem.available() && idx < 9)
            {
                response[idx++] = modem.read();
            }

            // Parseo: velocidad en registros 3-4 (valor entero /100 => m/s)
            int rawSpeed = (int(response[3]) << 8) | int(response[4]);
            velocity = rawSpeed / 100.0f;
            windKph = velocity * 3.6f;

            // Direccion en registros 5-6
            int rawDir = (int(response[5]) << 8) | int(response[6]);
            direction = rawDir;

        }
        else
        {
            // Timeout / trama incompleta
            velocity = -1.0f;
            windKph = -1.0f;
            direction = -1;
        }
    }

#if CHECK_SENSORS
    void testSensor()
    {
        Serial.println("Probando Anemometer...");
        for (int i = 0; i < 10; i++)
        {
            readData();
            Serial.print("Velocidad (m/s): ");
            Serial.print(velocity);
            Serial.print(" , Velocidad (km/h): ");
            Serial.print(windKph);
            Serial.print(" , Direccion: ");
            Serial.println(direction);
            delay(500);
        }
    }
#endif
};

#endif
