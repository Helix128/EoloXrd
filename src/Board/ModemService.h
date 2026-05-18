#ifndef MODEM_SERVICE_H
#define MODEM_SERVICE_H

#include <Arduino.h>
#include "Modem.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

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
  char responsePreview[4096] = "";
};

using ModemJobCallback = void (*)(const ModemJobResult &result, void *context);

class ModemService
{
public:
  static constexpr uint8_t QueueDepth = 4;
  static constexpr uint8_t HistoryDepth = 10;
  static constexpr uint8_t HttpMaxAttempts = 3;
  static constexpr uint32_t HttpRetryDelayMs = 3000;
  static constexpr uint32_t IdlePowerOffMs = 5UL * 60UL * 1000UL;
  static constexpr size_t MaxResponseBytes = sizeof(ModemJobResult::responsePreview);

  explicit ModemService(Modem &driver) : _driver(driver) {}

  bool begin()
  {
    if (_queue == nullptr)
    {
      _queue = xQueueCreate(QueueDepth, sizeof(Job *));
      if (_queue == nullptr)
      {
        setLastError("sin memoria para cola modem");
        setState(ModemServiceState::Error);
        return false;
      }
    }

    if (_mutex == nullptr)
    {
      _mutex = xSemaphoreCreateMutex();
      if (_mutex == nullptr)
      {
        setLastError("sin memoria para mutex modem");
        setState(ModemServiceState::Error);
        return false;
      }
    }

    bool createdWorker = false;
    if (_worker == nullptr)
    {
      BaseType_t created = xTaskCreatePinnedToCore(
        workerEntry,
        "ModemService",
        8192,
        this,
        1,
        &_worker,
        0);

      if (created != pdPASS)
      {
        _worker = nullptr;
        setLastError("sin memoria para tarea modem");
        setState(ModemServiceState::Error);
        return false;
      }
      createdWorker = true;
    }

    if (createdWorker) setState(ModemServiceState::Off);
    setLastError("OK");
    return true;
  }

  ModemJobId enqueueHttpGet(const char *url, const char *tag, ModemJobCallback callback, void *context = nullptr)
  {
    return enqueueJob(ModemJobKind::HttpGet, url, nullptr, 0, tag, callback, context);
  }

  ModemJobId enqueueHttpPost(const char *url, const char *payload, const char *tag, ModemJobCallback callback, void *context = nullptr)
  {
    return enqueueJob(ModemJobKind::HttpPost, url, payload, 0, tag, callback, context);
  }

  ModemJobId enqueueAtCommand(const char *command, uint32_t timeoutMs, const char *tag, ModemJobCallback callback, void *context = nullptr)
  {
    return enqueueJob(ModemJobKind::AtCommand, command, nullptr, timeoutMs, tag, callback, context);
  }

  void shutdownNow()
  {
    _driver.end();
    setState(ModemServiceState::Off);
  }

  ModemServiceState state() const
  {
    return _state;
  }

  const char *lastErrorText() const
  {
    return _lastError;
  }

  uint32_t pendingCount() const
  {
    return _queue != nullptr ? (uint32_t)uxQueueMessagesWaiting(_queue) : 0;
  }

  void printStatus(Print &out)
  {
    out.printf("state=%s active=", stateName(_state));
    if (_activeId == 0)
    {
      out.print("none");
    }
    else
    {
      out.printf("#%lu %s tag=%s", (unsigned long)_activeId, kindName(_activeKind), _activeTag);
    }
    out.printf(" pending=%lu last_error=%s\n", (unsigned long)pendingCount(), _lastError);

    out.println("Ultimos resultados:");
    uint8_t count = _historyCount < HistoryDepth ? _historyCount : HistoryDepth;
    if (count == 0)
    {
      out.println("  (sin resultados)");
      return;
    }

    for (uint8_t i = 0; i < count; ++i)
    {
      uint8_t idx = (_historyNext + HistoryDepth - count + i) % HistoryDepth;
      const ModemJobResult &r = _history[idx];
      out.printf("  #%lu %s %s tag=%s attempts=%u bytes=%u time=%lu ms error=%s\n",
                 (unsigned long)r.id,
                 kindName(r.kind),
                 statusName(r.status),
                 r.tag,
                 (unsigned int)r.attempts,
                 (unsigned int)r.bytes,
                 (unsigned long)r.durationMs,
                 r.errorText);
    }
  }

private:
  struct Job
  {
    ModemJobId id = 0;
    ModemJobKind kind = ModemJobKind::HttpGet;
    uint32_t timeoutMs = 0;
    char primary[512] = "";
    char payload[2048] = "";
    char tag[32] = "";
    ModemJobCallback callback = nullptr;
    void *context = nullptr;
  };

