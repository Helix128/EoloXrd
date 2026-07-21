#ifndef EOLO_TYPES_MODEM_TYPES_H
#define EOLO_TYPES_MODEM_TYPES_H

#include <stddef.h>
#include <stdint.h>

using ModemJobId = uint32_t;

enum class ModemJobKind : uint8_t
{
  HttpGet,
  HttpPost,
  AtCommand
};

enum class ModemJobStatus : uint8_t
{
  Queued,
  Running,
  Succeeded,
  Failed,
  Canceled
};

enum class ModemServiceState : uint8_t
{
  Off,
  Booting,
  AtReady,
  SimReady,
  Registered,
  DataReady,
  Busy,
  IdleWaitingPowerOff,
  Error
};

struct ModemJobResult
{
  ModemJobId id = 0;
  ModemJobKind kind = ModemJobKind::HttpGet;
  char tag[32] = "";
  ModemJobStatus status = ModemJobStatus::Queued;
  int httpStatus = 0;
  size_t bytes = 0;
  uint32_t durationMs = 0;
  uint8_t attempts = 0;
  char errorText[96] = "";
  char responsePreview[256] = "";
};

using ModemJobCallback = void (*)(const ModemJobResult &result, void *context);

#endif
