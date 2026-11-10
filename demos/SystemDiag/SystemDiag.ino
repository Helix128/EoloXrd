#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Preferences.h>
#include <Eolo/Core/Communication/RS485Protocol.h>
#include <Eolo/Core/Power/BatteryMath.h>
#include <Eolo/Core/Sensors/NtcThermistor.h>
#include <Eolo/Core/Sensors/PlantowerParser.h>
#include "../EoloDemoPinout.h"
#include "I2CDiag.h"
#include "StorageDiag.h"
#include <esp_system.h>

// El modelo se selecciona al compilar: escanear pines no identifica una placa.
static void motorsOff()
{
    const int pins[] = {MOTOR_PWM_PIN_0, MOTOR_PWM_PIN_1};
    for (int pin : pins) {
        if (pin >= 0) {
            digitalWrite(pin, EoloConfig::motorPwmInverted ? HIGH : LOW);
            pinMode(pin, OUTPUT);
        }
    }
}

static void inputsDiag()
{
    Serial.println("\n== Entradas ==");
#if defined(EOLO_MODEL_DRON_LEGACY)
    const int switches[] = {WAIT_SW0_PIN, WAIT_SW1_PIN, DURATION_SW0_PIN, DURATION_SW1_PIN};
    for (unsigned i = 0; i < 4; ++i) {
        pinMode(switches[i], INPUT);
        Serial.printf("DIP%u gpio=%d nivel=%d (verificar movimiento manualmente)\n",
                      i + 1, switches[i], digitalRead(switches[i]));
    }
    pinMode(NTC_PIN, INPUT);
    const int raw = analogRead(NTC_PIN);
    float temp = 0, resistance = 0;
    if (NtcThermistor::computeTemperature(raw, temp, resistance))
        Serial.printf("NTC raw=%d temperatura=%.1f C resistencia=%.0f ohm\n", raw, temp, resistance);
    else
        Serial.printf("NTC lectura no valida raw=%d\n", raw);
#elif defined(EOLO_MODEL_EXPRESS) || defined(EOLO_MODEL_EXPRESS_LEGACY) || defined(EOLO_MODEL_EXPRESS_LEGACY_SPI)
    pinMode(BATTERY_ADC_PIN, INPUT);
    const int raw = analogRead(BATTERY_ADC_PIN);
    Serial.printf("Bateria ADC gpio=%d raw=%d estimacion=%.2f V (no mide salud)\n",
                  BATTERY_ADC_PIN, raw, BatteryMath::voltageFromAdcLevel(raw));
#elif defined(EOLO_MODEL_DRON)
    pinMode(BATTERY_ADC_PIN, INPUT);
    const uint32_t mv = analogReadMilliVolts(BATTERY_ADC_PIN);
    Serial.printf("Bateria ADC gpio=%d pin=%lu mV estimacion=%.2f V (no mide salud)\n",
                  BATTERY_ADC_PIN, (unsigned long)mv, BatteryMath::pack2sVoltageFromPinMv(mv));
#else
    Serial.println("Bateria dual: consultar lectura I2C; ADC no corresponde a este perfil.");
#endif
}

#ifdef FEATURE_FLOW_AFM07
static HardwareSerial diagRs485(1);

static void modbusRead(const char *name, uint8_t slave, uint16_t reg, uint8_t count)
{
    uint8_t request[8];
    uint8_t response[32];
    uint16_t registers[2] = {};
    EoloCore::ModbusRtuProtocol::buildReadHolding(slave, reg, count, request, sizeof(request));
    while (diagRs485.available()) diagRs485.read();
    digitalWrite(RS485_DE_RE_PIN, HIGH);
    delayMicroseconds(200);
    diagRs485.write(request, sizeof(request));
    diagRs485.flush();
    delayMicroseconds(200);
    digitalWrite(RS485_DE_RE_PIN, LOW);
    uint32_t started = millis();
    size_t used = 0;
    const size_t expected = 5 + count * 2;
    while (millis() - started < 1000 && used < expected) {
        if (diagRs485.available()) {
            response[used++] = diagRs485.read();
            if (used >= 5 && (response[1] & 0x80)) break;
        } else delay(1);
    }
    const auto result = EoloCore::ModbusRtuProtocol::parseReadResponse(response, used, slave, count, registers);
    if (result.status == EoloCore::ModbusReadStatus::Ok) {
        Serial.printf("%s OK id=%u registro=0x%04X valor=%u", name, slave, reg, registers[0]);
        if (count > 1) Serial.printf(" registro1=%u", registers[1]);
        Serial.printf(" tiempo=%lu ms\n", (unsigned long)(millis() - started));
    } else {
        Serial.printf("%s SIN LECTURA VALIDA id=%u registro=0x%04X estado=%u excepcion=%u bytes=%u tiempo=%lu ms\n",
                      name, slave, reg, (unsigned)result.status, result.exceptionCode,
                      (unsigned)used, (unsigned long)(millis() - started));
    }
}

