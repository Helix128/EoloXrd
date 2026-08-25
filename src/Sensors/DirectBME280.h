#ifndef EOLO_DIRECT_BME280_H
#define EOLO_DIRECT_BME280_H

#include <Arduino.h>
#include <math.h>
#include "../Board/I2CBus.h"

// Implementación I2C mínima del BME280. Evita Adafruit BusIO para que cada
// operación quede serializada y con resultado verificable en I2CBus.
class DirectBME280 {
public:
    enum class InitFailure : uint8_t {
        None, Nack7677, WrongChipId, InvalidCalibration, ConfigurationFailed,
        Timeout, BusBusy, BusStuck
    };

    static const char *failureName(InitFailure failure) {
        switch (failure) {
        case InitFailure::None: return "NONE";
        case InitFailure::Nack7677: return "NACK_76_77";
        case InitFailure::WrongChipId: return "WRONG_CHIP_ID";
        case InitFailure::InvalidCalibration: return "INVALID_CALIBRATION";
        case InitFailure::ConfigurationFailed: return "CONFIGURATION_FAILED";
        case InitFailure::Timeout: return "TIMEOUT";
        case InitFailure::BusBusy: return "BUS_BUSY";
        case InitFailure::BusStuck: return "BUS_STUCK";
        }
        return "UNKNOWN";
    }

    bool begin() {
        _lastFailure = InitFailure::Nack7677;
        if (I2CBus::getInstance().linesStuck()) {
            _lastFailure = InitFailure::BusStuck;
            return false;
        }
        static constexpr uint8_t candidates[] = {0x76, 0x77};
        for (uint8_t candidate : candidates) {
            I2CBus::Result probe = I2CBus::getInstance().probe(candidate);
            if (probe != I2CBus::Result::Ok) {
                if (probe == I2CBus::Result::Timeout) _lastFailure = InitFailure::Timeout;
                else if (probe == I2CBus::Result::Busy) _lastFailure = InitFailure::BusBusy;
                continue;
            }
            uint8_t id = 0;
            if (!read(candidate, 0xD0, &id, 1)) {
                setTransportFailure(candidate);
                continue;
            }
            if (id != 0x60) {
                _lastFailure = InitFailure::WrongChipId;
                continue;
            }
            if (!loadCalibration(candidate)) {
                if (_lastFailure != InitFailure::Timeout && _lastFailure != InitFailure::BusBusy)
                    _lastFailure = InitFailure::InvalidCalibration;
                continue;
            }
            if (!configure(candidate)) {
                if (_lastFailure != InitFailure::Timeout && _lastFailure != InitFailure::BusBusy)
                    _lastFailure = InitFailure::ConfigurationFailed;
                continue;
            }
            _address = candidate;
            _ready = true;
            _lastFailure = InitFailure::None;
            return true;
        }
        _ready = false;
        return false;
    }

    InitFailure lastFailure() const { return _lastFailure; }

    bool readData(float &temperatureC, float &humidityPct, float &pressureHpa) const {
        if (!_ready)
            return false;
        uint8_t raw[8] = {};
        if (!read(_address, 0xF7, raw, sizeof(raw)))
            return false;
        int32_t adcP = (int32_t)(((uint32_t)raw[0] << 12) | ((uint32_t)raw[1] << 4) | (raw[2] >> 4));
        int32_t adcT = (int32_t)(((uint32_t)raw[3] << 12) | ((uint32_t)raw[4] << 4) | (raw[5] >> 4));
        int32_t adcH = (int32_t)(((uint16_t)raw[6] << 8) | raw[7]);

        int32_t fine = 0;
        float temp = compensateTemperature(adcT, fine);
        float pressure = compensatePressure(adcP, fine);
        float humidity = compensateHumidity(adcH, fine);
        if (!isfinite(temp) || !isfinite(pressure) || !isfinite(humidity) ||
            temp < -40.0f || temp > 85.0f || pressure < 300.0f || pressure > 1100.0f ||
            humidity < 0.0f || humidity > 100.0f)
            return false;
        temperatureC = temp;
        pressureHpa = pressure;
        humidityPct = humidity;
        return true;
    }

private:
    struct Calibration {
        uint16_t t1; int16_t t2, t3;
        uint16_t p1; int16_t p2, p3, p4, p5, p6, p7, p8, p9;
        uint8_t h1, h3; int16_t h2, h4, h5; int8_t h6;
    } _cal{};
    uint8_t _address = 0;
    bool _ready = false;
    InitFailure _lastFailure = InitFailure::None;

    void setTransportFailure(uint8_t address) {
        I2CBus::Result result = I2CBus::getInstance().getAddressStats(address).lastResult;
        if (result == I2CBus::Result::Timeout) _lastFailure = InitFailure::Timeout;
        else if (result == I2CBus::Result::Busy) _lastFailure = InitFailure::BusBusy;
    }

