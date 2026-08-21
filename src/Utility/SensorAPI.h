#ifndef SENSOR_API_H
#define SENSOR_API_H

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <SD.h>
#include <Eolo/Types/ModemHttpContract.h>
#include "../Board/ModemService.h"

class SensorAPI
{
    static constexpr size_t MaxData = 20;
    static constexpr size_t MaxUrl = ModemHttpContract::kHttpUrlStorageBytes;

    const char* baseURL = "https://api-sensores.cmasccp.cl/insertarMedicion";
    const char* idPrefix = "idsSensores";
    const char* varPrefix = "&idsVariables";
    const char* valuePrefix = "&valores";

    volatile bool isBusy = false; // true only while the spool head is in flight
    static constexpr uint16_t SpoolLimit = 8640;
    static constexpr const char* SpoolPath = "/eolo/telemetry.spool";
    uint16_t spoolPendingCount = 0;
    uint32_t spoolDroppedCount = 0;
    bool spoolLoaded = false;

    static void onSendDone(const ModemJobResult& result, void* context) {
        SensorAPI* self = static_cast<SensorAPI*>(context);
        if (self != nullptr) {
            self->isBusy = false;
            if (result.status == ModemJobStatus::Succeeded && result.httpStatus >= 200 && result.httpStatus < 300)
                self->popSpoolHead();
            self->pumpSpool();
        }

        if (result.status == ModemJobStatus::Succeeded) {
            LOG_F("SensorAPI: Datos enviados (job #%lu, %u bytes)\n",
                  (unsigned long)result.id,
                  (unsigned int)result.bytes);
        } else {
            LOG_F("SensorAPI: fallo al enviar datos (job #%lu, %s)\n",
                  (unsigned long)result.id,
                  result.errorText);
        }
    }

public:
    ModemService* modem;
    int sensorIds[MaxData]{};
    int variableIds[MaxData]{};
    float values[MaxData]{};
    int dataCount;
    int currentId = 0;
    char urlBuffer[MaxUrl]{};

    SensorAPI(ModemService* modemInstance, int count) : modem(modemInstance), dataCount(count) {
        if (dataCount < 0) dataCount = 0;
        if (dataCount > (int)MaxData) dataCount = MaxData;
    }

    ~SensorAPI() = default;

    void begin() {
        if (modem != nullptr) modem->begin();
        loadSpool();
        pumpSpool();
    }

    void addData(int sensorId, int variableId, float value) {
        if (currentId >= dataCount) return;
        sensorIds[currentId] = sensorId;
        variableIds[currentId] = variableId;
        values[currentId] = value;
        currentId++;
        LOG_F("SensorAPI: Añadido dato - SensorID: %d, VariableID: %d, Valor: %.2f\n", sensorId, variableId, value);
    }

    void send() {
        if (currentId == 0) return;
        char* p = urlBuffer;
        size_t left = sizeof(urlBuffer);
        
        if (!appendf(p, left, "%s?%s=", baseURL, idPrefix)) {
            finishBuildError();
            return;
        }
        for(int i = 0; i < currentId; i++) {
            if (!appendf(p, left, "%d%s", sensorIds[i], (i < currentId - 1) ? "," : "")) {
                finishBuildError();
                return;
            }
        }

        if (!appendf(p, left, "%s=", varPrefix)) {
            finishBuildError();
            return;
        }
        for(int i = 0; i < currentId; i++) {
            if (!appendf(p, left, "%d%s", variableIds[i], (i < currentId - 1) ? "," : "")) {
                finishBuildError();
                return;
            }
        }

        if (!appendf(p, left, "%s=", valuePrefix)) {
            finishBuildError();
            return;
        }
        for(int i = 0; i < currentId; i++) {
            const bool coordinate = sensorIds[i] == 754 &&
                                    (variableIds[i] == 11 || variableIds[i] == 12);
            if (!appendf(p, left, coordinate ? "%.6f%s" : "%.2f%s", values[i],
                         (i < currentId - 1) ? "," : "")) {
                finishBuildError();
                return;
            }
        }

        if (!ModemHttpContract::urlFits(urlBuffer)) {
            finishBuildError();
            return;
        }

        currentId = 0;
        if (!appendSpool(urlBuffer)) {
            LOG_LN("SensorAPI: SD no disponible; muestra descartada");
            spoolDroppedCount++;
            return;
        }
        pumpSpool();
    }

    uint16_t pendingTelemetry() const { return spoolPendingCount; }
    uint32_t droppedTelemetry() const { return spoolDroppedCount; }
    bool telemetryInFlight() const { return isBusy; }

private:
    bool appendf(char*& p, size_t& left, const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        int n = vsnprintf(p, left, fmt, ap);
        va_end(ap);
        if (n < 0 || (size_t)n >= left) return false;
        p += n;
        left -= (size_t)n;
        return true;
    }

    void finishBuildError() {
        currentId = 0;
        isBusy = false;
        LOG_LN("SensorAPI: URL demasiado larga; envio descartado");
    }

    void loadSpool() {
        if (spoolLoaded) return;
        spoolLoaded = true;
        if (!SD.begin(SD_CS_PIN)) return;
        File file = SD.open(SpoolPath, FILE_READ);
        if (!file) return;
        while (file.available()) {
            file.readStringUntil('\n');
            if (spoolPendingCount < SpoolLimit) ++spoolPendingCount;
        }
        file.close();
    }

    bool appendSpool(const char* url) {
        loadSpool();
        if (!SD.begin(SD_CS_PIN) || !ModemHttpContract::urlFits(url)) return false;
        if (!SD.exists("/eolo") && !SD.mkdir("/eolo")) return false;
        if (spoolPendingCount >= SpoolLimit) {
            if (!dropOldestSpool()) return false;
            ++spoolDroppedCount;
            LOG_LN("SensorAPI: spool lleno; se descarto la muestra mas antigua");
        }
        File file = SD.open(SpoolPath, FILE_APPEND);
        if (!file) return false;
        bool ok = file.println(url) > 0;
        file.close();
        if (ok) ++spoolPendingCount;
        return ok;
    }

    bool readSpoolHead(char* url, size_t size) {
        if (url == nullptr || size == 0) return false;
        File file = SD.open(SpoolPath, FILE_READ);
        if (!file) return false;
        size_t n = file.readBytesUntil('\n', url, size - 1);
        file.close();
        url[n] = '\0';
        return n > 0 && ModemHttpContract::urlFits(url);
    }

    bool dropOldestSpool() {
        File in = SD.open(SpoolPath, FILE_READ);
        if (!in) return false;
        in.readStringUntil('\n');
        File out = SD.open("/eolo/telemetry.tmp", FILE_WRITE);
        if (!out) { in.close(); return false; }
        while (in.available()) out.println(in.readStringUntil('\n'));
        in.close(); out.close();
        SD.remove(SpoolPath);
        bool ok = SD.rename("/eolo/telemetry.tmp", SpoolPath);
        if (ok && spoolPendingCount) --spoolPendingCount;
        return ok;
    }

    void popSpoolHead() { dropOldestSpool(); }

    void pumpSpool() {
        if (isBusy || spoolPendingCount == 0 || modem == nullptr) return;
        char head[MaxUrl] = "";
        if (!readSpoolHead(head, sizeof(head))) return;
        ModemJobId id = modem->enqueueHttpGet(head, "sensor-api", onSendDone, this);
        if (id != 0) isBusy = true;
    }
};

#endif