  Modem &_driver;
  QueueHandle_t _queue = nullptr;
  TaskHandle_t _worker = nullptr;
  SemaphoreHandle_t _mutex = nullptr;
  volatile ModemServiceState _state = ModemServiceState::Off;
  volatile ModemJobId _nextId = 1;
  volatile ModemJobId _activeId = 0;
  ModemJobKind _activeKind = ModemJobKind::HttpGet;
  char _activeTag[32] = "";
  char _lastError[96] = "OK";
  ModemJobResult _history[HistoryDepth];
  uint8_t _historyNext = 0;
  uint8_t _historyCount = 0;

  ModemJobId enqueueJob(ModemJobKind kind,
                        const char *primary,
                        const char *payload,
                        uint32_t timeoutMs,
                        const char *tag,
                        ModemJobCallback callback,
                        void *context)
  {
    if (!begin()) return 0;
    if (primary == nullptr || primary[0] == '\0')
    {
      setLastError("job modem invalido");
      return 0;
    }

    Job *job = new Job();
    if (job == nullptr)
    {
      setLastError("sin memoria para job modem");
      return 0;
    }

    job->id = nextId();
    job->kind = kind;
    job->timeoutMs = timeoutMs;
    copyText(job->primary, sizeof(job->primary), primary);
    copyText(job->payload, sizeof(job->payload), payload != nullptr ? payload : "");
    copyText(job->tag, sizeof(job->tag), tag != nullptr ? tag : "");
    job->callback = callback;
    job->context = context;

    if (xQueueSend(_queue, &job, 0) != pdTRUE)
    {
      delete job;
      setLastError("cola modem llena");
      return 0;
    }

    setState(ModemServiceState::IdleWaitingPowerOff);
    return job->id;
  }

  ModemJobId nextId()
  {
    ModemJobId id = _nextId++;
    if (id == 0) id = _nextId++;
    return id;
  }

  static void workerEntry(void *arg)
  {
    static_cast<ModemService *>(arg)->workerLoop();
  }

  void workerLoop()
  {
    Job *job = nullptr;
    while (true)
    {
      if (xQueueReceive(_queue, &job, pdMS_TO_TICKS(IdlePowerOffMs)) == pdTRUE && job != nullptr)
      {
        processJob(job);
        delete job;
        job = nullptr;
        continue;
      }

      if (_activeId == 0 && pendingCount() == 0 && _state != ModemServiceState::Off)
      {
        _driver.end();
        setState(ModemServiceState::Off);
      }
    }
  }

