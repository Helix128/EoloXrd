#ifndef EOLO_SYSTEM_STORAGE_DIAG_H
#define EOLO_SYSTEM_STORAGE_DIAG_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <nvs.h>

// El llamador inicializa Serial y alimenta los periféricos antes de ejecutar.
// Consultar NVS directamente evita que Preferences imprima como error un
// namespace ausente: es normal antes de guardar preferencias o calibración.
namespace StorageDiag {

inline void printBlob(nvs_handle_t handle, const char *key)
{
    size_t length = 0;
    const esp_err_t result = nvs_get_blob(handle, key, nullptr, &length);
    if (result == ESP_OK)
        Serial.printf("NVS clave=%s estado=presente bytes=%u contenido=no_validado\n", key, static_cast<unsigned>(length));
    else if (result == ESP_ERR_NVS_NOT_FOUND)
        Serial.printf("NVS clave=%s estado=ausente\n", key);
    else
        Serial.printf("NVS clave=%s estado=error codigo=%s\n", key, esp_err_to_name(result));
}

inline void printU8(nvs_handle_t handle, const char *key)
{
    uint8_t value = 0;
    const esp_err_t result = nvs_get_u8(handle, key, &value);
    if (result == ESP_OK)
        Serial.printf("NVS clave=%s estado=presente valor=%u\n", key, static_cast<unsigned>(value));
    else if (result == ESP_ERR_NVS_NOT_FOUND)
        Serial.printf("NVS clave=%s estado=ausente\n", key);
    else
        Serial.printf("NVS clave=%s estado=error codigo=%s\n", key, esp_err_to_name(result));
}

inline void printU32(nvs_handle_t handle, const char *key)
{
    uint32_t value = 0;
    size_t length = sizeof(value);
    // DisplayPowerConfig usa SettingsStore::putUInt, que guarda un blob.
    const esp_err_t result = nvs_get_blob(handle, key, &value, &length);
    if (result == ESP_OK && length == sizeof(value))
        Serial.printf("NVS clave=%s estado=presente valor=%lu\n", key, static_cast<unsigned long>(value));
    else if (result == ESP_ERR_NVS_NOT_FOUND)
        Serial.printf("NVS clave=%s estado=ausente\n", key);
    else if (result == ESP_OK)
        Serial.printf("NVS clave=%s estado=error tamano=%u esperado=%u\n", key,
                      static_cast<unsigned>(length), static_cast<unsigned>(sizeof(value)));
    else
        Serial.printf("NVS clave=%s estado=error codigo=%s\n", key, esp_err_to_name(result));
}

inline void checkNamespace(const char *name, bool calibration)
{
    nvs_handle_t handle;
    const esp_err_t result = nvs_open(name, NVS_READONLY, &handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        Serial.printf("NVS namespace=%s estado=ausente\n", name);
        return;
    }
    if (result != ESP_OK) {
        Serial.printf("NVS namespace=%s estado=error codigo=%s\n", name, esp_err_to_name(result));
        return;
    }

    Serial.printf("NVS namespace=%s estado=presente\n", name);
    if (calibration) {
        printBlob(handle, "slot0");
        printBlob(handle, "slot1");
        printBlob(handle, "pumpOrder");
        printU8(handle, "calVersion");
    } else {
        printU32(handle, "bright");
        printU32(handle, "dimS");
        printU32(handle, "offS");
    }
    nvs_close(handle);
}

inline void checkSd()
{
#if EOLO_DISPLAY_SPI && !defined(FEATURE_HEADLESS)
    // La OLED SPI comparte SCK/MOSI con la SD en estos perfiles. No tocar
    // DISPLAY_CS_PIN en modelos I2C: ese GPIO puede controlar una bomba.
    if (DISPLAY_CS_PIN >= 0 && DISPLAY_CS_PIN != SD_CS_PIN) {
        digitalWrite(DISPLAY_CS_PIN, HIGH);
        pinMode(DISPLAY_CS_PIN, OUTPUT);
    }
#endif
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN);
    const uint32_t started = millis();
    const bool mounted = SD.begin(SD_CS_PIN, SPI);
    const uint32_t elapsed = millis() - started;
    if (!mounted || SD.cardType() == CARD_NONE) {
        Serial.printf("SD estado=error etapa=montaje duracion_ms=%lu\n", static_cast<unsigned long>(elapsed));
        SD.end();
        return;
    }

    File root = SD.open("/", FILE_READ);
    if (!root || !root.isDirectory()) {
        Serial.printf("SD estado=error etapa=abrir_raiz duracion_ms=%lu\n", static_cast<unsigned long>(elapsed));
        if (root)
            root.close();
        SD.end();
        return;
    }

    // Leer una entrada confirma acceso al directorio; no divulgar su nombre.
    File entry = root.openNextFile();
    const bool hasEntry = static_cast<bool>(entry);
    if (entry)
        entry.close();
    root.close();
    Serial.printf("SD estado=montada tipo=%u total_bytes=%llu usado_bytes=%llu raiz_primera_entrada=%s duracion_ms=%lu escritura=no_probada\n",
                  static_cast<unsigned>(SD.cardType()),
                  static_cast<unsigned long long>(SD.totalBytes()),
                  static_cast<unsigned long long>(SD.usedBytes()),
                  hasEntry ? "presente" : "ninguna_o_error_lectura",
                  static_cast<unsigned long>(elapsed));
    SD.end();
}

} // namespace StorageDiag

inline void diagStorage()
{
    Serial.println("=== Almacenamiento (solo lectura) ===");
    StorageDiag::checkNamespace("eolo_calib", true);
#ifndef FEATURE_HEADLESS
    StorageDiag::checkNamespace("eolo_display", false);
#endif
    StorageDiag::checkSd();
}

#endif
