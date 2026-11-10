import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class OptionalNvsReaderTest(unittest.TestCase):
    def test_missing_values_are_silent_but_real_errors_are_reported(self):
        compiler = shutil.which("g++")
        if not compiler:
            self.skipTest("g++ no disponible")

        with tempfile.TemporaryDirectory() as temp:
            tmp = Path(temp)
            (tmp / "Arduino.h").write_text(r"""
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>
struct FakeSerial {
    std::string output;
    void printf(const char* format, ...) {
        char buffer[512]; va_list args; va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args); va_end(args); output += buffer;
    }
    void println(const char* text) { output += text; output += '\n'; }
};
inline FakeSerial Serial;
inline uint32_t millis() { return 0; }
""")
            (tmp / "SPI.h").write_text(r"""
#pragma once
struct FakeSPI { void begin(int, int, int) {} };
inline FakeSPI SPI;
""")
            (tmp / "SD.h").write_text(r"""
#pragma once
#include <stdint.h>
#include "SPI.h"
constexpr int CARD_NONE = 0, FILE_READ = 0;
struct File {
    operator bool() const { return true; }
    bool isDirectory() const { return true; }
    File openNextFile() { return {}; }
    void close() {}
};
struct FakeSD {
    bool begin(int, FakeSPI&) { return true; }
    int cardType() { return 1; }
    File open(const char*, int) { return {}; }
    uint64_t totalBytes() { return 0; }
    uint64_t usedBytes() { return 0; }
    void end() {}
};
inline FakeSD SD;
""")
            (tmp / "nvs.h").write_text(r"""
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
using esp_err_t = int;
using nvs_handle_t = unsigned;
constexpr int ESP_OK = 0, ESP_ERR_NVS_NOT_FOUND = 1, FAILURE = 2;
constexpr int ESP_ERR_NVS_TYPE_MISMATCH = 3, ESP_ERR_NVS_INVALID_LENGTH = 4;
constexpr int NVS_READONLY = 0;
inline int openResult = ESP_OK, blobResult = ESP_OK, byteResult = ESP_OK;
inline int closeCount = 0;
inline uint32_t blobValue = 0;
inline uint8_t byteValue = 0;
inline size_t blobLength = sizeof(blobValue);
inline esp_err_t nvs_open(const char*, int, nvs_handle_t* h) { *h = 1; return openResult; }
inline void nvs_close(nvs_handle_t) { ++closeCount; }
inline esp_err_t nvs_get_blob(nvs_handle_t, const char*, void* out, size_t* len) {
    if (blobResult != ESP_OK) return blobResult;
    if (*len < blobLength) return FAILURE;
    memcpy(out, &blobValue, blobLength); *len = blobLength; return ESP_OK;
}
inline esp_err_t nvs_get_u8(nvs_handle_t, const char*, uint8_t* out) {
    if (byteResult != ESP_OK) return byteResult;
    *out = byteValue; return ESP_OK;
}
inline const char* esp_err_to_name(esp_err_t) { return "fake"; }
""")
            (tmp / "Preferences.h").write_text(r"""
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <nvs.h>
class Preferences {
public:
    bool begin(const char*, bool) { return true; }
    void end() {}
    size_t putBytes(const char*, const void* data, size_t n) {
        if (n != sizeof(blobValue)) return 0;
        memcpy(&blobValue, data, n); blobLength = n; blobResult = ESP_OK; return n;
    }
    size_t putBool(const char*, bool) { return 1; }
    bool isKey(const char*) { return true; }
    void remove(const char*) {}
    void clear() {}
};
""")
            (tmp / "test.cpp").write_text(r"""
#include <cassert>
#include <nvs.h>
static int logs = 0;
#define EOLO_UTILITY_LOG_H
#define LOG_F(...) (++logs)
#include "src/Common/Data/PreferencesSettingsStore.h"
#define EOLO_DISPLAY_SPI 0
#define FEATURE_HEADLESS
#define SD_SCK_PIN 18
#define SD_MISO_PIN 19
#define SD_MOSI_PIN 23
#define SD_CS_PIN 5
#include "demos/SystemDiag/StorageDiag.h"

int main() {
    PreferencesSettingsStore settings("eolo_display");
    uint32_t value = 999;
    openResult = ESP_ERR_NVS_NOT_FOUND;
    assert(!settings.getBytes("bright", &value, sizeof(value)));
    assert(settings.getUInt("bright", 100) == 100);
    assert(settings.getBool("enabled", true));
    assert(logs == 0 && closeCount == 0);

    openResult = FAILURE;
    assert(!settings.getBytes("bright", &value, sizeof(value)));
    assert(logs == 1);

    openResult = ESP_OK;
    blobResult = ESP_ERR_NVS_NOT_FOUND;
    assert(!settings.getBytes("bright", &value, sizeof(value)));
    assert(logs == 1 && closeCount == 1);

    blobResult = ESP_OK;
    blobValue = 0;
    assert(settings.getBytes("bright", &value, sizeof(value)) && value == 0);
    assert(!settings.getBool("enabled", true));

    blobResult = FAILURE;
    assert(!settings.getBytes("bright", &value, sizeof(value)));
    assert(logs == 2);
    {
        OptionalNvsReader reader("eolo_calib");
        assert(reader.readBlob("slot0", &value, sizeof(value)) == OptionalNvsReader::Result::Error);
    }
    assert(logs == 3);
    blobResult = ESP_OK;
    blobLength = 2;
    assert(!settings.getBytes("bright", &value, sizeof(value)));
    assert(logs == 4);
    {
        OptionalNvsReader reader("eolo_calib");
        assert(reader.readBlob("slot0", &value, sizeof(value)) == OptionalNvsReader::Result::Invalid);
    }
    assert(logs == 5);

    byteResult = FAILURE;
    assert(settings.getBool("enabled", true));
    assert(logs == 6);
    assert(settings.putUInt("bright", 42));

    // La demo debe leer el mismo blob que escribe SettingsStore::putUInt.
    Serial.output.clear();
    StorageDiag::printU32(1, "bright");
    assert(Serial.output.find("estado=presente valor=42") != std::string::npos);
    blobLength = 2;
    Serial.output.clear();
    StorageDiag::printU32(1, "bright");
    assert(Serial.output.find("estado=error tamano=2") != std::string::npos);
}
""")
            binary = tmp / "test"
            subprocess.run([compiler, "-std=c++17", "-I", str(tmp), "-I", str(ROOT),
                            "-I", str(ROOT / "include"), str(tmp / "test.cpp"),
                            "-o", str(binary)], check=True, capture_output=True, text=True)
            subprocess.run([str(binary)], check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
