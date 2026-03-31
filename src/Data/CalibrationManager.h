#ifndef CALIBRATION_MANAGER_H
#define CALIBRATION_MANAGER_H

#include "../Config.h"
#include "../Effectors/Motor.h"
#include "Components.h"
#include "Profiler.h"
#include <Preferences.h>
#include <math.h>
#include <string.h>

class CalibrationManager
{
public:
    static const int MAX_POINTS = 200;
    static const int MIN_POINTS_PHASE = 6;
    static constexpr float MIN_FLOW_RANGE = 0.25f;

    int numPoints = 0;
    float motorPcts[MAX_POINTS];
    float flows[MAX_POINTS];

    int numPointsSmall = 0;
    float motorPctsSmall[MAX_POINTS];
    float flowsSmall[MAX_POINTS];

    int numPointsBig = 0;
    float motorPctsBig[MAX_POINTS];
    float flowsBig[MAX_POINTS];

    int numPointsBoth = 0;
    float motorPctsBoth[MAX_POINTS];
    float flowsBoth[MAX_POINTS];

    bool isLoaded = false;

    bool validateCurve(const float *curveFlows, int points, float minRange = MIN_FLOW_RANGE) const
    {
        if (points < MIN_POINTS_PHASE)
            return false;

        if (curveFlows == nullptr)
            return false;

        float first = curveFlows[0];
        float last = curveFlows[points - 1];
        if ((last - first) < minRange)
            return false;

        for (int i = 1; i < points; i++)
        {
            if (curveFlows[i] < curveFlows[i - 1])
                return false;
        }

        return true;
    }

    bool hasCompletePhaseCalibration() const
    {
        return validateCurve(flowsSmall, numPointsSmall) &&
               validateCurve(flowsBig, numPointsBig) &&
               validateCurve(flowsBoth, numPointsBoth);
    }

    static int sortArrays(float *mPcts, float *fls, int n)
    {
        if (n < 2)
            return n;

        for (int i = 0; i < n - 1; i++)
        {
            for (int j = 0; j < n - i - 1; j++)
            {
                if (fls[j] > fls[j + 1])
                {
                    float tf = fls[j];
                    fls[j] = fls[j + 1];
                    fls[j + 1] = tf;

                    float tm = mPcts[j];
                    mPcts[j] = mPcts[j + 1];
                    mPcts[j + 1] = tm;
                }
            }
        }
        return n;
    }

    void commitPhase(MotorManager::DistributionMode mode, float *mPcts, float *fls, int n)
    {
        if (n <= 0)
            return;

        n = sortArrays(mPcts, fls, n);

        if (mode == MotorManager::SMALL_ONLY)
        {
            numPointsSmall = n;
            memcpy(motorPctsSmall, mPcts, n * sizeof(float));
            memcpy(flowsSmall, fls, n * sizeof(float));
        }
        else if (mode == MotorManager::BIG_ONLY)
        {
            numPointsBig = n;
            memcpy(motorPctsBig, mPcts, n * sizeof(float));
            memcpy(flowsBig, fls, n * sizeof(float));
        }
        else if (mode == MotorManager::BOTH_EXTREME)
        {
            numPointsBoth = n;
            memcpy(motorPctsBoth, mPcts, n * sizeof(float));
            memcpy(flowsBoth, fls, n * sizeof(float));
        }

        // Curva global legado: priorizar BOTH, luego BIG, luego SMALL.
        if (numPointsBoth > 1)
        {
            numPoints = numPointsBoth;
            memcpy(motorPcts, motorPctsBoth, numPoints * sizeof(float));
            memcpy(flows, flowsBoth, numPoints * sizeof(float));
        }
        else if (numPointsBig > 1)
        {
            numPoints = numPointsBig;
            memcpy(motorPcts, motorPctsBig, numPoints * sizeof(float));
            memcpy(flows, flowsBig, numPoints * sizeof(float));
        }
        else if (numPointsSmall > 1)
        {
            numPoints = numPointsSmall;
            memcpy(motorPcts, motorPctsSmall, numPoints * sizeof(float));
            memcpy(flows, flowsSmall, numPoints * sizeof(float));
        }
    }

