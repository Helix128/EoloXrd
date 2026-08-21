#ifndef EOLO_CORE_COMMUNICATION_GNSS_PARSER_H
#define EOLO_CORE_COMMUNICATION_GNSS_PARSER_H

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct GnssData
{
    float latitude = -1.0f;
    float longitude = -1.0f;
    float speedKmh = -1.0f;
    float satellites = -1.0f;
    bool valid = false;
};

// Parser independiente de Arduino para la respuesta SIMCom AT+CGNSSINFO.
// Las revisiones del modem añaden campos al final; se usan los índices
// documentados y se acepta un BeiDou adicional cuando esté presente.
class GnssParser
{
public:
    static bool parse(const char *response, GnssData &out)
    {
        out = GnssData{};
        if (response == nullptr) return false;
        const char *prefix = strstr(response, "+CGNSSINFO:");
        if (prefix == nullptr) return false;
        prefix += strlen("+CGNSSINFO:");
        while (*prefix == ' ' || *prefix == '\t') ++prefix;

        char fields[24][24] = {};
        size_t count = split(prefix, fields, 24);
        // run status, fix status, UTC, latitude, N/S, longitude, E/W, ...
        if (count < 11 || !hasFix(fields[1]) || !hasFixMode(fields[10])) return false;

        float latitude = coordinate(fields[3], fields[4], 'N', 'S');
        float longitude = coordinate(fields[5], fields[6], 'E', 'W');
        float knots = number(fields[8]);
        if (!isfinite(latitude) || !isfinite(longitude) || !isfinite(knots) || knots < 0.0f)
            return false;

        int satellites = 0;
        bool hasSatellites = false;
        // SIM7600: GPS in view=16 y GLONASS in view=18. Algunas revisiones
        // exponen BeiDou inmediatamente después; sumarlo sólo si existe.
        addSatellite(fields, count, 16, satellites, hasSatellites);
        addSatellite(fields, count, 18, satellites, hasSatellites);
        addSatellite(fields, count, 19, satellites, hasSatellites);

        out.latitude = latitude;
        out.longitude = longitude;
        out.speedKmh = knots * 1.852f;
        out.satellites = hasSatellites ? (float)satellites : -1.0f;
        out.valid = true;
        return true;
    }

private:
    static size_t split(const char *source, char fields[][24], size_t capacity)
    {
        size_t field = 0, pos = 0;
        for (const char *p = source; *p != '\0' && *p != '\r' && *p != '\n'; ++p) {
            if (*p == ',') {
                if (++field >= capacity) break;
                pos = 0;
            } else if (pos + 1 < 24) {
                fields[field][pos++] = *p;
                fields[field][pos] = '\0';
            }
        }
        return field + 1;
    }

    static bool hasFix(const char *value) { return value[0] == '1' || value[0] == '2'; }
    static bool hasFixMode(const char *value) { return value[0] == '2' || value[0] == '3'; }

    static float number(const char *value)
    {
        if (value == nullptr || value[0] == '\0') return NAN;
        char *end = nullptr;
        float result = strtof(value, &end);
        return end != value && *end == '\0' ? result : NAN;
    }

    static float coordinate(const char *value, const char *hemisphere, char positive, char negative)
    {
        float ddmm = number(value);
        if (!isfinite(ddmm) || hemisphere == nullptr || hemisphere[1] != '\0') return NAN;
        int degrees = (int)(ddmm / 100.0f);
        float minutes = ddmm - (degrees * 100.0f);
        if (minutes < 0.0f || minutes >= 60.0f) return NAN;
        float decimal = degrees + minutes / 60.0f;
        if (hemisphere[0] == positive) return decimal;
        if (hemisphere[0] == negative) return -decimal;
        return NAN;
    }

    static void addSatellite(char fields[][24], size_t count, size_t index, int &total, bool &known)
    {
        if (index >= count || fields[index][0] == '\0') return;
        float value = number(fields[index]);
        if (!isfinite(value) || value < 0.0f || value != floorf(value)) return;
        total += (int)value;
        known = true;
    }
};

#endif
