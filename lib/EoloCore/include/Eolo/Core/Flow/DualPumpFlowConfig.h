#ifndef EOLO_CORE_FLOW_DUAL_PUMP_FLOW_CONFIG_H
#define EOLO_CORE_FLOW_DUAL_PUMP_FLOW_CONFIG_H

#include <stdint.h>
#include <Eolo/Core/Flow/FlowMotorController.h>

// Control de flujo de las variantes de dos bombas en serie (Express y
// Standard). Usa la ley continua del Dron sobre el PWM virtual 0..2*maxPwm:
// primero la bomba débil, luego la fuerte (ver DualMotorFlowController).
namespace DualPumpFlow
{
// Curva PWM virtual -> caudal nominal. El tramo 0..2047 es el barrido de una
// bomba del Dron; 2047..4094 supone que la segunda bomba suma ~3 L/min. Es un
// modelo de partida, no una medición de estas variantes: la ley ajusta la
// ganancia observada en línea, así que aguanta ±30 % de error (ver
// test_dual_pump_*), pero el umbral de arranque (primer punto con caudal)
// conviene validarlo en cada bomba.
inline constexpr FlowFeedForwardPoint kFeedForward[] = {
    {184, 0.00f}, {410, 0.80f}, {594, 1.71f}, {819, 2.51f}, {1003, 3.11f},
    {1228, 3.78f}, {1412, 4.60f}, {1638, 5.28f}, {1823, 5.87f}, {2047, 6.58f},
    {2457, 7.18f}, {3276, 8.38f}, {4094, 9.58f},
};

constexpr FlowPidConfig makeConfig(float rampCountsPerSecond)
{
    FlowPidConfig config{};
    config.intervalMs = 100UL;
    config.kp = 20.0f;
    config.ki = 0.8f;
    config.integralLimit = 20.0f;
    config.maxStep = 24;
    config.filterAlpha = 0.30f;
    config.minActive = 0.20f;
    config.kd = 0.0f;
    config.maxDtMs = 1000UL;
    config.sensorStaleMs = 1200UL;
    config.kickPwm = 800;
    config.kickMs = 250UL;
    config.stallFlowLpm = 0.10f;
    config.restallCooldownMs = 8000UL;
    config.stallConfirmMs = 1500UL;
    config.softTrimMax = 80;
    config.softMaxStep = 2;
    config.sensitivity = 1.0f;
    config.recenterDelayMs = 4000UL;
    config.zeroFlowConfirmSamples = 5;
    config.feedForward = {kFeedForward, static_cast<uint8_t>(sizeof(kFeedForward) / sizeof(kFeedForward[0]))};
    config.rampCountsPerSecond = rampCountsPerSecond;
    config.motorCount = 2;
    // Zona muerta de la integral: ruido del AFM07 de 0,004-0,013 L/min.
    config.deadband = 0.012f;
    config.deadbandRel = 0.0f;
    config.driftThreshold = 0.01f;
    config.freezeIntegral = true;
    // Ley continua: PI incremental normalizado por la pendiente de la tabla.
    // Llega a la consigna sin sobrepico y, si algo externo la desvía, sigue
    // corrigiendo sin saltos de centro ni tope de recorte.
    config.smoothLaw = true;
    config.smoothKi = 0.70f;
    // El término P actúa sobre la medición cruda (filterAlpha no interviene):
    // con ruido de 0,10 L/min, 0,15 mueve el PWM 1,5 veces menos que 0,40 y
    // asienta igual de rápido. El Dron usa 0,40 con sigma <= 0,007 L/min.
    config.smoothKp = 0.15f;
    config.smoothKiStart = 1.30f;
    config.smoothSlew = 60; // el doble que el Dron: el rango virtual también lo es
    config.smoothSlewStart = 200;
    config.smoothStartFraction = 0.92f;
    config.smoothHoldMs = 1500UL;
    return config;
}
} // namespace DualPumpFlow

#endif
