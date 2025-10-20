#ifndef SESSION_H
#define SESSION_H

typedef struct Session{

    // Tiempo
    unsigned long startTime = 0;
    unsigned long endTime = 3600; 

    // Flujo
    float targetFlow = 5.0; 

    // Sensor de material particulado
    bool usePlantower = true;

}Session;

#endif