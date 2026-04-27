#ifndef CALIBRATION_MANAGER_H
#define CALIBRATION_MANAGER_H

#include "../Config.h"
#include "../Effectors/Motor.h"
#include "Components.h"
#include "Profiler.h"
#include <Preferences.h>
#include <math.h>

class CalibrationManager
{
public:
    static const int MAX_POINTS = 200;
    static const int MIN_POINTS = 6;
    static constexpr float MIN_FLOW_RANGE = 0.25f;

    int numPoints = 0;
    int pwm0[MAX_POINTS];   // PWM del motor 0 en cada punto
    int pwm1[MAX_POINTS];   // PWM del motor 1 en cada punto
    float flows[MAX_POINTS]; // Flujo medido en cada punto

    int weakMotor = 0; // Índice del motor más débil (determinado en discovery)
    bool isLoaded = false;

    void sortByFlow()
    {
        for (int i = 0; i < numPoints - 1; i++)
        {
            for (int j = 0; j < numPoints - i - 1; j++)
            {
                if (flows[j] > flows[j + 1])
                {
                    float tf = flows[j];
                    flows[j] = flows[j + 1];
                    flows[j + 1] = tf;

                    int tp0 = pwm0[j];
                    pwm0[j] = pwm0[j + 1];
                    pwm0[j + 1] = tp0;

                    int tp1 = pwm1[j];
                    pwm1[j] = pwm1[j + 1];
                    pwm1[j + 1] = tp1;
                }
            }
        }
    }

    bool validate() const
    {
        if (numPoints < MIN_POINTS)
            return false;

        float first = flows[0];
        float last = flows[numPoints - 1];
        if ((last - first) < MIN_FLOW_RANGE)
            return false;

        for (int i = 1; i < numPoints; i++)
        {
            if (flows[i] < flows[i - 1])
                return false;
        }
        return true;
    }

    bool getMotorPwms(float targetFlow, int &outPwm0, int &outPwm1) const
    {
        if (!isLoaded || numPoints < 2)
        {
            outPwm0 = 0;
            outPwm1 = 0;
            return false;
        }

        if (targetFlow <= flows[0])
        {
            outPwm0 = pwm0[0];
            outPwm1 = pwm1[0];
            return true;
        }

        if (targetFlow >= flows[numPoints - 1])
        {
            outPwm0 = pwm0[numPoints - 1];
            outPwm1 = pwm1[numPoints - 1];
            return true;
        }

        for (int i = 0; i < numPoints - 1; i++)
        {
            float f0 = flows[i];
            float f1 = flows[i + 1];

            if (targetFlow >= f0 && targetFlow <= f1)
            {
                float denom = f1 - f0;
                if (fabs(denom) < 1e-6f)
                {
                    outPwm0 = (pwm0[i] + pwm0[i + 1]) / 2;
                    outPwm1 = (pwm1[i] + pwm1[i + 1]) / 2;
                    return true;
                }

                float t = (targetFlow - f0) / denom;
                outPwm0 = pwm0[i] + static_cast<int>(t * (pwm0[i + 1] - pwm0[i]));
                outPwm1 = pwm1[i] + static_cast<int>(t * (pwm1[i + 1] - pwm1[i]));
                return true;
            }
        }

        outPwm0 = pwm0[numPoints - 1];
        outPwm1 = pwm1[numPoints - 1];
        return true;
    }

    void save()
    {
        Preferences preferences;
        preferences.begin("eolo_calib", false);

        preferences.putUChar("calVersion", 3);
        preferences.putInt("numPoints", numPoints);
        preferences.putInt("weakMotor", weakMotor);
        preferences.putBytes("pwm0", pwm0, numPoints * sizeof(int));
        preferences.putBytes("pwm1", pwm1, numPoints * sizeof(int));
        preferences.putBytes("flows", flows, numPoints * sizeof(float));

        preferences.end();

        LOG_LN("Calibración v3 guardada en Flash:");
        Serial.print(" Puntos: ");
        Serial.println(numPoints);
        Serial.print(" Motor débil: ");
        Serial.println(weakMotor);
        if (numPoints > 0)
        {
            Serial.print(" Rango de flujo: ");
            Serial.print(flows[0], 2);
            Serial.print(" - ");
            Serial.print(flows[numPoints - 1], 2);
            Serial.println(" L/min");
        }
    }

