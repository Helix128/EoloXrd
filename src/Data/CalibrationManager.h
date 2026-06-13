#ifndef CALIBRATION_MANAGER_H
#define CALIBRATION_MANAGER_H

#include "../Config.h"
#include "../Effectors/Motor.h"
#include "Components.h"
#include "Profiler.h"
#include <Preferences.h>
#include <math.h>
#include <Eolo/Core/Calibration/CalibrationModel.h>

class CalibrationManager
{
public:
    static const int MAX_POINTS = CalibrationModel::MaxPoints;
    static const int MIN_POINTS = CalibrationModel::MinPoints;
    static constexpr float MIN_FLOW_RANGE = CalibrationModel::MinFlowRange;

    int numPoints = 0;
    int pwm0[MAX_POINTS];   // PWM del motor 0 en cada punto
    int pwm1[MAX_POINTS];   // PWM del motor 1 en cada punto
    float flows[MAX_POINTS]; // Flujo medido en cada punto

    int weakMotor = 0; // Índice del motor más débil (determinado en discovery)
    bool isLoaded = false;

    void sortByFlow()
    {
        CalibrationModel::sortByFlow(numPoints, pwm0, pwm1, flows);
    }

    bool validate() const
    {
        return CalibrationModel::validate(numPoints, flows);
    }

    bool getMotorPwms(float targetFlow, int &outPwm0, int &outPwm1) const
    {
        return CalibrationModel::getMotorPwms(isLoaded, numPoints, pwm0, pwm1, flows, targetFlow, outPwm0, outPwm1);
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
        LOG_OUT(" Puntos: ");
        LOG_OUT_LN(numPoints);
        LOG_OUT(" Motor débil: ");
        LOG_OUT_LN(weakMotor);
        if (numPoints > 0)
        {
            LOG_OUT(" Rango de flujo: ");
            LOG_OUT(flows[0], 2);
            LOG_OUT(" - ");
            LOG_OUT(flows[numPoints - 1], 2);
            LOG_OUT_LN(" L/min");
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
        LOG_OUT(" Puntos: ");
        LOG_OUT_LN(numPoints);
        LOG_OUT(" Motor débil: ");
        LOG_OUT_LN(weakMotor);
        if (numPoints > 0)
        {
            LOG_OUT(" Rango de flujo: ");
            LOG_OUT(flows[0], 2);
            LOG_OUT(" - ");
            LOG_OUT(flows[numPoints - 1], 2);
            LOG_OUT_LN(" L/min");
        }

        if (!isLoaded)
        {
            LOG_LN("Calibración cargada pero no válida (curva no cumple requisitos mínimos).");
        }

        return isLoaded;
    }

    void test(Components &components)
    {
#ifdef FEATURE_HEADLESS
        (void)components;
        LOG_LN("Diagnóstico de calibración no disponible en modo headless.");
#else
        LOG_OUT_LN("\n========================================");
        LOG_OUT_LN("DIAGNÓSTICO DE CALIBRACIÓN v3");
        LOG_OUT_LN("========================================");

        if (!isLoaded || numPoints == 0)
        {
            LOG_LN("ERROR: No hay calibración cargada!");
            delay(3000);
            return;
        }

        LOG_OUT("Puntos de calibración: ");
        LOG_OUT_LN(numPoints);
        LOG_OUT("Motor débil: ");
        LOG_OUT_LN(weakMotor);

        LOG_OUT_LN("\nTabla PWM0 | PWM1 | Flujo:");
        LOG_OUT_LN("------+------+-------------");
        for (int i = 0; i < numPoints; i++)
        {
            LOG_OUT(pwm0[i]);
            LOG_OUT("\t| ");
            LOG_OUT(pwm1[i]);
            LOG_OUT("\t| ");
            LOG_OUT(flows[i], 2);
            LOG_OUT_LN(" L/min");
        }

        LOG_OUT_LN("\n========================================");
        LOG_OUT_LN("PRUEBA DE INTERPOLACIÓN");
        LOG_OUT_LN("========================================");

        float minFlow = flows[0];
        float maxFlow = flows[numPoints - 1];
        int numTests = 10;
        float step = (maxFlow - minFlow) / (numTests - 1);

        LOG_OUT_LN("Flujo Obj. | PWM0 | PWM1");
        LOG_OUT_LN("-----------+------+------");
        for (int i = 0; i < numTests; i++)
        {
            float targetFlow = minFlow + i * step;
            int p0, p1;
            getMotorPwms(targetFlow, p0, p1);

            LOG_OUT(targetFlow, 2);
            LOG_OUT(" L/min | ");
            LOG_OUT(p0);
            LOG_OUT("\t| ");
            LOG_OUT_LN(p1);
        }

        LOG_OUT_LN("\n========================================");
        LOG_OUT_LN("PRUEBA EN TIEMPO REAL");
        LOG_OUT_LN("========================================");

        for (int i = 0; i < numTests; i++)
        {
            float targetFlow = minFlow + i * step;
            int p0, p1;
            getMotorPwms(targetFlow, p0, p1);

            LOG_OUT("\nObjetivo: ");
            LOG_OUT(targetFlow, 2);
            LOG_OUT(" L/min -> PWM[");
            LOG_OUT(p0);
            LOG_OUT(", ");
            LOG_OUT(p1);
            LOG_OUT_LN("]");

            components.motor.setMotorPwm(0, p0);
            components.motor.setMotorPwm(1, p1);
            delay(2000);

            for (int t = 0; t < 5; t++)
            {
                FlowData flowData;
                if (!components.flowSensor.getData(flowData) || !flowData.valid)
                {
                    LOG_OUT_LN("Error al leer sensor de flujo");
                    continue;
                }
                float diff = flowData.flow - targetFlow;
                LOG_OUT(t);
                LOG_OUT("s | ");
                LOG_OUT(flowData.flow, 2);
                LOG_OUT(" L/min | ");
                if (diff >= 0)
                    LOG_OUT("+");
                LOG_OUT(diff, 2);
                LOG_OUT_LN(" L/min");
                delay(1000);
            }
        }

        components.motor.setPwm(0);
        LOG_OUT_LN("\nDiagnóstico completado.");
        LOG_OUT_LN("Presiona el botón para volver.");
        LOG_OUT_LN("========================================\n");

        while (!components.input.isButtonPressed())
        {
            components.input.poll();
            delay(100);
        }
        components.input.resetCounter();
#endif
    }
};

#endif
