#ifndef EOLO_CORE_COMMUNICATION_HTTP_URL_H
#define EOLO_CORE_COMMUNICATION_HTTP_URL_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>

// Utilidades de URL HTTP independientes de Arduino. Los mensajes de error se
// devuelven por `error` (cadena estática, nullptr si la falla no tiene mensaje)
// para que el llamador decida cómo registrarlos.
namespace HttpUrl
{
    namespace detail
    {
        // Devuelve el inicio del host, o nullptr si el esquema no es http(s).
        inline const char *hostStart(const char *url)
        {
            if (strncmp(url, "http://", 7) == 0) return url + 7;
            if (strncmp(url, "https://", 8) == 0) return url + 8;
            return nullptr;
        }

        inline bool parseOctet(const char *text, int start, int end, int &value)
        {
            if (start >= end || end - start > 3) return false;
            value = 0;
            for (int i = start; i < end; ++i)
            {
                char c = text[i];
                if (c < '0' || c > '9') return false;
                value = value * 10 + (c - '0');
            }
            return value >= 0 && value <= 255;
        }
    }

    // Exige http:// o https:// con host y garantiza una ruta ("/" si falta).
    inline bool normalize(const char *input, char *output, size_t outputSize, const char *&error)
    {
        error = nullptr;
        if (input == nullptr || output == nullptr || outputSize == 0)
        {
            error = "URL HTTP inválida";
            return false;
        }

        const char *host = detail::hostStart(input);
        if (host == nullptr)
        {
            error = "URL debe iniciar con http:// o https://";
            return false;
        }
        if (host[0] == '\0' || host[0] == '/')
        {
            error = "URL sin host";
            return false;
        }

        const char *format = strchr(host, '/') != nullptr ? "%s" : "%s/";
        int written = snprintf(output, outputSize, format, input);
        if (written < 0 || written >= (int)outputSize)
        {
            error = "URL demasiado larga";
            return false;
        }
        return true;
    }

    // Copia el host (sin puerto ni ruta) de una URL http(s).
    inline bool extractHost(const char *url, char *host, size_t hostSize)
    {
        if (url == nullptr || host == nullptr || hostSize == 0) return false;

        const char *hostStart = detail::hostStart(url);
        if (hostStart == nullptr) return false;

        const char *hostEnd = hostStart;
        while (*hostEnd != '\0' && *hostEnd != '/' && *hostEnd != ':') hostEnd++;
        size_t len = hostEnd - hostStart;
        if (len == 0 || len >= hostSize) return false;

        memcpy(host, hostStart, len);
        host[len] = '\0';
        return true;
    }

    // Reemplaza el host de una URL http:// por una IP conservando la ruta.
    inline bool withHostIp(const char *url, const char *ip, char *output, size_t outputSize,
                           const char *&error)
    {
        error = nullptr;
        if (url == nullptr || ip == nullptr || output == nullptr || outputSize == 0) return false;
        if (strncmp(url, "http://", 7) != 0) return false;

        const char *pathStart = strchr(url + 7, '/');
        if (pathStart == nullptr) pathStart = "/";

        int written = snprintf(output, outputSize, "http://%s%s", ip, pathStart);
        if (written < 0 || written >= (int)outputSize)
        {
            error = "URL IP demasiado larga";
            return false;
        }
        return true;
    }

    // Verdadero si todo `text` es una dirección IPv4 decimal con cuatro octetos.
    inline bool isValidIPv4(const char *text)
    {
        if (text == nullptr) return false;
        const int length = (int)strlen(text);
        int start = 0;
        for (int octet = 0; octet < 4; ++octet)
        {
            int end = length;
            if (octet != 3)
            {
                const char *dot = strchr(text + start, '.');
                if (dot == nullptr) return false;
                end = (int)(dot - text);
            }
            int value = 0;
            if (!detail::parseOctet(text, start, end, value)) return false;
            start = end + 1;
        }
        return start == length + 1;
    }
}

#endif