static void rs485Diag()
{
    Serial.println("\n== RS485: lecturas Modbus ==");
    pinMode(RS485_DE_RE_PIN, OUTPUT);
    digitalWrite(RS485_DE_RE_PIN, LOW);
    diagRs485.begin(EoloCore::ModbusRtuProtocol::Afm07BaudRate, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
    modbusRead("AFM07 flujo", EoloCore::ModbusRtuProtocol::Afm07SlaveId,
               EoloCore::ModbusRtuProtocol::Afm07FlowRegister, 1);
    delay(250);
    modbusRead("AFM07 diagnostico (0=sin alerta)", EoloCore::ModbusRtuProtocol::Afm07SlaveId,
               EoloCore::ModbusRtuProtocol::Afm07DiagnosticRegister, 1);
#ifdef FEATURE_ANEMOMETER
    delay(250);
    modbusRead("Anemometro", 1, 0, 2);
#endif
    diagRs485.end();
}
#else
static void rs485Diag() { Serial.println("\nRS485: no configurado en este perfil"); }
#endif

static void plantowerDiag(uint32_t listenMs = 5000)
{
#ifdef FEATURE_PLANTOWER
    Serial.printf("\n== Plantower: escucha UART hasta %lu ms rx=%d tx=%d baud=9600 ==\n",
                  (unsigned long)listenMs, PT_RX, PT_TX);
    HardwareSerial sensor(2);
    sensor.begin(9600, SERIAL_8N1, PT_RX, PT_TX);
    PlantowerParser parser;
    uint32_t start = millis();
    bool valid = false;
    uint32_t bytes = 0, headers = 0;
    uint8_t previous = 0;
    uint8_t first[32] = {};
    size_t firstCount = 0;
    while (millis() - start < listenMs && !valid) {
        while (sensor.available() && !valid) {
            uint8_t byte = sensor.read();
            if (firstCount < sizeof(first)) first[firstCount++] = byte;
            ++bytes;
            if (previous == 0x42 && byte == 0x4D) ++headers;
            previous = byte;
            valid = parser.processByte(byte);
        }
        delay(1);
    }
    if (valid) {
        const auto data = parser.getData();
        Serial.printf("Plantower OK PM1=%u PM2.5=%u PM10=%u ug/m3\n", data.pm1_0, data.pm2_5, data.pm10_0);
    } else {
        Serial.printf("Plantower SIN DATOS VALIDOS bytes=%lu cabeceras_424d=%lu primeros_bytes=",
                      (unsigned long)bytes, (unsigned long)headers);
        if (!firstCount) Serial.print("ninguno");
        for (size_t i = 0; i < firstCount; ++i) Serial.printf("%02X", first[i]);
        Serial.println("; cero bytes, trama invalida y PM=0 son estados distintos");
    }
    sensor.end();
#else
    (void)listenMs;
    Serial.println("\nPlantower: no configurado en este perfil");
#endif
}

static void runDiagnosis()
{
    Serial.printf("\n=== SystemDiag inicio modelo=%s t=%lu ms ===\n", EoloConfig::kModelName, (unsigned long)millis());
    diagI2C();
    diagStorage();
    inputsDiag();
    rs485Diag();
    plantowerDiag();
    Serial.println("\nNO PROBADO: bombas, pantalla/LED visual, precision de sensores y reloj, encoder mecanico,");
    Serial.println("validez de calibracion NVS, escritura SD y modem. Consultar demos especificas.");
    Serial.println("=== SystemDiag fin; r=repetir, p=escuchar Plantower 30 s ===");
}

void setup()
{
    motorsOff();
    Serial.begin(115200);
    delay(500);
    Serial.printf("SystemDiag modelo=%s reset=%d SDK=%s\n", EoloConfig::kModelName,
                  (int)esp_reset_reason(), ESP.getSdkVersion());
#ifdef EOLO_MODEL_EXPRESS_LEGACY_SPI
    Serial.println("Pinout Express Rev.1 alternativo SPI (validacion fisica pendiente)");
#endif
    enableDemoPeripheralPower();
    const uint32_t warmup = EoloConfig::i2cWarmupMs > 1000 ? EoloConfig::i2cWarmupMs : 1000;
    Serial.printf("Alimentacion de perifericos: espera %lu ms\n", (unsigned long)warmup);
    delay(warmup);
    runDiagnosis();
}

void loop()
{
    if (Serial.available()) {
        const int c = Serial.read();
        if (c == 'r' || c == 'R') runDiagnosis();
        if (c == 'p' || c == 'P') plantowerDiag(30000);
    }
    delay(20);
}