  void processJob(Job *job)
  {
    ModemJobResult result;
    result.id = job->id;
    result.kind = job->kind;
    result.status = ModemJobStatus::Running;
    copyText(result.tag, sizeof(result.tag), job->tag);

    setActive(job);
    setState(ModemServiceState::Busy);

    uint32_t start = millis();
    bool ok = false;

    if (job->kind == ModemJobKind::AtCommand)
    {
      String response;
      result.attempts = 1;
      ok = _driver.rawAT(job->primary, response, job->timeoutMs > 0 ? job->timeoutMs : 5000);
      copyText(result.responsePreview, sizeof(result.responsePreview), response.c_str());
      result.bytes = response.length();
      result.httpStatus = 0;
      copyText(result.errorText, sizeof(result.errorText), ok ? "OK" : _driver.lastErrorText());
    }
    else
    {
      char *response = (char *)malloc(MaxResponseBytes);
      if (response == nullptr)
      {
        ok = false;
        result.attempts = 0;
        copyText(result.errorText, sizeof(result.errorText), "sin memoria para respuesta modem");
      }
      else
      {
        response[0] = '\0';
        for (uint8_t attempt = 1; attempt <= HttpMaxAttempts; ++attempt)
        {
          result.attempts = attempt;
          setState(ModemServiceState::Booting);
          ok = job->kind == ModemJobKind::HttpGet
                 ? _driver.executeHttpGet(job->primary, response, MaxResponseBytes)
                 : _driver.executeHttpPost(job->primary, job->payload, response, MaxResponseBytes);

          if (ok) break;
          copyText(result.errorText, sizeof(result.errorText), _driver.lastErrorText());
          if (attempt < HttpMaxAttempts) vTaskDelay(pdMS_TO_TICKS(HttpRetryDelayMs));
        }

        copyText(result.responsePreview, sizeof(result.responsePreview), response);
        result.bytes = strlen(response);
        result.httpStatus = ok ? 200 : 0;
        if (ok) copyText(result.errorText, sizeof(result.errorText), "OK");
        free(response);
      }
    }

    result.durationMs = millis() - start;
    result.status = ok ? ModemJobStatus::Succeeded : ModemJobStatus::Failed;
    if (!ok && result.errorText[0] == '\0') copyText(result.errorText, sizeof(result.errorText), _driver.lastErrorText());

    setLastError(result.errorText);
    pushHistory(result);
    clearActive();
    setState(ok ? ModemServiceState::IdleWaitingPowerOff : ModemServiceState::Error);

    if (job->callback != nullptr)
    {
      job->callback(result, job->context);
    }
  }

  void setActive(const Job *job)
  {
    _activeId = job->id;
    _activeKind = job->kind;
    copyText(_activeTag, sizeof(_activeTag), job->tag);
  }

  void clearActive()
  {
    _activeId = 0;
    _activeTag[0] = '\0';
  }

  void pushHistory(const ModemJobResult &result)
  {
    _history[_historyNext] = result;
    _historyNext = (_historyNext + 1) % HistoryDepth;
    if (_historyCount < HistoryDepth) _historyCount++;
  }

  void setState(ModemServiceState state)
  {
    _state = state;
  }

  void setLastError(const char *message)
  {
    copyText(_lastError, sizeof(_lastError), message != nullptr ? message : "error desconocido");
  }

  static void copyText(char *dest, size_t size, const char *src)
  {
    if (dest == nullptr || size == 0) return;
    if (src == nullptr) src = "";
    strncpy(dest, src, size - 1);
    dest[size - 1] = '\0';
  }

  static const char *kindName(ModemJobKind kind)
  {
    switch (kind)
    {
    case ModemJobKind::HttpGet: return "HttpGet";
    case ModemJobKind::HttpPost: return "HttpPost";
    case ModemJobKind::AtCommand: return "AtCommand";
    }
    return "?";
  }

  static const char *statusName(ModemJobStatus status)
  {
    switch (status)
    {
    case ModemJobStatus::Queued: return "Queued";
    case ModemJobStatus::Running: return "Running";
    case ModemJobStatus::Succeeded: return "Succeeded";
    case ModemJobStatus::Failed: return "Failed";
    case ModemJobStatus::Canceled: return "Canceled";
    }
    return "?";
  }

  static const char *stateName(ModemServiceState state)
  {
    switch (state)
    {
    case ModemServiceState::Off: return "Off";
    case ModemServiceState::Booting: return "Booting";
    case ModemServiceState::AtReady: return "AtReady";
    case ModemServiceState::SimReady: return "SimReady";
    case ModemServiceState::Registered: return "Registered";
    case ModemServiceState::DataReady: return "DataReady";
    case ModemServiceState::Busy: return "Busy";
    case ModemServiceState::IdleWaitingPowerOff: return "IdleWaitingPowerOff";
    case ModemServiceState::Error: return "Error";
    }
    return "?";
  }
};

#endif