    float interpolateCurve(float targetFlow, const float *curveFlows, const float *curvePcts, int points) const
    {
        if (points <= 0)
            return 0.0f;

        if (targetFlow <= curveFlows[0])
            return curvePcts[0];

        if (targetFlow >= curveFlows[points - 1])
            return curvePcts[points - 1];

        for (int i = 0; i < points - 1; i++)
        {
            float f0 = curveFlows[i];
            float f1 = curveFlows[i + 1];
            float m0 = curvePcts[i];
            float m1 = curvePcts[i + 1];

            if (targetFlow >= f0 && targetFlow <= f1)
            {
                float denom = (f1 - f0);
                if (fabs(denom) < 1e-6f)
                    return (m0 + m1) / 2.0f;

                float t = (targetFlow - f0) / denom;
                return m0 + t * (m1 - m0);
            }
        }

        return curvePcts[points - 1];
    }

    float getTargetMotorPct(float targetFlow, MotorManager &motor)
    {
        if (!isLoaded || numPoints == 0)
        {
            LOG_LN("Advertencia: No hay calibración cargada.");
            return 0;
        }

        MotorManager::DistributionMode mode = motor.getResolvedMode();
        if (mode == MotorManager::SMALL_ONLY && numPointsSmall > 1)
            return interpolateCurve(targetFlow, flowsSmall, motorPctsSmall, numPointsSmall);

        if (mode == MotorManager::BIG_ONLY && numPointsBig > 1)
            return interpolateCurve(targetFlow, flowsBig, motorPctsBig, numPointsBig);

        if (mode == MotorManager::BOTH_EXTREME && numPointsBoth > 1)
            return interpolateCurve(targetFlow, flowsBoth, motorPctsBoth, numPointsBoth);

        return interpolateCurve(targetFlow, flows, motorPcts, numPoints);
    }

    void save()
    {
        Preferences preferences;
        preferences.begin("eolo_calib", false);

        uint8_t calVersion = (numPointsSmall > 1 && numPointsBig > 1 && numPointsBoth > 1) ? 2 : 1;
        preferences.putUChar("calVersion", calVersion);
        preferences.putInt("numPoints", numPoints);
        preferences.putBytes("motorPcts", motorPcts, numPoints * sizeof(float));
        preferences.putBytes("flows", flows, numPoints * sizeof(float));

        if (calVersion >= 2)
        {
            preferences.putInt("numPtsSm", numPointsSmall);
            preferences.putBytes("mtrPctSm", motorPctsSmall, numPointsSmall * sizeof(float));
            preferences.putBytes("flowsSm", flowsSmall, numPointsSmall * sizeof(float));

            preferences.putInt("numPtsBg", numPointsBig);
            preferences.putBytes("mtrPctBg", motorPctsBig, numPointsBig * sizeof(float));
            preferences.putBytes("flowsBg", flowsBig, numPointsBig * sizeof(float));

            preferences.putInt("numPtsBt", numPointsBoth);
            preferences.putBytes("mtrPctBt", motorPctsBoth, numPointsBoth * sizeof(float));
            preferences.putBytes("flowsBt", flowsBoth, numPointsBoth * sizeof(float));
        }

        preferences.end();

        LOG_LN("Calibración guardada en Flash:");
        Serial.print(" Número de puntos: ");
        Serial.println(numPoints);
        if (numPoints > 0)
        {
            Serial.print(" Rango de flujo: ");
            Serial.print(flows[0], 2);
            Serial.print(" - ");
            Serial.print(flows[numPoints - 1], 2);
            Serial.println(" L/min");
        }

        if (hasCompletePhaseCalibration())
        {
            LOG_LN("Set completo de calibración por fases disponible (small/big/both)");
        }
        else
        {
            LOG_LN("Advertencia: calibración por fases incompleta; se mantiene fallback legacy/global");
        }
    }