    bool load()
    {
        Preferences preferences;
        preferences.begin("eolo_calib", true);

        uint8_t calVersion = preferences.getUChar("calVersion", 0);

        if (calVersion != 3)
        {
            preferences.end();
            if (calVersion > 0)
            {
                LOG_F("Calibración v%d encontrada — incompatible, se requiere recalibración.\n", calVersion);
            }
            else
            {
                LOG_LN("No hay calibración guardada en Flash");
            }
            isLoaded = false;
            return false;
        }

        numPoints = preferences.getInt("numPoints", 0);

        if (numPoints <= 0 || numPoints > MAX_POINTS)
        {
            preferences.end();
            LOG_LN("Calibración inválida en Flash");
            isLoaded = false;
            numPoints = 0;
            return false;
        }

        weakMotor = preferences.getInt("weakMotor", 0);

        size_t p0Bytes = preferences.getBytes("pwm0", pwm0, numPoints * sizeof(int));
        size_t p1Bytes = preferences.getBytes("pwm1", pwm1, numPoints * sizeof(int));
        size_t fBytes = preferences.getBytes("flows", flows, numPoints * sizeof(float));

        preferences.end();

        size_t expectedInt = numPoints * sizeof(int);
        size_t expectedFloat = numPoints * sizeof(float);

        if (p0Bytes != expectedInt || p1Bytes != expectedInt || fBytes != expectedFloat)
        {
            LOG_LN("Error al leer calibración desde Flash");
            isLoaded = false;
            numPoints = 0;
            return false;
        }

        isLoaded = validate();

        LOG_LN("Calibración v3 cargada desde Flash:");
        Serial.print(" Puntos: ");
        Serial.println(numPoints);
        Serial.print(" Motor débil: ");
        Serial.println(weakMotor);
        if (numPoints > 0)
        {
            Serial.print(" Rango de flujo: ");
            Serial.print(flows[0], 2);
            Serial.print(" - ");
            Serial.print(flows[numPoints - 1], 2);
            Serial.println(" L/min");
        }

        if (!isLoaded)
        {
            LOG_LN("Calibración cargada pero no válida (curva no cumple requisitos mínimos).");
        }

        return isLoaded;
    }

    void test(Components &components)
    {
        Serial.println("\n========================================");
        Serial.println("DIAGNÓSTICO DE CALIBRACIÓN v3");
        Serial.println("========================================");

        if (!isLoaded || numPoints == 0)
        {
            LOG_LN("ERROR: No hay calibración cargada!");
            delay(3000);
            return;
        }

        Serial.print("Puntos de calibración: ");
        Serial.println(numPoints);
        Serial.print("Motor débil: ");
        Serial.println(weakMotor);

        Serial.println("\nTabla PWM0 | PWM1 | Flujo:");
        Serial.println("------+------+-------------");
        for (int i = 0; i < numPoints; i++)
        {
            Serial.print(pwm0[i]);
            Serial.print("\t| ");
            Serial.print(pwm1[i]);
            Serial.print("\t| ");
            Serial.print(flows[i], 2);
            Serial.println(" L/min");
        }

        Serial.println("\n========================================");
        Serial.println("PRUEBA DE INTERPOLACIÓN");
        Serial.println("========================================");

        float minFlow = flows[0];
        float maxFlow = flows[numPoints - 1];
        int numTests = 10;
        float step = (maxFlow - minFlow) / (numTests - 1);

        Serial.println("Flujo Obj. | PWM0 | PWM1");
        Serial.println("-----------+------+------");
        for (int i = 0; i < numTests; i++)
        {
            float targetFlow = minFlow + i * step;
            int p0, p1;
            getMotorPwms(targetFlow, p0, p1);

            Serial.print(targetFlow, 2);
            Serial.print(" L/min | ");
            Serial.print(p0);
            Serial.print("\t| ");
            Serial.println(p1);
        }

        Serial.println("\n========================================");
        Serial.println("PRUEBA EN TIEMPO REAL");
        Serial.println("========================================");

        for (int i = 0; i < numTests; i++)
        {
            float targetFlow = minFlow + i * step;
            int p0, p1;
            getMotorPwms(targetFlow, p0, p1);

            Serial.print("\nObjetivo: ");
            Serial.print(targetFlow, 2);
            Serial.print(" L/min -> PWM[");
            Serial.print(p0);
            Serial.print(", ");
            Serial.print(p1);
            Serial.println("]");

            components.motor.setMotorPwm(0, p0);
            components.motor.setMotorPwm(1, p1);
            delay(2000);

            for (int t = 0; t < 5; t++)
            {
                FlowData flowData;
                if (!components.flowSensor.getData(flowData) || !flowData.valid)
                {
                    Serial.println("Error al leer sensor de flujo");
                    continue;
                }
                float diff = flowData.flow - targetFlow;
                Serial.print(t);
                Serial.print("s | ");
                Serial.print(flowData.flow, 2);
                Serial.print(" L/min | ");
                if (diff >= 0)
                    Serial.print("+");
                Serial.print(diff, 2);
                Serial.println(" L/min");
                delay(1000);
            }
        }

        components.motor.setPwm(0);
        Serial.println("\nDiagnóstico completado.");
        Serial.println("Presiona el botón para volver.");
        Serial.println("========================================\n");

        while (!components.input.isButtonPressed())
        {
            components.input.poll();
            delay(100);
        }
        components.input.resetCounter();
    }
};

#endif
