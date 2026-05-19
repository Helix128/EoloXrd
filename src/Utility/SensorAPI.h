#ifndef SENSOR_API_H
#define SENSOR_API_H

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../Board/ModemService.h"

class SensorAPI
{
    static constexpr size_t MaxData = 20;
    static constexpr size_t MaxUrl = 2048;

    const char* baseURL = "http://api-sensores.cmasccp.cl/insertarMedicion";
    const char* idPrefix = "idsSensores";
    const char* varPrefix = "&idsVariables";
    const char* valuePrefix = "&valores";

    volatile bool isBusy = false;

    static void onSendDone(const ModemJobResult& result, void* context) {
        SensorAPI* self = static_cast<SensorAPI*>(context);
        if (self != nullptr) self->isBusy = false;

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
        if (currentId == 0 || isBusy) return;
        isBusy = true; 
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
            if (!appendf(p, left, "%.2f%s", values[i], (i < currentId - 1) ? "," : "")) {
                finishBuildError();
                return;
            }
        }

        currentId = 0;
        ModemJobId id = modem != nullptr
            ? modem->enqueueHttpGet(urlBuffer, "sensor-api", onSendDone, this)
            : 0;

        if (id == 0) {
            isBusy = false;
            LOG_LN("SensorAPI: no se pudo encolar envio");
        } else {
            LOG_F("SensorAPI: envio encolado (job #%lu)\n", (unsigned long)id);
        }
    }

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
};

#endif