    bool load()
    {
        Preferences preferences;
        preferences.begin("eolo_calib", false);

        uint8_t calVersion = preferences.getUChar("calVersion", 0);

        if (!preferences.isKey("numPoints"))
        {
            preferences.end();
            LOG_LN("No hay calibración guardada en Flash");
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

        size_t motorBytes = preferences.getBytes("motorPcts", motorPcts, numPoints * sizeof(float));
        size_t flowBytes = preferences.getBytes("flows", flows, numPoints * sizeof(float));

        // Reset por defecto de calibración por fase.
        numPointsSmall = 0;
        numPointsBig = 0;
        numPointsBoth = 0;

        if (calVersion >= 2)
        {
            int ptsSm = preferences.getInt("numPtsSm", 0);
            int ptsBg = preferences.getInt("numPtsBg", 0);
            int ptsBt = preferences.getInt("numPtsBt", 0);

            if (ptsSm > 0 && ptsSm <= MAX_POINTS)
            {
                size_t smM = preferences.getBytes("mtrPctSm", motorPctsSmall, ptsSm * sizeof(float));
                size_t smF = preferences.getBytes("flowsSm", flowsSmall, ptsSm * sizeof(float));
                if (smM == (size_t)(ptsSm * sizeof(float)) && smF == (size_t)(ptsSm * sizeof(float)))
                    numPointsSmall = ptsSm;
            }

            if (ptsBg > 0 && ptsBg <= MAX_POINTS)
            {
                size_t bgM = preferences.getBytes("mtrPctBg", motorPctsBig, ptsBg * sizeof(float));
                size_t bgF = preferences.getBytes("flowsBg", flowsBig, ptsBg * sizeof(float));
                if (bgM == (size_t)(ptsBg * sizeof(float)) && bgF == (size_t)(ptsBg * sizeof(float)))
                    numPointsBig = ptsBg;
            }

            if (ptsBt > 0 && ptsBt <= MAX_POINTS)
            {
                size_t btM = preferences.getBytes("mtrPctBt", motorPctsBoth, ptsBt * sizeof(float));
                size_t btF = preferences.getBytes("flowsBt", flowsBoth, ptsBt * sizeof(float));
                if (btM == (size_t)(ptsBt * sizeof(float)) && btF == (size_t)(ptsBt * sizeof(float)))
                    numPointsBoth = ptsBt;
            }
        }

        preferences.end();

        if (motorBytes != numPoints * sizeof(float) || flowBytes != numPoints * sizeof(float))
        {
            LOG_LN("Error al leer calibración desde Flash");
            isLoaded = false;
            numPoints = 0;
            return false;
        }

        bool legacyValid = validateCurve(flows, numPoints, 0.10f);
        bool phasedValid = hasCompletePhaseCalibration();
        isLoaded = phasedValid || legacyValid;
        LOG_LN("Calibración cargada desde Flash:");
        Serial.print(" Versión de calibración: ");
        Serial.println(calVersion);
        Serial.print(" Número de puntos: ");
        Serial.println(numPoints);
        if (numPoints > 0)
        {
            Serial.print(" Rango de flujo: ");
            Serial.print(flows[0], 2);
            Serial.print(" - ");
            Serial.print(flows[numPoints - 1], 2);
            Serial.println(" L/min");

            if (flows[0] == 0 && flows[numPoints - 1] == 0)
            {
                LOG_LN("Calibración inválida detectada (rango 0-0). Ejecutando nueva calibración...");
                isLoaded = false;
                return false;
            }
        }

        if (phasedValid)
        {
            LOG_LN("Curvas por fase cargadas (small/big/both)");
        }
        else if (!legacyValid)
        {
            LOG_LN("Calibración inválida: ni curva global ni set por fases cumplen mínimos.");
            return false;
        }

        return true;
    }

    void test(Components &components)
    {
        Serial.println("\n========================================");
        Serial.println("DIAGNÓSTICO DE CALIBRACIÓN");
        Serial.println("========================================");

        if (!isLoaded || numPoints == 0)
        {
            LOG_LN("ERROR: No hay calibración cargada!");
            delay(3000);
            return;
        }

        Serial.print("Puntos de calibración: ");
        Serial.println(numPoints);
        Serial.println("\nTabla completa Motor% -> Flujo:");
        Serial.println("Motor%  | Flujo(L/min)");
        Serial.println("--------+-------------");
        for (int i = 0; i < numPoints; i++)
        {
            Serial.print(motorPcts[i], 1);
            Serial.print("%");
            if (motorPcts[i] < 10)
                Serial.print("  ");
            else if (motorPcts[i] < 100)
                Serial.print(" ");
            Serial.print("     | ");
            Serial.println(flows[i], 2);
        }

        Serial.println("\n========================================");
        Serial.println("PRUEBA DE INTERPOLACIÓN");
        Serial.println("========================================");

        float minFlow = flows[0];
        float maxFlow = flows[numPoints - 1];
        int numTests = 10;
        float step = (maxFlow - minFlow) / (numTests - 1);

        Serial.println("Flujo Obj. | Motor% Calc. | Esperado");
        Serial.println("-----------+--------------+---------");
        for (int i = 0; i < numTests; i++)
        {
            float targetFlow = minFlow + i * step;
            float calculatedPct = getTargetMotorPct(targetFlow, components.motor);

            Serial.print(targetFlow, 2);
            Serial.print(" L/min");
            if (targetFlow < 10)
                Serial.print("  ");
            Serial.print(" | ");
            Serial.print(calculatedPct, 1);
            Serial.print("%");
            if (calculatedPct < 10)
                Serial.print("  ");
            else if (calculatedPct < 100)
                Serial.print(" ");
            Serial.print("       | ");

            bool found = false;
            for (int j = 0; j < numPoints - 1; j++)
            {
                if (targetFlow >= flows[j] && targetFlow <= flows[j + 1])
                {
                    float ratio = (targetFlow - flows[j]) / (flows[j + 1] - flows[j]);
                    float expectedPct = motorPcts[j] + ratio * (motorPcts[j + 1] - motorPcts[j]);
                    Serial.print(expectedPct, 1);
                    Serial.print("%");
                    found = true;
                    break;
                }
            }
            if (!found)
                Serial.print("fuera rango");
            Serial.println();
        }

        Serial.println("\n========================================");
        Serial.println("PRUEBA EN TIEMPO REAL");
        Serial.println("========================================");
        Serial.println("Probando todo el rango de flujo calibrado...");

        for (int i = 0; i < numTests; i++)
        {
            float targetFlow = minFlow + i * step;
            float calculatedPct = getTargetMotorPct(targetFlow, components.motor);

            Serial.print("\nAplicando motor al ");
            Serial.print(calculatedPct, 1);
            Serial.print("% para ");
            Serial.print(targetFlow, 2);
            Serial.println(" L/min");

            components.motor.setPowerPct(calculatedPct);

            for (int j = 0; j < 20; j++)
            {
                ;
                delay(100);
            }

            Serial.println("Tiempo | Flujo Real | Diferencia");
            Serial.println("-------+------------+-----------");
            for (int t = 0; t < 5; t++)
            {
                FlowData flowData;
                if (!components.flowSensor.getData(flowData) || !flowData.valid)
                {
                    Serial.println("Error al leer sensor de flujo");
                    continue;
                }
                float realFlow = flowData.flow;

                float diff = realFlow - targetFlow;

                Serial.print(t);
                Serial.print("s     | ");
                Serial.print(realFlow, 2);
                Serial.print(" L/min | ");
                if (diff >= 0)
                    Serial.print("+");
                Serial.print(diff, 2);
                Serial.println(" L/min");

                delay(1000);
            }
        }

        components.motor.setPowerPct(0);

        Serial.println("\n========================================");
        Serial.println("Diagnóstico completado");
        Serial.println("Presiona el botón para volver");
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
