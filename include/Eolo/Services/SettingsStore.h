#ifndef EOLO_SERVICES_SETTINGS_STORE_H
#define EOLO_SERVICES_SETTINGS_STORE_H

#include <stddef.h>
#include <stdint.h>

class SettingsStore
{
public:
    virtual ~SettingsStore() = default;
    virtual bool getBytes(const char *key, void *data, size_t len) = 0;
    virtual bool putBytes(const char *key, const void *data, size_t len) = 0;
    virtual bool getBool(const char *key, bool defaultValue) = 0;
    virtual bool putBool(const char *key, bool value) = 0;
    virtual bool contains(const char *key)
    {
        (void)key;
        return false;
    }
    virtual void remove(const char *key) = 0;
    virtual void clear() {}

    // Conveniencias portables para los servicios que almacenan configuración
    // tipada. Se implementan sobre bytes para que una implementación nativa de
    // tests no tenga que conocer Preferences ni el formato NVS.
    uint32_t getUInt(const char *key, uint32_t defaultValue)
    {
        uint32_t value = defaultValue;
        return getBytes(key, &value, sizeof(value)) ? value : defaultValue;
    }

    float getFloat(const char *key, float defaultValue)
    {
        float value = defaultValue;
        return getBytes(key, &value, sizeof(value)) ? value : defaultValue;
    }

    bool putUInt(const char *key, uint32_t value)
    {
        return putBytes(key, &value, sizeof(value));
    }

    bool putFloat(const char *key, float value)
    {
        return putBytes(key, &value, sizeof(value));
    }
};

#endif
