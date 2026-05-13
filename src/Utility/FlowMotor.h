#ifndef FlowMotor_Test
#define FlowMotor_Test

#include "../Data/Context.h"
#include <SD.h>
#include <vector>

float GetTargetMotorPct(float targetFlow, float* motorPcts, float* flows, int numPoints)
{
    if (targetFlow <= flows[0])
        return motorPcts[0];
    
    for (int i = 0; i < numPoints; i++) {
        if (flows[i] >= targetFlow) {
            return motorPcts[i];
        }
    }
    return motorPcts[numPoints - 1];
}

void testFlowMotor(Context &ctx)
{   
    LOG_LN("=== Test Motor-Flujo ===");

    LOG_LN("Calibrando...");
    
    const int maxCalPoints = 100;
    int numPoints = 0;
    float currentPct = 0;
    std::vector<float> flows(maxCalPoints);
    std::vector<float> motorPcts(maxCalPoints);
    float lastFlow = -1;
    while(currentPct <= 100 && numPoints < maxCalPoints)
    {   
        ctx.components.motor.setPowerPct(currentPct);
        delay(1000); 

        FlowData flowData;
        if (!ctx.components.flowSensor.getData(flowData) || !flowData.valid)
        {
            LOG_LN("Error al leer sensor de flujo durante calibración");
            continue;
        }
        float measuredFlow = flowData.flow;
        if(measuredFlow-lastFlow<0.1f && currentPct!=0)
        {
            //currentPct += 1.0f;
            //continue;
        }
        lastFlow = measuredFlow;
        Serial.print("Motor ");
        Serial.print(currentPct);
        Serial.print("% -> Flujo medido: ");
        Serial.print(measuredFlow, 2);
        LOG_LN(" L/min");

        motorPcts[numPoints] = currentPct;
        flows[numPoints] = measuredFlow;
        numPoints++;
        
        currentPct += 1.0f; // Incrementar en 1%
    }

    ctx.components.motor.setPowerPct(0);

    // Save calibration to CSV
    File file = SD.open("/calibracion.csv", FILE_WRITE);
    if (file) {
        file.println("motor,flujo");
        for (int i = 0; i < numPoints; i++) {
            file.print(motorPcts[i]);
            file.print(",");
            file.println(flows[i]);
        }
        file.close();
        LOG_LN("Calibración guardada en calibracion.csv");
    } else {
        LOG_LN("Error al abrir calibracion.csv");
    }
}


#endif