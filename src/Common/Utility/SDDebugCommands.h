#ifndef SD_DEBUG_COMMANDS_H
#define SD_DEBUG_COMMANDS_H

#include <Arduino.h>
#include <SD.h>
#include "ConsoleInput.h"
#include "DebugCommandRouter.h"

class SDDebugCommands : public ConsoleCommandHandler {
private:
    static uint32_t updateCrc32(uint32_t crc, uint8_t byte) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i) {
            crc = (crc & 1) ? ((crc >> 1) ^ 0xEDB88320UL) : (crc >> 1);
        }
        return crc;
    }

    void listDirectory(File dir, Print& out, const String& parentPath) {
        while (true) {
            File entry = dir.openNextFile();
            if (!entry) {
                break;
            }

            String name = entry.name();
            // Limpiar prefijo slash si ya viene en name
            if (name.startsWith("/")) {
                name = name.substring(1);
            }

            String fullPath = parentPath + "/" + name;
            if (entry.isDirectory()) {
                listDirectory(entry, out, fullPath);
            } else {
                out.print("FILE ");
                out.print(entry.size());
                out.print(" ");
                out.println(fullPath);
            }
            entry.close();
        }
    }

    void handleStatus(Print& out) {
        sdcard_type_t sdType = SD.cardType();
        if (sdType == CARD_NONE) {
            out.println("SD: NO_CARD");
            return;
        }

        const char* typeStr = "UNKNOWN";
        switch (sdType) {
            case CARD_MMC:  typeStr = "MMC"; break;
            case CARD_SD:   typeStr = "SDSC"; break;
            case CARD_SDHC: typeStr = "SDHC"; break;
            default: break;
        }

        uint64_t totalBytes = SD.totalBytes();
        uint64_t usedBytes = SD.usedBytes();
        out.printf("SD: READY type=%s total_mb=%llu used_mb=%llu\n",
                   typeStr,
                   totalBytes / (1024ULL * 1024ULL),
                   usedBytes / (1024ULL * 1024ULL));
    }

    void handleList(const String& path, Print& out) {
        sdcard_type_t sdType = SD.cardType();
        if (sdType == CARD_NONE) {
            out.println("ERROR: NO_CARD");
            return;
        }

        String target = path.length() == 0 ? "/" : path;
        if (!target.startsWith("/")) {
            target = "/" + target;
        }

        File root = SD.open(target.c_str());
        if (!root) {
            out.printf("ERROR: CANNOT_OPEN %s\n", target.c_str());
            return;
        }

        out.println("LIST_START");
        if (root.isDirectory()) {
            listDirectory(root, out, target == "/" ? "" : target);
        } else {
            out.printf("FILE %u %s\n", (uint32_t)root.size(), target.c_str());
        }
        root.close();
        out.println("LIST_END");
    }

    void handleGet(const String& rawPath, Print& out) {
        sdcard_type_t sdType = SD.cardType();
        if (sdType == CARD_NONE) {
            out.println("ERROR: NO_CARD");
            return;
        }

        String path = rawPath;
        path.trim();
        if (!path.startsWith("/")) {
            path = "/" + path;
        }

        File file = SD.open(path.c_str(), FILE_READ);
        if (!file || file.isDirectory()) {
            out.printf("ERROR: NOT_FOUND %s\n", path.c_str());
            return;
        }

        size_t fileSize = file.size();
        out.printf("START %s %u\n", path.c_str(), (uint32_t)fileSize);

        static const char hexChars[] = "0123456789ABCDEF";
        uint8_t buffer[256];
        uint32_t seq = 0;
        uint32_t fileCrc = 0xFFFFFFFFUL;

        while (file.available()) {
            size_t bytesRead = file.read(buffer, sizeof(buffer));
            if (bytesRead == 0) break;

            out.printf("BLK %u %u ", seq++, (uint32_t)bytesRead);
            for (size_t i = 0; i < bytesRead; ++i) {
                uint8_t b = buffer[i];
                fileCrc = updateCrc32(fileCrc, b);
                out.print(hexChars[(b >> 4) & 0x0F]);
                out.print(hexChars[b & 0x0F]);
            }
            out.println();
            yield();
        }

        file.close();
        fileCrc ^= 0xFFFFFFFFUL;
        out.printf("END %s %08X\n", path.c_str(), fileCrc);
    }

public:
    bool handle(const String& line, Print& out) override {
        if (line == "sd" || line == "sd help") {
            out.println("Comandos SD:");
            out.println("  sd status             estado de la tarjeta");
            out.println("  sd ls [ruta]          listar archivos recursivamente");
            out.println("  sd get <ruta>         descargar archivo con CRC32");
            return true;
        }

        if (line == "sd status") {
            handleStatus(out);
            return true;
        }

        if (line == "sd ls" || line.startsWith("sd ls ")) {
            String path = "";
            if (line.startsWith("sd ls ")) {
                path = line.substring(6);
                path.trim();
            }
            handleList(path, out);
            return true;
        }

        if (line.startsWith("sd get ")) {
            String path = line.substring(7);
            path.trim();
            handleGet(path, out);
            return true;
        }

        return false;
    }
};

#endif // SD_DEBUG_COMMANDS_H
