#ifndef FlowMotor_Test
#define FlowMotor_Test

#include "../Data/Context.h"
#include <SD.h>

float GetTargetMotorPct(float targetFlow, float* calMotorPcts, float* calFlows, int numPoints)
{
    if (targetFlow <= calFlows[0])
        return calMotorPcts[0];
    
    for (int i = 0; i < numPoints; i++) {
        if (calFlows[i] >= targetFlow) {
            return calMotorPcts[i];
        }
    }
    return calMotorPcts[numPoints - 1];
}

void testFlowMotor(Context &ctx)
{   
    Serial.println("=== Test Motor-Flujo ===");

    Serial.println("Calibrando...");
    
    int maxCalPoints = 100;
    int numCalPoints = 0;
    float currentPct = 0;
    float* calFlows = new float[maxCalPoints];
    float* calMotorPcts = new float[maxCalPoints];
    float lastFlow = -1;
    while(currentPct <= 100 && numCalPoints < maxCalPoints)
    {   
        ctx.components.motor.setPowerPct(currentPct);
        delay(1000); 

        ctx.components.flowSensor.readData();
        float measuredFlow = ctx.components.flowSensor.flow;
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
        Serial.println(" L/min");

        calMotorPcts[numCalPoints] = currentPct;
        calFlows[numCalPoints] = measuredFlow;
        numCalPoints++;
        
        currentPct += 1.0f; // Incrementar en 1%
    }

    ctx.components.motor.setPowerPct(0);

    // Save calibration to CSV
    File file = SD.open("/calibracion.csv", FILE_WRITE);
    if (file) {
        file.println("motor,flujo");
        for (int i = 0; i < numCalPoints; i++) {
            file.print(calMotorPcts[i]);
            file.print(",");
            file.println(calFlows[i]);
        }
        file.close();
        Serial.println("Calibración guardada en calibracion.csv");
    } else {
        Serial.println("Error al abrir calibracion.csv");
    }
    return;
    Serial.println("Calibración completa.");
    Serial.print("Rango de flujo medido: ");
    Serial.print(calFlows[0]);
    Serial.print(" - ");
    Serial.print(calFlows[10]);
    Serial.println(" L/min");

    Serial.println("Ingresa valor de flujo (0-20 L/min) via Serial");
    Serial.println("Escribe 'exit' para salir");
    Serial.println();
    
    ctx.components.motor.setPowerPct(0);

    ctx.session.targetFlow = 0.0;
    Serial.print("Flujo objetivo actual: ");
    Serial.print(ctx.session.targetFlow, 1);
    Serial.println(" L/min");
    
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
                        Serial.println("Saliendo de la prueba...");
                        return;
                    }
                    
                    float newTarget = inputBuffer.toFloat();
                    if (newTarget >= 0.0 && newTarget <= 20.0)
                    {
                        ctx.session.targetFlow = newTarget;
                        Serial.print("Nuevo flujo objetivo: ");
                        Serial.print(ctx.session.targetFlow, 1);
                        Serial.println(" L/min");
                    }
                    else
                    {
                        Serial.println("Valor inválido. Ingresa 0-20 L/min o 'exit'");
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
        ctx.components.flowSensor.readData();
        float newFlow = ctx.components.flowSensor.flow;
        samples[sampleIndex] = newFlow;
        sampleIndex = (sampleIndex + 1) % numSamples;
        if (validSamples < numSamples) validSamples++;
        
        float sum = 0.0;
        for (int i = 0; i < numSamples; i++) {
            sum += samples[i];
        }
        float averageFlow = sum / validSamples;

        ctx.components.motor.setPowerPct(GetTargetMotorPct(ctx.session.targetFlow, calMotorPcts, calFlows, numCalPoints));

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
            Serial.println("%");
        }
        
        delay(10);
    }
}


#endif