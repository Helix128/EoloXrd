#ifndef AFM07_HPP
#define AFM07_HPP

#include <ModbusMaster.h>

// Clase para manejar el sensor de flujo AFM07
class AFM07
{
private:
    // Definición de pines
    static const uint8_t RS485_TX = 17;
    static const uint8_t RS485_RX = 16;
    static const uint8_t RS485_DE_RE_PIN = 4;

    // Registros del sensor
    static const uint8_t AFM07_ID = 0x01;
    static const uint16_t REG_INSTANT_FLOW = 0x0000;

    static const uint8_t FACTOR_LECTURA = 100;

    HardwareSerial ModbusSerial;
    ModbusMaster node;

    static void preTransmission()
    {
        digitalWrite(RS485_DE_RE_PIN, HIGH); // Configura módulo RS485 en modo transmisión
    }

    static void postTransmission()
    {
        digitalWrite(RS485_DE_RE_PIN, LOW); // Configura módulo RS485 en modo recepción
    }

public:
    AFM07() : ModbusSerial(2)
    {
    }

    float velocity = 0.0; // m/s
    float flow = 0.0;    // L/min

    void begin()
    {
        pinMode(RS485_DE_RE_PIN, OUTPUT);
        digitalWrite(RS485_DE_RE_PIN, LOW); // Configura módulo RS485 en modo recepción

        ModbusSerial.begin(9600, SERIAL_8N1, RS485_RX, RS485_TX); //

        node.begin(AFM07_ID, ModbusSerial);
        node.preTransmission(preTransmission);
        node.postTransmission(postTransmission);
    }

    void readData()
    {
        uint8_t result;

        result = node.readHoldingRegisters(REG_INSTANT_FLOW, 1); // Lee el registro de flujo instantáneo

        if (result == node.ku8MBSuccess)
        {
            uint16_t valorCrudo = node.getResponseBuffer(0);
            flow = (float)valorCrudo / FACTOR_LECTURA;
            velocity = flow; // TODO: CALCULAR VALOR REAL (?) Innecesario tal vez
        }
        else
        {
            flow = -1.0;
            velocity = -1.0;
        }
    }
};

#endif
