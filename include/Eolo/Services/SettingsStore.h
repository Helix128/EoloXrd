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
    virtual void remove(const char *key) = 0;
};

#endif
