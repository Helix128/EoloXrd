#ifndef EOLO_OPTIONAL_NVS_READER_H
#define EOLO_OPTIONAL_NVS_READER_H

#include <Arduino.h>
#include <nvs.h>
#include "../Utility/Log.h"

// Los datos opcionales pueden no existir. Un fallo real de NVS debe seguir
// siendo visible y distinguirse de un namespace o clave todavía no guardado.
class OptionalNvsReader {
public:
    enum class Result { Ok, Missing, Invalid, Error };

    explicit OptionalNvsReader(const char *name) : _name(name)
    {
        if (!name) {
            _status = Result::Error;
            return;
        }
        const esp_err_t error = nvs_open(name, NVS_READONLY, &_handle);
        if (error == ESP_OK) _status = Result::Ok;
        else if (error == ESP_ERR_NVS_NOT_FOUND) _status = Result::Missing;
        else {
            _status = Result::Error;
            LOG_F("NVS: no se pudo abrir %s: %s\n", name, esp_err_to_name(error));
        }
    }

    ~OptionalNvsReader() { if (_status == Result::Ok) nvs_close(_handle); }
    OptionalNvsReader(const OptionalNvsReader &) = delete;
    OptionalNvsReader &operator=(const OptionalNvsReader &) = delete;

    Result status() const { return _status; }

    Result readBlob(const char *key, void *data, size_t size) const
    {
        if (_status != Result::Ok) return _status;
        size_t length = size;
        const esp_err_t error = nvs_get_blob(_handle, key, data, &length);
        if (error == ESP_ERR_NVS_NOT_FOUND) return Result::Missing;
        if (error == ESP_ERR_NVS_TYPE_MISMATCH || error == ESP_ERR_NVS_INVALID_LENGTH ||
            (error == ESP_OK && length != size)) {
            LOG_F("NVS: dato inválido %s/%s: %s, esperado=%u, recibido=%u\n",
                  _name, key, esp_err_to_name(error), (unsigned)size, (unsigned)length);
            return Result::Invalid;
        }
        if (error != ESP_OK || length != size) {
            LOG_F("NVS: no se pudo leer %s/%s: %s, esperado=%u, recibido=%u\n",
                  _name, key, esp_err_to_name(error), (unsigned)size, (unsigned)length);
            return Result::Error;
        }
        return Result::Ok;
    }

    Result readU8(const char *key, uint8_t &value) const
    {
        if (_status != Result::Ok) return _status;
        const esp_err_t error = nvs_get_u8(_handle, key, &value);
        if (error == ESP_ERR_NVS_NOT_FOUND) return Result::Missing;
        if (error != ESP_OK) {
            LOG_F("NVS: no se pudo leer %s/%s: %s\n", _name, key, esp_err_to_name(error));
            return Result::Error;
        }
        return Result::Ok;
    }

private:
    const char *_name;
    nvs_handle_t _handle = 0;
    Result _status = Result::Missing;
};

#endif
