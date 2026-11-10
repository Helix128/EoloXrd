#ifndef EOLO_SYSTEM_DIAG_I2C_H
#define EOLO_SYSTEM_DIAG_I2C_H

#include <Arduino.h>
#include <Wire.h>
#include <Eolo/Core/Sensors/FS3000FlowModel.h>
#include <Eolo/Core/Power/BatteryProtocol.h>
#include "../EoloDemoPinout.h"
#include "../../src/Common/Sensors/DirectBME280.h"

// Diagnóstico secuencial: este sketch es el único dueño de Wire y Wire1.
// Solo se sondean direcciones esperadas; un ACK no identifica por sí solo el chip.
namespace SystemI2CDiag {

static uint8_t probe(TwoWire &wire, const char *bus, const char *device, uint8_t address) {
    const uint32_t start = millis();
    wire.beginTransmission(address);
    const uint8_t error = wire.endTransmission();
    Serial.printf("I2C sondeo bus=%s dispositivo=%s addr=0x%02X error_wire=%u duracion_ms=%lu\n",
                  bus, device, address, error, (unsigned long)(millis() - start));
    return error;
}

static bool read(TwoWire &wire, const char *name, uint8_t address,
                 uint8_t *data, size_t size, int reg = -1) {
    const uint32_t start = millis();
    if (reg >= 0) {
        wire.beginTransmission(address);
        wire.write(static_cast<uint8_t>(reg));
        uint8_t error = wire.endTransmission(false);
        if (error) {
            Serial.printf("I2C lectura=%s addr=0x%02X registro=0x%02X error_wire=%u duracion_ms=%lu\n",
                          name, address, reg, error, (unsigned long)(millis() - start));
            return false;
        }
    }
    const size_t received = wire.requestFrom(address, size, true);
    const uint32_t elapsed = millis() - start;
    if (received != size) {
        while (wire.available()) wire.read();
        Serial.printf("I2C lectura=%s addr=0x%02X esperados=%u recibidos=%u duracion_ms=%lu\n",
                      name, address, (unsigned)size, (unsigned)received, (unsigned long)elapsed);
        return false;
    }
    for (size_t i = 0; i < size; ++i) data[i] = wire.read();
    Serial.printf("I2C lectura=%s addr=0x%02X bytes=%u duracion_ms=%lu\n",
                  name, address, (unsigned)size, (unsigned long)elapsed);
    return true;
}

static bool bcd(uint8_t value, uint8_t max, uint8_t &out) {
    if ((value & 0x0F) > 9 || (value >> 4) > 9) return false;
    out = (value >> 4) * 10 + (value & 0x0F);
    return out <= max;
}

static void rtc(TwoWire &wire) {
    uint8_t data[7] = {}, status = 0;
    const bool timeRead = read(wire, "rtc_fecha", 0x68, data, sizeof(data), 0x00);
    const bool statusRead = read(wire, "rtc_estado", 0x68, &status, 1, 0x0F);
    if (statusRead)
        Serial.printf("RTC compatible_DS3231 status=0x%02X osf=%u (osf=1 indica detencion desde su ultima limpieza)\n",
                      status, (status & 0x80) != 0);
    if (!timeRead) return;
    uint8_t second = 0, minute = 0, hour = 0, day = 0, month = 0, year = 0;
    bool valid = bcd(data[0] & 0x7F, 59, second) && bcd(data[1] & 0x7F, 59, minute);
    if (data[2] & 0x40) {
        uint8_t h12 = 0;
        valid = bcd(data[2] & 0x1F, 12, h12) && h12 >= 1 && valid;
        hour = (h12 % 12) + ((data[2] & 0x20) ? 12 : 0);
    } else {
        valid = bcd(data[2] & 0x3F, 23, hour) && valid;
    }
    valid = bcd(data[4] & 0x3F, 31, day) && day >= 1 && valid;
    valid = bcd(data[5] & 0x1F, 12, month) && month >= 1 && valid;
    valid = bcd(data[6], 99, year) && valid;
    if (valid) {
        const unsigned fullYear = 2000U + year + ((data[5] & 0x80) ? 100U : 0U);
        static const uint8_t daysPerMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        unsigned maxDay = daysPerMonth[month - 1];
        if (month == 2 && fullYear % 4 == 0 && (fullYear % 100 != 0 || fullYear % 400 == 0)) ++maxDay;
        valid = day <= maxDay;
        Serial.printf("RTC compatible_DS3231 fecha=%04u-%02u-%02uT%02u:%02u:%02u calendario=%s\n",
                      fullYear, month, day, hour, minute, second, valid ? "valido" : "invalido");
    } else {
        Serial.println("RTC fecha=invalida (registro BCD fuera de rango)");
    }
}

static void bme(TwoWire &wire) {
    bool ack = false, otherError = false, identified = false;
    for (uint8_t address = 0x76; address <= 0x77; ++address) {
        const uint8_t error = probe(wire, "principal", "bme", address);
        if (error != 0) {
            if (error != 2 && error != 3) otherError = true;
            continue;
        }
        ack = true;
        uint8_t id = 0;
        if (!read(wire, "bme_chip_id", address, &id, 1, 0xD0)) continue;
        Serial.printf("BME addr=0x%02X chip_id=0x%02X resultado=%s\n",
                      address, id, id == 0x60 ? "compatible_BME280" : "no_BME280");
        if (id == 0x60) identified = true;
    }
    if (!ack && !otherError)
        Serial.println("BME sin respuesta confirmada en 0x76 y 0x77; codigos 2/3 no confirman ausencia fisica");
    else if (!ack)
        Serial.println("BME estado=indeterminado por error de transporte; revisar codigos Wire");
    if (!identified) return;

    Serial.println("BME inicializacion: configuracion temporal del sensor, sin cambios persistentes");
    DirectBME280 sensor;
    const uint32_t started = millis();
    if (!sensor.begin()) {
        Serial.printf("BME inicializacion=fallo causa=%s duracion_ms=%lu\n",
                      DirectBME280::failureName(sensor.lastFailure()), (unsigned long)(millis() - started));
        return;
    }
    // Normal x16: permitir la primera conversión antes de leer los datos.
    delay(200);
    float temperature = 0.0f, humidity = 0.0f, pressure = 0.0f;
    if (!sensor.readData(temperature, humidity, pressure)) {
        const I2CBus::Stats stats = I2CBus::getInstance().getStats();
        Serial.printf("BME lectura=invalida ultimo_i2c=%s addr=0x%02X duracion_ms=%lu\n",
                      I2CBus::resultName(stats.lastResult), stats.lastAddress, (unsigned long)(millis() - started));
        return;
    }
    Serial.printf("BME lectura=plausible temperatura_c=%.2f humedad_pct=%.2f presion_hpa=%.2f duracion_ms=%lu\n",
                  temperature, humidity, pressure, (unsigned long)(millis() - started));
}

static void encoder(TwoWire &wire, const char *bus) {
    uint8_t frame[3] = {};
    if (read(wire, "encoder", EoloConfig::attinyAddress, frame, sizeof(frame)))
        Serial.printf("ENCODER bus=%s direccion=%u contador=%u boton=%u (sin comando reset)\n",
                      bus, frame[0], frame[1], frame[2]);
}

static void fs3000(TwoWire &wire) {
    uint8_t frame[FS3000FlowModel::FrameSize] = {};
    if (!read(wire, "fs3000", FS3000FlowModel::I2cAddress, frame, sizeof(frame))) return;
    uint16_t raw = 0;
    if (!FS3000FlowModel::parseFrame(frame, raw)) {
        Serial.printf("FS3000 trama_invalida=%02X%02X%02X%02X%02X\n",
                      frame[0], frame[1], frame[2], frame[3], frame[4]);
        return;
    }
    float velocity = FS3000FlowModel::velocityFromRaw(raw);
    Serial.printf("FS3000 trama=valida raw=%u velocidad_m_s=%.3f caudal_estimado_l_min=%.3f\n",
                  raw, velocity, FS3000FlowModel::flowFromVelocity(velocity));
}

static void battery(TwoWire &wire) {
    uint8_t frame[BatteryProtocol::FrameSize] = {};
    if (!read(wire, "bateria_dual", 0x0A, frame, sizeof(frame))) return;
    BatteryProtocolPacket data;
    if (!BatteryProtocol::decode(frame, sizeof(frame), data)) {
        Serial.println("BATERIA_DUAL trama=invalida");
        return;
    }
    Serial.printf("BATERIA_DUAL dc_v=%.2f bateria0_v=%.2f bateria1_v=%.2f mosfet=%u\n",
                  data.dcVoltage, data.batt0Voltage, data.batt1Voltage, data.activeMosfet);
}

} // namespace SystemI2CDiag

