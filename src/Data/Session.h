#ifndef SESSION_H
#define SESSION_H

#include <RTClib.h>

typedef struct Session{

    // Tiempo
    unsigned long startTime = 0;
    DateTime startDate;
    unsigned long endTime = 3600;
    DateTime endDate; 
    unsigned long elapsedTime = 0;
    unsigned long lastLog = 0;
    
    // Flujo
    float targetFlow = 5.0; 
    
    // Sensor de material particulado
    bool usePlantower = true;

    // Datos de sesión
    float capturedVolume = 0.0;

}Session;

#endif