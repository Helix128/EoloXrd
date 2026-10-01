#ifndef EOLO_CORE_COMMUNICATION_SIGNAL_QUALITY_H
#define EOLO_CORE_COMMUNICATION_SIGNAL_QUALITY_H

#include <stdint.h>
#include <string.h>

// Calidad de señal celular a partir de la respuesta AT+CSQ, independiente de
// Arduino. CSQ 0-31 (GSM) y 100-191 (TD-SCDMA/LTE extendido) se convierten a
// dBm; 99 y 199 significan "desconocido".
namespace SignalQuality
{
    // Extrae <rssi> de "+CSQ: <rssi>,<ber>". Devuelve -1 si la respuesta no es
    // válida; `berOut` (opcional) recibe <ber> o 99 si falta.
    inline int parseCsq(const char *response, int *berOut = nullptr)
    {
        if (response == nullptr) return -1;

        const char *pos = strstr(response, "+CSQ:");
        if (pos == nullptr) return -1;
        pos += 5;

        while (*pos == ' ' || *pos == '\t') pos++;
        if (*pos < '0' || *pos > '9') return -1;

        int value = 0;
        while (*pos >= '0' && *pos <= '9')
        {
          value = value * 10 + (*pos - '0');
          pos++;
        }

        if (value < 0 || value > 199) return -1;

        while (*pos == ' ' || *pos == '\t') pos++;
        if (*pos == ',')
        {
          pos++;
          while (*pos == ' ' || *pos == '\t') pos++;
          int ber = 0;
          if (*pos >= '0' && *pos <= '9')
          {
            while (*pos >= '0' && *pos <= '9')
            {
              ber = ber * 10 + (*pos - '0');
              pos++;
            }
            if (berOut != nullptr) *berOut = ber;
          }
        }

        return value;
    }

    // Barras de señal (0-4) a partir del CSQ y de si el valor es conocido.
    inline uint8_t barsFromCsq(uint8_t csq, bool known)
    {
        if (!known || csq == 99 || csq == 199) return 0;

        int dbm = -120;
        if (csq <= 31)
        {
          dbm = -113 + (2 * (int)csq);
        }
        else if (csq >= 100 && csq <= 191)
        {
          dbm = (int)csq - 216;
        }
        else
        {
          return 0;
        }

        if (dbm >= -73) return 4;
        if (dbm >= -85) return 3;
        if (dbm >= -97) return 2;
        if (dbm >= -109) return 1;
        return 0;
    }
}

#endif
