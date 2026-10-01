#ifndef EOLO_CORE_DEBUG_CONSOLE_ARGS_H
#define EOLO_CORE_DEBUG_CONSOLE_ARGS_H

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Eolo/Core/Flow/FlowMotorController.h>

// Interpretación de comandos de consola "nombre=valor" y formato de duraciones,
// independiente de Arduino.
namespace ConsoleArgs
{
    namespace detail
    {
        // Devuelve el inicio del valor de `name=` en `args`, o nullptr si falta.
        inline const char *valueOf(const char *args, const char *name)
        {
            if (args == nullptr || name == nullptr) return nullptr;
            char key[32];
            int written = snprintf(key, sizeof(key), "%s=", name);
            if (written < 0 || written >= (int)sizeof(key)) return nullptr;
            const char *pos = strstr(args, key);
            return pos == nullptr ? nullptr : pos + written;
        }

        // El valor termina en el primer espacio: `name= x` es un valor vacío
        // (0) y no debe leer el argumento siguiente. Devuelve nullptr si el
        // valor está vacío.
        inline const char *numberStart(const char *value)
        {
            while (*value != '\0' && *value != ' ' && isspace((unsigned char)*value)) ++value;
            return (*value == '\0' || *value == ' ') ? nullptr : value;
        }
    }

    inline bool readInt(const char *args, const char *name, int &value)
    {
        const char *start = detail::valueOf(args, name);
        if (start == nullptr) return false;
        const char *number = detail::numberStart(start);
        value = number == nullptr ? 0 : (int)strtol(number, nullptr, 10);
        return true;
    }

    inline bool readFloat(const char *args, const char *name, float &value)
    {
        const char *start = detail::valueOf(args, name);
        if (start == nullptr) return false;
        const char *number = detail::numberStart(start);
        value = number == nullptr ? 0.0f : (float)strtod(number, nullptr);
        return true;
    }

    // Aplica sobre `current` los parámetros PID presentes en `args`
    // ("kp=35 ki=1 interval=200 ...").
    inline FlowPidConfig parsePidConfig(const char *args, const FlowPidConfig &current)
    {
        FlowPidConfig config = current;
        int intValue = 0;
        float floatValue = 0.0f;
        if (readInt(args, "interval", intValue)) config.intervalMs = intValue;
        if (readInt(args, "maxStep", intValue)) config.maxStep = intValue;
        if (readInt(args, "maxDt", intValue)) config.maxDtMs = intValue;
        if (readInt(args, "stale", intValue)) config.sensorStaleMs = intValue;
        if (readFloat(args, "deadband", floatValue)) config.deadband = floatValue;
        if (readFloat(args, "kp", floatValue)) config.kp = floatValue;
        if (readFloat(args, "ki", floatValue)) config.ki = floatValue;
        if (readFloat(args, "kd", floatValue)) config.kd = floatValue;
        if (readFloat(args, "ilim", floatValue)) config.integralLimit = floatValue;
        if (readFloat(args, "alpha", floatValue)) config.filterAlpha = floatValue;
        if (readFloat(args, "minActive", floatValue)) config.minActive = floatValue;
        if (readInt(args, "kick", intValue)) config.kickPwm = intValue;
        if (readInt(args, "kickMs", intValue)) config.kickMs = (uint32_t)intValue;
        if (readFloat(args, "stallFlow", floatValue)) config.stallFlowLpm = floatValue;
        if (readInt(args, "cooldown", intValue)) config.restallCooldownMs = (uint32_t)intValue;
        if (readInt(args, "stallConfirm", intValue)) config.stallConfirmMs = (uint32_t)intValue;
        if (readInt(args, "trimMax", intValue)) config.softTrimMax = intValue;
        if (readInt(args, "softStep", intValue)) config.softMaxStep = intValue;
        if (readFloat(args, "sens", floatValue)) config.sensitivity = floatValue;
        if (readInt(args, "recenter", intValue)) config.recenterDelayMs = (uint32_t)intValue;
        if (readInt(args, "faultStop", intValue)) config.sensorFaultStopMs = (uint32_t)intValue;
        if (readInt(args, "zeroConfirm", intValue)) config.zeroFlowConfirmSamples = (uint8_t)intValue;
        return config;
    }

    // "2 h 05 min", "3 min 07 s" o "42 s". `infinite` (p. ej. UINT32_MAX) se
    // escribe como "infinita".
    inline void formatDuration(uint32_t seconds, uint32_t infinite, char *out, size_t outSize)
    {
        if (seconds == infinite)
        {
            snprintf(out, outSize, "infinita");
            return;
        }

        uint32_t hours = seconds / 3600;
        uint32_t minutes = (seconds % 3600) / 60;
        uint32_t secs = seconds % 60;

        if (hours > 0)
            snprintf(out, outSize, "%lu h %02lu min", (unsigned long)hours, (unsigned long)minutes);
        else if (minutes > 0)
            snprintf(out, outSize, "%lu min %02lu s", (unsigned long)minutes, (unsigned long)secs);
        else
            snprintf(out, outSize, "%lu s", (unsigned long)secs);
    }
}

#endif
