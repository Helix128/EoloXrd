#ifndef SENSOR_API_H
#define SENSOR_API_H

#include <stdio.h>
#include <string.h>
#include "../Board/ModemService.h"

class SensorAPI
{
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
    int* sensorIds;
    int* variableIds;
    float* values;
    int dataCount;
    int currentId = 0;
    char* apiBuffer;
    char* urlBuffer;
    const int API_BUFFER_SIZE = 4096;
    const int MAX_URL_SIZE = 2048;

    SensorAPI(ModemService* modemInstance, int count) : modem(modemInstance), dataCount(count) {
        sensorIds = new int[count];
        variableIds = new int[count];
        values = new float[count];
        urlBuffer = new char[MAX_URL_SIZE];
        apiBuffer = new char[API_BUFFER_SIZE];
    }

    ~SensorAPI() {
        delete[] sensorIds;
        delete[] variableIds;
        delete[] values;
        delete[] urlBuffer;
        delete[] apiBuffer;
    }

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
        
        p += sprintf(p, "%s?%s=", baseURL, idPrefix);
        for(int i = 0; i < currentId; i++) {
            p += sprintf(p, "%d%s", sensorIds[i], (i < currentId - 1) ? "," : "");
        }

        p += sprintf(p, "%s=", varPrefix);
        for(int i = 0; i < currentId; i++) {
            p += sprintf(p, "%d%s", variableIds[i], (i < currentId - 1) ? "," : "");
        }

        p += sprintf(p, "%s=", valuePrefix);
        for(int i = 0; i < currentId; i++) {
            p += sprintf(p, "%.2f%s", values[i], (i < currentId - 1) ? "," : "");
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
};

#endif
