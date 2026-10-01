#ifndef EOLO_CORE_COMMUNICATION_AT_RESPONSE_H
#define EOLO_CORE_COMMUNICATION_AT_RESPONSE_H

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "HttpUrl.h"

// Interpretación de respuestas AT de módems SIMCom, independiente de Arduino.
namespace AtResponse
{
    // Verdadero si la respuesta de AT+CREG?/CGREG?/CEREG? indica registrado en
    // red local (1) o en roaming (5). Con formato "+CREG: n,stat" usa `stat`;
    // con "+CREG: stat" usa el único campo.
    inline bool isRegistered(const char *response)
    {
        if (response == nullptr) return false;
        const char *colon = strchr(response, ':');
        if (colon == nullptr || strstr(response, "ERROR") != nullptr) return false;

        const char *value = colon + 1;
        while (isspace((unsigned char)*value)) ++value;
        const char *comma = strchr(value, ',');
        long stat = strtol(comma == nullptr ? value : comma + 1, nullptr, 10);
        return stat == 1 || stat == 5;
    }

    // Verdadero si la respuesta de AT+IPADDR contiene una IPv4 completa
    // (cuatro octetos válidos que no continúan con dígito o punto).
    inline bool hasValidIp(const char *response)
    {
        if (response == nullptr || strstr(response, "ERROR") != nullptr) return false;

        const int length = (int)strlen(response);
        for (int i = 0; i < length; ++i)
        {
            if (!isdigit((unsigned char)response[i])) continue;

            int pos = i;
            bool ok = true;
            for (int octet = 0; octet < 4; ++octet)
            {
                int start = pos;
                while (pos < length && isdigit((unsigned char)response[pos])) pos++;
                int value = 0;
                if (!HttpUrl::detail::parseOctet(response, start, pos, value))
                {
                    ok = false;
                    break;
                }
                if (octet < 3)
                {
                    if (pos >= length || response[pos] != '.')
                    {
                        ok = false;
                        break;
                    }
                    pos++;
                }
            }

            if (ok)
            {
                char next = pos < length ? response[pos] : '\0';
                if (!isdigit((unsigned char)next) && next != '.') return true;
            }
        }
        return false;
    }

    // Texto del código de error de +HTTPACTION (600-717) o "" si es desconocido.
    inline const char *httpActionStatusText(int status)
    {
        switch (status)
        {
        case 600: return "Not HTTP PDU";
        case 601: return "Network Error";
        case 602: return "No memory";
        case 603: return "DNS Error";
        case 604: return "Stack Busy";
        case 701: return "Alert state";
        case 702: return "Unknown error";
        case 703: return "Busy";
        case 704: return "Connection closed";
        case 705: return "Timeout";
        case 706: return "Socket send/recv failed";
        case 707: return "File/memory error";
        case 708: return "Invalid parameter";
        case 709: return "Network error";
        case 710: return "SSL session failed";
        case 711: return "Wrong state";
        case 712: return "Create socket failed";
        case 713: return "Get DNS failed";
        case 714: return "Connect socket failed";
        case 715: return "Handshake failed";
        case 716: return "Close socket failed";
        case 717: return "No network";
        default: return "";
        }
    }
}

#endif
