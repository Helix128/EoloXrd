#ifndef SESSION_H
#define SESSION_H

#include <RTClib.h>

typedef struct Session{

    // Tiempo
    
    DateTime startDate;
    unsigned int duration = 0;
    unsigned long elapsedTime = 0;
    unsigned long lastLog = 0;
    
    // Flujo
    float targetFlow = 5.0; 
    
    // Sensor de material particulado
    bool usePlantower = true;

    // Datos de sesión
    float capturedVolume = 0.0; // GUARDADO COMO LITROS, EN LAS ESCENAS SE MUESTRA COMO m3 (1m3 = 1000L)

}Session;

#endif