#ifndef EOLO_BOARD_I2C_RETRY_POLICY_H
#define EOLO_BOARD_I2C_RETRY_POLICY_H

#include <stddef.h>
#include <stdint.h>

// Política única para reintentos de periféricos I²C. Mantiene la secuencia
// histórica y evita que el scheduler de componentes y el bus diverjan.
namespace I2CRetryPolicy
{
inline constexpr uint32_t kDelaysMs[] = {250UL, 500UL, 1000UL, 2000UL, 5000UL};
inline constexpr size_t kDelayCount = sizeof(kDelaysMs) / sizeof(kDelaysMs[0]);

constexpr uint32_t delayForFailures(uint8_t failures)
{
    size_t index = failures == 0 ? 0U : static_cast<size_t>(failures - 1U);
    if (index >= kDelayCount)
        index = kDelayCount - 1U;
    return kDelaysMs[index];
}
}

#endif // EOLO_BOARD_I2C_RETRY_POLICY_H
