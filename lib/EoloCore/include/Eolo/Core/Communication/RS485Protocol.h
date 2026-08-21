#ifndef EOLO_CORE_COMMUNICATION_RS485_PROTOCOL_H
#define EOLO_CORE_COMMUNICATION_RS485_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

namespace EoloCore
{

enum class ModbusReadStatus : uint8_t
{
    Ok,
    Incomplete,
    InvalidCrc,
    UnexpectedSlave,
    UnexpectedFunction,
    InvalidByteCount,
    Exception
};

struct ModbusReadResult
{
    ModbusReadStatus status = ModbusReadStatus::Incomplete;
    uint8_t exceptionCode = 0;
};

class ModbusRtuProtocol
{
public:
    static constexpr uint8_t ReadHoldingRegisters = 0x03;
    static constexpr uint8_t MaxReadRegisters = 64;

    static uint16_t crc16(const uint8_t *data, size_t length)
    {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < length; ++i)
        {
            crc ^= data[i];
            for (uint8_t bit = 0; bit < 8; ++bit)
                crc = (crc & 1U) ? static_cast<uint16_t>((crc >> 1) ^ 0xA001U)
                                 : static_cast<uint16_t>(crc >> 1);
        }
        return crc;
    }

    static bool buildReadHolding(uint8_t slaveId, uint16_t startReg, uint8_t count,
                                 uint8_t *request, size_t capacity)
    {
        if (request == nullptr || capacity < 8 || count == 0 || count > MaxReadRegisters)
            return false;

        request[0] = slaveId;
        request[1] = ReadHoldingRegisters;
        request[2] = static_cast<uint8_t>(startReg >> 8);
        request[3] = static_cast<uint8_t>(startReg);
        request[4] = 0x00;
        request[5] = count;
        const uint16_t frameCrc = crc16(request, 6);
        request[6] = static_cast<uint8_t>(frameCrc);
        request[7] = static_cast<uint8_t>(frameCrc >> 8);
        return true;
    }

    static ModbusReadResult parseReadResponse(const uint8_t *frame, size_t length,
                                              uint8_t expectedSlave, uint8_t expectedCount,
                                              uint16_t *registers)
    {
        ModbusReadResult result;
        if (frame == nullptr || length < 5 || expectedCount == 0 ||
            expectedCount > MaxReadRegisters)
            return result;

        if (frame[0] != expectedSlave)
        {
            result.status = ModbusReadStatus::UnexpectedSlave;
            return result;
        }

        const uint16_t receivedCrc = static_cast<uint16_t>(frame[length - 2]) |
                                      static_cast<uint16_t>(frame[length - 1] << 8);
        if (crc16(frame, length - 2) != receivedCrc)
        {
            result.status = ModbusReadStatus::InvalidCrc;
            return result;
        }

        if (frame[1] == static_cast<uint8_t>(ReadHoldingRegisters | 0x80U))
        {
            if (length != 5)
            {
                result.status = ModbusReadStatus::InvalidByteCount;
                return result;
            }
            result.status = ModbusReadStatus::Exception;
            result.exceptionCode = frame[2];
            return result;
        }

        if (frame[1] != ReadHoldingRegisters)
        {
            result.status = ModbusReadStatus::UnexpectedFunction;
            return result;
        }

        const uint8_t expectedByteCount = static_cast<uint8_t>(expectedCount * 2U);
        if (frame[2] != expectedByteCount || length != static_cast<size_t>(5U + expectedByteCount))
        {
            result.status = ModbusReadStatus::InvalidByteCount;
            return result;
        }

        if (registers != nullptr)
        {
            for (uint8_t i = 0; i < expectedCount; ++i)
                registers[i] = static_cast<uint16_t>(frame[3U + i * 2U] << 8) |
                               static_cast<uint16_t>(frame[4U + i * 2U]);
        }
        result.status = ModbusReadStatus::Ok;
        return result;
    }
};

class RS485TimingModel
{
public:
    static constexpr uint32_t kAfmIntervalMs = 200;
    static constexpr uint32_t kAnemometerIntervalMs = 1100;
    static constexpr uint32_t kAnemometerOfflineIntervalMs = 5000;
    static constexpr uint32_t kResponseStartTimeoutMs = 250;
    static constexpr uint32_t kFrameCompletionTimeoutMs = 35;
    static constexpr uint32_t kBusQuietUs = 8000;
    // Incluye la espera máxima para despejar un bus ocupado, la respuesta
    // Modbus y la ventana de cierre de trama.
    static constexpr uint32_t kBusQuietTimeoutMs = 100;
    static constexpr uint32_t kAnemometerSlotBudgetMs = 150;

    static bool due(uint32_t nowMs, uint32_t deadlineMs)
    {
        return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
    }

    static uint32_t timeUntil(uint32_t nowMs, uint32_t deadlineMs)
    {
        return deadlineMs - nowMs;
    }

    static bool optionalFitsBeforeCritical(uint32_t nowMs, uint32_t criticalDueMs,
                                           bool criticalRegistered,
                                           uint32_t optionalBudgetMs)
    {
        return !criticalRegistered || due(nowMs, criticalDueMs) ||
               timeUntil(nowMs, criticalDueMs) >= optionalBudgetMs;
    }

    static uint32_t nextPeriodicDue(uint32_t previousDueMs, uint32_t nowMs,
                                    uint32_t intervalMs)
    {
        if (intervalMs == 0)
            return nowMs;
        uint32_t next = previousDueMs + intervalMs;
        while (due(nowMs, next))
            next += intervalMs;
        return next;
    }
};

} // namespace EoloCore

#endif // EOLO_CORE_COMMUNICATION_RS485_PROTOCOL_H