    static uint16_t u16le(const uint8_t *p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
    static int16_t s16le(const uint8_t *p) { return (int16_t)u16le(p); }

    bool read(uint8_t address, uint8_t reg, uint8_t *data, size_t len) const {
        return I2CBus::getInstance().writeThenRead(address, &reg, 1, data, len, false);
    }

    bool write(uint8_t address, uint8_t reg, uint8_t value) const {
        uint8_t data[] = {reg, value};
        return I2CBus::getInstance().writeBytes(address, data, sizeof(data), false);
    }

    bool loadCalibration(uint8_t address) {
        uint8_t first[26] = {};
        uint8_t second[7] = {};
        if (!read(address, 0x88, first, sizeof(first)) || !read(address, 0xE1, second, sizeof(second)))
            return false;
        _cal.t1 = u16le(first + 0); _cal.t2 = s16le(first + 2); _cal.t3 = s16le(first + 4);
        _cal.p1 = u16le(first + 6); _cal.p2 = s16le(first + 8); _cal.p3 = s16le(first + 10);
        _cal.p4 = s16le(first + 12); _cal.p5 = s16le(first + 14); _cal.p6 = s16le(first + 16);
        _cal.p7 = s16le(first + 18); _cal.p8 = s16le(first + 20); _cal.p9 = s16le(first + 22);
        _cal.h1 = first[25]; _cal.h2 = s16le(second + 0); _cal.h3 = second[2];
        _cal.h4 = (int16_t)(((int16_t)(int8_t)second[3] << 4) | (second[4] & 0x0F));
        _cal.h5 = (int16_t)(((int16_t)(int8_t)second[5] << 4) | (second[4] >> 4));
        _cal.h6 = (int8_t)second[6];
        return _cal.t1 != 0 && _cal.p1 != 0;
    }

    bool configure(uint8_t address) const {
        // Misma configuración por defecto de Adafruit: normal, x16 en T/P/H,
        // filtro apagado y standby 0.5 ms.
        if (!write(address, 0xF4, 0x00)) return false;
        if (!write(address, 0xF2, 0x05)) return false;
        if (!write(address, 0xF5, 0x00)) return false;
        return write(address, 0xF4, 0xB7);
    }

    float compensateTemperature(int32_t adcT, int32_t &fine) const {
        int32_t var1 = (((adcT >> 3) - ((int32_t)_cal.t1 << 1)) * (int32_t)_cal.t2) >> 11;
        int32_t var2 = (((((adcT >> 4) - (int32_t)_cal.t1) * ((adcT >> 4) - (int32_t)_cal.t1)) >> 12) * (int32_t)_cal.t3) >> 14;
        fine = var1 + var2;
        return (float)((fine * 5 + 128) >> 8) / 100.0f;
    }

    float compensatePressure(int32_t adcP, int32_t fine) const {
        int64_t var1 = (int64_t)fine - 128000;
        int64_t var2 = var1 * var1 * _cal.p6;
        var2 += (var1 * _cal.p5) << 17;
        var2 += ((int64_t)_cal.p4) << 35;
        var1 = ((var1 * var1 * _cal.p3) >> 8) + ((var1 * _cal.p2) << 12);
        var1 = (((((int64_t)1) << 47) + var1) * _cal.p1) >> 33;
        if (var1 == 0) return NAN;
        int64_t pressure = 1048576 - adcP;
        pressure = (((pressure << 31) - var2) * 3125) / var1;
        var1 = (_cal.p9 * (pressure >> 13) * (pressure >> 13)) >> 25;
        var2 = (_cal.p8 * pressure) >> 19;
        pressure = ((pressure + var1 + var2) >> 8) + (((int64_t)_cal.p7) << 4);
        return (float)pressure / 25600.0f;
    }

    float compensateHumidity(int32_t adcH, int32_t fine) const {
        int32_t v = fine - 76800;
        v = (((((adcH << 14) - ((int32_t)_cal.h4 << 20) - ((int32_t)_cal.h5 * v)) + 16384) >> 15) *
             (((((((v * (int32_t)_cal.h6) >> 10) * (((v * (int32_t)_cal.h3) >> 11) + 32768)) >> 10) + 2097152) *
                 (int32_t)_cal.h2 + 8192) >> 14));
        v -= (((((v >> 15) * (v >> 15)) >> 7) * (int32_t)_cal.h1) >> 4);
        v = v < 0 ? 0 : v;
        v = v > 419430400 ? 419430400 : v;
        return (float)(v >> 12) / 1024.0f;
    }
};

#endif
