#ifndef EOLO_PREFERENCES_SETTINGS_STORE_H
#define EOLO_PREFERENCES_SETTINGS_STORE_H

#include <Preferences.h>
#include <Eolo/Services/SettingsStore.h>

// Adaptador NVS para los contratos portables de SettingsStore.  Cada
// operación abre/cierra el namespace para que el objeto pueda ser compartido
// por servicios header-only y no deje una sesión Preferences abierta entre
// tareas. Las claves y namespaces son responsabilidad del consumidor y, por
// tanto, permanecen compatibles con el firmware existente.
class PreferencesSettingsStore final : public SettingsStore
{
    Preferences _preferences;
    const char *_namespace;
    bool _readOnly;

    bool open(bool readOnly)
    {
        return _namespace != nullptr && _preferences.begin(_namespace, readOnly);
    }

public:
    explicit PreferencesSettingsStore(const char *name, bool readOnly = false)
        : _namespace(name), _readOnly(readOnly)
    {
    }

    ~PreferencesSettingsStore() override
    {
        _preferences.end();
    }

    bool getBytes(const char *key, void *data, size_t len) override
    {
        if (key == nullptr || data == nullptr || !open(true))
            return false;
        const bool ok = _preferences.getBytes(key, data, len) == len;
        _preferences.end();
        return ok;
    }

    bool putBytes(const char *key, const void *data, size_t len) override
    {
        if (_readOnly || key == nullptr || data == nullptr || !open(false))
            return false;
        const bool ok = _preferences.putBytes(key, data, len) == len;
        _preferences.end();
        return ok;
    }

    bool getBool(const char *key, bool defaultValue) override
    {
        if (key == nullptr || !open(true))
            return defaultValue;
        const bool result = _preferences.getBool(key, defaultValue);
        _preferences.end();
        return result;
    }

    bool putBool(const char *key, bool value) override
    {
        if (_readOnly || key == nullptr || !open(false))
            return false;
        const bool ok = _preferences.putBool(key, value) == 1;
        _preferences.end();
        return ok;
    }

    bool contains(const char *key) override
    {
        if (key == nullptr || !open(true))
            return false;
        const bool result = _preferences.isKey(key);
        _preferences.end();
        return result;
    }

    void remove(const char *key) override
    {
        if (_readOnly || key == nullptr || !open(false))
            return;
        _preferences.remove(key);
        _preferences.end();
    }

    void clear() override
    {
        if (_readOnly || !open(false))
            return;
        _preferences.clear();
        _preferences.end();
    }
};

#endif
