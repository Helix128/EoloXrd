#ifndef FlowMotor_Test
#define FlowMotor_Test

#include "../Data/Context.h"
#include <SD.h>

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
    
    int maxCalPoints = 100;
    int numPoints = 0;
    float currentPct = 0;
    float* flows = new float[maxCalPoints];
    float* motorPcts = new float[maxCalPoints];
    float lastFlow = -1;
    while(currentPct <= 100 && numCalPoints < maxCalPoints)
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
    return;
    LOG_LN("Calibración completa.");
    Serial.print("Rango de flujo medido: ");
    Serial.print(flows[0]);
    Serial.print(" - ");
    Serial.print(flows[10]);
    LOG_LN(" L/min");

    LOG_LN("Ingresa valor de flujo (0-8 L/min) via Serial");
    LOG_LN("Escribe 'exit' para salir");
    LOG_LN();
    
    ctx.components.motor.setPowerPct(0);

    ctx.session.targetFlow = 0.0;
    Serial.print("Flujo objetivo actual: ");
    Serial.print(ctx.session.targetFlow, 1);
    LOG_LN(" L/min");
    
    String inputBuffer = "";
    
    const int numSamples = 4;
    float samples[numSamples] = {0.0};
    int sampleIndex = 0;
    int validSamples = 0;
    
    while(true)
    {
        while (Serial.available() > 0)
        {
            char c = Serial.read();
            if (c == '\n' || c == '\r')
            {
                if (inputBuffer.length() > 0)
                {
                    if (inputBuffer.equalsIgnoreCase("exit"))
                    {
                        ctx.components.motor.setPowerPct(0);
                        LOG_LN("Saliendo de la prueba...");
                        return;
                    }
                    
                    float newTarget = inputBuffer.toFloat();
                    if (newTarget >= 0.0 && newTarget <= 8.0)
                    {
                        ctx.session.targetFlow = newTarget;
                        Serial.print("Nuevo flujo objetivo: ");
                        Serial.print(ctx.session.targetFlow, 1);
                        LOG_LN(" L/min");
                    }
                    else
                    {
                        LOG_LN("Valor inválido. Ingresa 0-8 L/min o 'exit'");
                    }
                    
                    inputBuffer = "";
                }
            }
            else
            {
                inputBuffer += c;
            }
        }
        
        // Leer un sample del sensor de flujo por ciclo
        FlowData flowData;
        if (!ctx.components.flowSensor.getData(flowData) || !flowData.valid)
        {
            LOG_LN("Error al leer sensor de flujo");
            delay(100);
            continue;
        }   
        float newFlow = flowData.flow;
        samples[sampleIndex] = newFlow;
        sampleIndex = (sampleIndex + 1) % numSamples;
        if (validSamples < numSamples) validSamples++;
        
        float sum = 0.0;
        for (int i = 0; i < numSamples; i++) {
            sum += samples[i];
        }
        float averageFlow = sum / validSamples;

        ctx.components.motor.setPowerPct(GetTargetMotorPct(ctx.session.targetFlow, motorPcts, flows, numPoints));

        // Mostrar estado cada segundo aproximadamente
        static unsigned long lastPrint = 0;
        if (millis() - lastPrint > 1000) {
            lastPrint = millis();
            Serial.print("Flujo promedio: ");
            Serial.print(averageFlow, 2);
            Serial.print(" Flujo objetivo: ");
            Serial.print(ctx.session.targetFlow, 1);
            Serial.print(" L/min | Potencia motor: ");
            Serial.print(ctx.components.motor.getPowerPct(), 1);
            LOG_LN("%");
        }
        
        delay(10);
    }
}


#endif