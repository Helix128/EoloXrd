#include <Arduino.h>
#include <ModbusMaster.h>

#if !defined(EOLO_MODEL_DRON) && !defined(EOLO_MODEL_STANDARD) && \
    !defined(EOLO_MODEL_EXPRESS) && !defined(EOLO_MODEL_EXPRESS_LEGACY)
#define EOLO_MODEL_DRON
#endif
#include "../EoloDemoPinout.h"

/*
  Demo independiente AFM07 por RS485/Modbus RTU usando ModbusMaster.

  No depende de archivos de src ni del RS485Bus del proyecto.
  Abrir esta carpeta/sketch en Arduino IDE y seleccionar una placa ESP32.

  Libreria necesaria en Arduino IDE:
    - ModbusMaster

  Conexiones usadas segun EOLO_MODEL_* en ../EoloDemoPinout.h.

  Parametros esperados:
    Slave ID: 2
    Baud: 4800 8N1
    Registro flujo instantaneo: 0x0000
    Factor: valor / 10 = L/min
*/

static const uint32_t SERIAL_BAUD = 115200;
static const uint32_t RS485_BAUD = 4800;
static const uint8_t AFM07_SLAVE_ID = 2;
static const uint16_t AFM07_FLOW_REG = 0x0000;

HardwareSerial rs485(2);
ModbusMaster afm07;

void preTransmission() {
  digitalWrite(RS485_DE_RE_PIN, HIGH);
  delayMicroseconds(200);
}

void postTransmission() {
  delayMicroseconds(200);
  digitalWrite(RS485_DE_RE_PIN, LOW);
}

const char *modbusErrorText(uint8_t code) {
  switch (code) {
    case ModbusMaster::ku8MBSuccess: return "exito";
    case ModbusMaster::ku8MBIllegalFunction: return "funcion ilegal";
    case ModbusMaster::ku8MBIllegalDataAddress: return "direccion ilegal";
    case ModbusMaster::ku8MBIllegalDataValue: return "valor ilegal";
    case ModbusMaster::ku8MBSlaveDeviceFailure: return "fallo en esclavo";
    case ModbusMaster::ku8MBInvalidSlaveID: return "ID invalido";
    case ModbusMaster::ku8MBInvalidFunction: return "funcion invalida";
    case ModbusMaster::ku8MBResponseTimedOut: return "timeout";
    case ModbusMaster::ku8MBInvalidCRC: return "CRC invalido";
    default: return "error desconocido";
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);

  enableDemoPeripheralPower();

  pinMode(RS485_DE_RE_PIN, OUTPUT);
  digitalWrite(RS485_DE_RE_PIN, LOW);

  rs485.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  afm07.begin(AFM07_SLAVE_ID, rs485);
  afm07.preTransmission(preTransmission);
  afm07.postTransmission(postTransmission);

  Serial.println();
  Serial.println("Demo AFM07 independiente");
  Serial.println("Leyendo ID 2, registro 0x0000 cada 1 segundo...");
}

void loop() {
  uint8_t result = afm07.readHoldingRegisters(AFM07_FLOW_REG, 1);

  if (result == ModbusMaster::ku8MBSuccess) {
    uint16_t rawFlow = afm07.getResponseBuffer(0);
    float flowLpm = rawFlow / 10.0f;
    Serial.printf("AFM07 OK | raw=%u | flujo=%.2f L/min\n", rawFlow, flowLpm);
  } else {
    Serial.printf("AFM07 sin lectura valida | codigo=0x%02X (%s)\n", result, modbusErrorText(result));
    Serial.println("Revisar energia, A/B, GND comun, ID y baudrate.");
  }

  delay(1000);
}
