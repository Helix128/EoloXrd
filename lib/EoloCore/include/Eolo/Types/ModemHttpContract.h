#ifndef EOLO_TYPES_MODEM_HTTP_CONTRACT_H
#define EOLO_TYPES_MODEM_HTTP_CONTRACT_H

#include <stddef.h>

// Contrato de texto para solicitudes HTTP encoladas. El límite de URL deja
// espacio para la barra que Modem agrega al normalizar una URL sin ruta y para
// el comando AT+HTTPPARA que la transporta.
namespace ModemHttpContract
{
inline constexpr size_t kHttpAtCommandBufferBytes = 512;
inline constexpr size_t kHttpUrlAtCommandOverheadChars =
    (sizeof("AT+HTTPPARA=\"URL\",\"") - 1) + (sizeof("\"") - 1);
inline constexpr size_t kHttpUrlNormalizedMaxChars =
    kHttpAtCommandBufferBytes - 1 - kHttpUrlAtCommandOverheadChars;
inline constexpr size_t kHttpUrlMaxChars = kHttpUrlNormalizedMaxChars - 1;
inline constexpr size_t kHttpUrlStorageBytes = kHttpUrlNormalizedMaxChars + 1;

inline constexpr size_t kHttpPayloadStorageBytes = 2048;
inline constexpr size_t kHttpPayloadMaxChars = kHttpPayloadStorageBytes - 1;

inline bool textFits(const char *text, size_t maxChars)
{
    if (text == nullptr) return true;
    for (size_t index = 0; index <= maxChars; ++index)
    {
        if (text[index] == '\0') return true;
    }
    return false;
}

// Values are interpolated in quoted AT commands.  Rejecting control bytes and
// quotes here prevents a console URL/body from injecting a second command.
inline bool isSafeAtText(const char *text)
{
    if (text == nullptr) return true;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p)
        if (*p < 0x20 || *p == 0x7f || *p == '"' || *p == '\r' || *p == '\n') return false;
    return true;
}

inline bool urlFits(const char *url)
{
    return url != nullptr && url[0] != '\0' && textFits(url, kHttpUrlMaxChars) && isSafeAtText(url);
}

inline bool payloadFits(const char *payload)
{
    // Un POST sin body conserva la semántica previa: se envía como body vacío.
    return textFits(payload, kHttpPayloadMaxChars) && isSafeAtText(payload);
}
} // namespace ModemHttpContract

#endif
