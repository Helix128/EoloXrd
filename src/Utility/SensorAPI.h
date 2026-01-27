#ifndef SENSOR_API_H
#define SENSOR_API_H

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "../Board/Modem.h"

#define SENSOR_CMD_INIT (1 << 0)
#define SENSOR_CMD_SEND (1 << 1)

class SensorAPI
{
    const char* baseURL = "http://api-sensores.cmasccp.cl/insertarMedicion";
    const char* idPrefix = "idsSensores";
    const char* varPrefix = "&idsVariables";
    const char* valuePrefix = "&valores";

    TaskHandle_t taskHandle = nullptr;
    volatile bool isBusy = false;

    static void taskWorker(void* arg) {
        SensorAPI* self = (SensorAPI*)arg;
        uint32_t flags = 0;

        while (true) {
            if (xTaskNotifyWait(0, ULONG_MAX, &flags, portMAX_DELAY) == pdTRUE) {
                if (flags & SENSOR_CMD_INIT) {
                    LOG_LN("Inicializando SensorAPI...");
                    self->modem->begin();
                    LOG_LN("SensorAPI inicializada");
                }
                
                if (flags & SENSOR_CMD_SEND) {
                    LOG_LN("SensorAPI: Enviando datos...");
                    self->modem->get(self->urlBuffer, self->apiBuffer, self->API_BUFFER_SIZE);
                    self->isBusy = false;
                    LOG_LN("SensorAPI: Datos enviados");
                }
            }
        }
    }

public:
    Modem* modem;
    int* sensorIds;
    int* variableIds;
    float* values;
    int dataCount;
    int currentId = 0;
    char* apiBuffer;
    char* urlBuffer;
    const int API_BUFFER_SIZE = 4096;
    const int MAX_URL_SIZE = 2048;

    SensorAPI(Modem* modemInstance, int count) : modem(modemInstance), dataCount(count) {
        sensorIds = new int[count];
        variableIds = new int[count];
        values = new float[count];
        urlBuffer = new char[MAX_URL_SIZE];
        apiBuffer = new char[API_BUFFER_SIZE];

        xTaskCreatePinnedToCore(taskWorker, "SensorAPI", 4096, this, 1, &taskHandle, 0);
    }

    ~SensorAPI() {
        if (taskHandle) vTaskDelete(taskHandle);
        delete[] sensorIds;
        delete[] variableIds;
        delete[] values;
        delete[] urlBuffer;
        delete[] apiBuffer;
    }

    void begin() {
        xTaskNotify(taskHandle, SENSOR_CMD_INIT, eSetBits);
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
        xTaskNotify(taskHandle, SENSOR_CMD_SEND, eSetBits);

        LOG_LN("SensorAPI: Preparado para enviar datos");
    }
};

#endif