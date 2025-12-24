#ifndef PLANTOWER_H
#define PLANTOWER_H

#include "../Config.h"

#define PT_TX 32
#define PT_RX 34
#define PT_PWR 4
class Plantower
{
private:
    unsigned char bufferRTT[32] = {};
    bool validateChecksum() {
        unsigned int CR1 = (bufferRTT[30] << 8) + bufferRTT[31];
        unsigned int CR2 = 0;
        for (int i = 0; i < 30; i++) CR2 += bufferRTT[i];
        return (CR1 == CR2);
    }

public:
    float pm1 = -1.0, pm25 = -1.0, pm10 = -1.0;

    bool isActive = false;
    void begin() {   
        Serial2.begin(9600, SERIAL_8N1, PT_RX, PT_TX);
        Serial2.setTimeout(100); 
        pinMode(PT_PWR, OUTPUT);
        digitalWrite(PT_PWR, HIGH);
        delay(500);
        while(Serial2.available()) Serial2.read();
        isActive = true;
        Serial.println("Plantower inicializado");
    }

    void setPower(bool active){
        // stub, ya no se usa.
    }

    void readData(int maxRetries = 32) {
        Serial.println("Leyendo datos Plantower...");
        if (!isActive) return;
        bool dataRead = false;
        for (int attempt = 0; attempt < maxRetries; attempt++) {
            if (readDataAttempt()){
                dataRead = true;
                break;
            }
            delay(25); 
        }
        if (!dataRead) {
            pm1 = -1.0;
            pm25 = -1.0;
            pm10 = -1.0;
            Serial.println("Error leyendo datos Plantower.");
        }
        else{
            Serial.print("Datos Plantower leídos: PM1.0=");
            Serial.print(pm1);
            Serial.print(" µg/m3, PM2.5=");
            Serial.print(pm25);
            Serial.print(" µg/m3, PM10=");
            Serial.print(pm10);
            Serial.println(" µg/m3");
        }
    }

    bool readDataAttempt(){
        if (!isActive) return true; 
        while (Serial2.available() >= 32) {
            if (Serial2.peek() != 0x42) {
                Serial2.read(); 
                continue;       
            }
            if (Serial2.readBytes(bufferRTT, 32) != 32) return false; 
            if (validateChecksum()) {
                pm1  = (float)((bufferRTT[10] << 8) + bufferRTT[11]);
                pm25 = (float)((bufferRTT[12] << 8) + bufferRTT[13]);
                pm10 = (float)((bufferRTT[14] << 8) + bufferRTT[15]);
                return true;
            }
        }
        return false; 
    }
};
#endif // PLANTOWER_H