// La alimentación del perfil debe estar activa y estabilizada antes de llamar.
static void diagI2C() {
    using namespace SystemI2CDiag;
    Serial.printf("I2C bus=principal sda=%d scl=%d hz=%lu timeout_ms=%lu\n",
                  SDA_PIN, SCL_PIN, (unsigned long)EoloConfig::i2cClockHz,
                  (unsigned long)EoloConfig::i2cTransactionTimeoutMs);
    if (!Wire.begin(SDA_PIN, SCL_PIN, EoloConfig::i2cClockHz)) {
        Serial.println("I2C bus=principal inicio=fallo");
        return;
    }
    Wire.setTimeOut(EoloConfig::i2cTransactionTimeoutMs);
    Serial.println("I2C: sondeo limitado a direcciones esperadas para el perfil; error_wire=0 indica ACK");
    if (probe(Wire, "principal", "rtc_posible", 0x68) == 0) rtc(Wire);
    bme(Wire);
#ifdef FEATURE_FLOW_FS3000
    if (probe(Wire, "principal", "fs3000_posible", FS3000FlowModel::I2cAddress) == 0) fs3000(Wire);
#endif
#ifdef FEATURE_DUAL_BATTERY
    if (probe(Wire, "principal", "bateria_dual_posible", 0x0A) == 0) battery(Wire);
#endif
#if !defined(FEATURE_HEADLESS) && !EOLO_DISPLAY_SPI
    probe(Wire, "principal", "oled_posible", 0x3C);
#endif
#ifndef FEATURE_HEADLESS
#if ENCODER_SDA_PIN >= 0
    Serial.printf("I2C bus=encoder sda=%d scl=%d hz=%lu timeout_ms=%lu\n",
                  ENCODER_SDA_PIN, ENCODER_SCL_PIN, (unsigned long)EoloConfig::encoderBusClockHz,
                  (unsigned long)EoloConfig::i2cTransactionTimeoutMs);
    if (Wire1.begin(ENCODER_SDA_PIN, ENCODER_SCL_PIN, EoloConfig::encoderBusClockHz)) {
        Wire1.setTimeOut(EoloConfig::i2cTransactionTimeoutMs);
        if (probe(Wire1, "encoder", "encoder_posible", EoloConfig::attinyAddress) == 0)
            encoder(Wire1, "encoder");
    } else {
        Serial.println("I2C bus=encoder inicio=fallo");
    }
#else
    if (probe(Wire, "principal", "encoder_posible", EoloConfig::attinyAddress) == 0)
        encoder(Wire, "principal");
#endif
#endif
}

#endif
