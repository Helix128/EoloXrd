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
  char responsePreview[256] = "";
};

using ModemJobCallback = void (*)(const ModemJobResult &result, void *context);

class ModemService
{
public:
  static constexpr uint8_t QueueDepth = 4;
  static constexpr uint8_t HistoryDepth = 4;
  static constexpr uint8_t HttpMaxAttempts = 3;
  static constexpr uint32_t HttpRetryDelayMs = 3000;
  static constexpr uint32_t IdlePowerOffMs = 2UL * 60UL * 1000UL;
  static constexpr size_t MaxResponseBytes = sizeof(ModemJobResult::responsePreview);
  static constexpr size_t HttpResponseBytes = 4096;

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

  void warmUp()
  {
    if (_activeId != 0 || pendingCount() > 0) return;
    enqueueAtCommand("AT+CSQ", 5000, "modem-warmup", nullptr);
  }

  void shutdownNow()
  {
    _driver.end();
    _shutdownWhenIdle = false;
    clearSignalQuality();
    setState(ModemServiceState::Off);
  }

  void shutdownWhenIdle()
  {
    _shutdownWhenIdle = true;
    if (_activeId == 0 && pendingCount() == 0)
    {
      _driver.end();
      _shutdownWhenIdle = false;
      clearSignalQuality();
      setState(ModemServiceState::Off);
    }
  }

  ModemServiceState state() const
  {
    if (!takeMutex(pdMS_TO_TICKS(50))) return _state;
    ModemServiceState state = _state;
    giveMutex();
    return state;
  }

  bool hasSignalQuality() const
  {
    if (!takeMutex(pdMS_TO_TICKS(50))) return _signalKnown;
    bool known = _signalKnown;
    giveMutex();
    return known;
  }

  uint8_t signalQualityCsq() const
  {
    if (!takeMutex(pdMS_TO_TICKS(50))) return _signalCsq;
    uint8_t csq = _signalCsq;
    giveMutex();
    return csq;
  }

  uint8_t signalQualityBars() const
  {
    if (!_signalKnown || _signalCsq == 99) return 0;
    if (_signalCsq >= 20) return 4;
    if (_signalCsq >= 14) return 3;
    if (_signalCsq >= 8) return 2;
    if (_signalCsq >= 2) return 1;
    return 0;
  }

  uint32_t signalQualityAgeMs() const
  {
    if (!takeMutex(pdMS_TO_TICKS(50))) return UINT32_MAX;
    uint32_t age = _signalKnown ? millis() - _signalUpdatedAtMs : UINT32_MAX;
    giveMutex();
    return age;
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
  mutable SemaphoreHandle_t _mutex = nullptr;
  ModemServiceState _state = ModemServiceState::Off;
  ModemJobId _nextId = 1;
  ModemJobId _activeId = 0;
  ModemJobKind _activeKind = ModemJobKind::HttpGet;
  char _activeTag[32] = "";
  char _lastError[96] = "OK";
  bool _shutdownWhenIdle = false;
  bool _signalKnown = false;
  uint8_t _signalCsq = 99;
  uint32_t _signalUpdatedAtMs = 0;
  Job _jobPool[QueueDepth];
  bool _jobUsed[QueueDepth]{};
  char _responseBuffer[HttpResponseBytes] = "";
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

    Job *job = allocJob();
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
      freeJob(job);
      setLastError("cola modem llena");
      return 0;
    }

    setState(ModemServiceState::IdleWaitingPowerOff);
    _shutdownWhenIdle = false;
    return job->id;
  }

  ModemJobId nextId()
  {
    if (!takeMutex(portMAX_DELAY)) return 0;
    ModemJobId id = _nextId++;
    if (id == 0) id = _nextId++;
    giveMutex();
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
        freeJob(job);
        job = nullptr;
        continue;
      }

      if (_activeId == 0 && pendingCount() == 0 && _state != ModemServiceState::Off)
      {
        _driver.end();
        _shutdownWhenIdle = false;
        clearSignalQuality();
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
      result.attempts = 1;
      ok = _driver.rawAT(job->primary, _responseBuffer, HttpResponseBytes, job->timeoutMs > 0 ? job->timeoutMs : 5000);
      copyText(result.responsePreview, sizeof(result.responsePreview), _responseBuffer);
      result.bytes = strlen(_responseBuffer);
      result.httpStatus = 0;
      copyText(result.errorText, sizeof(result.errorText), ok ? "OK" : _driver.lastErrorText());
      if (ok) updateSignalFromResponse(_responseBuffer);
    }
    else
    {
      char *response = _responseBuffer;
      {
        response[0] = '\0';
        for (uint8_t attempt = 1; attempt <= HttpMaxAttempts; ++attempt)
        {
          result.attempts = attempt;
          setState(ModemServiceState::Booting);
          ok = job->kind == ModemJobKind::HttpGet
                 ? _driver.executeHttpGet(job->primary, response, HttpResponseBytes)
                 : _driver.executeHttpPost(job->primary, job->payload, response, HttpResponseBytes);

          if (ok) break;
          copyText(result.errorText, sizeof(result.errorText), _driver.lastErrorText());
          if (attempt < HttpMaxAttempts) vTaskDelay(pdMS_TO_TICKS(HttpRetryDelayMs));
        }

        copyText(result.responsePreview, sizeof(result.responsePreview), response);
        result.bytes = strlen(response);
        result.httpStatus = ok ? 200 : 0;
        if (ok) copyText(result.errorText, sizeof(result.errorText), "OK");
      }
      refreshSignalQuality();
    }

    result.durationMs = millis() - start;
    result.status = ok ? ModemJobStatus::Succeeded : ModemJobStatus::Failed;
    if (!ok && result.errorText[0] == '\0') copyText(result.errorText, sizeof(result.errorText), _driver.lastErrorText());

    setLastError(result.errorText);
    pushHistory(result);
    clearActive();
    setState(ok ? ModemServiceState::IdleWaitingPowerOff : ModemServiceState::Error);

    if (_shutdownWhenIdle && pendingCount() == 0)
    {
      _driver.end();
      _shutdownWhenIdle = false;
      clearSignalQuality();
      setState(ModemServiceState::Off);
    }

    if (job->callback != nullptr)
    {
      job->callback(result, job->context);
    }
  }

  void setActive(const Job *job)
  {
    if (!takeMutex(portMAX_DELAY)) return;
    _activeId = job->id;
    _activeKind = job->kind;
    copyText(_activeTag, sizeof(_activeTag), job->tag);
    giveMutex();
  }

  void clearActive()
  {
    if (!takeMutex(portMAX_DELAY)) return;
    _activeId = 0;
    _activeTag[0] = '\0';
    giveMutex();
  }

  void pushHistory(const ModemJobResult &result)
  {
    if (!takeMutex(portMAX_DELAY)) return;
    _history[_historyNext] = result;
    _historyNext = (_historyNext + 1) % HistoryDepth;
    if (_historyCount < HistoryDepth) _historyCount++;
    giveMutex();
  }

  void setState(ModemServiceState state)
  {
    if (!takeMutex(portMAX_DELAY))
    {
      _state = state;
      return;
    }
    _state = state;
    giveMutex();
  }

  void setLastError(const char *message)
  {
    if (!takeMutex(portMAX_DELAY))
    {
      copyText(_lastError, sizeof(_lastError), message != nullptr ? message : "error desconocido");
      return;
    }
    copyText(_lastError, sizeof(_lastError), message != nullptr ? message : "error desconocido");
    giveMutex();
  }

  void refreshSignalQuality()
  {
    if (_driver.rawAT("AT+CSQ", _responseBuffer, HttpResponseBytes, 5000))
    {
      updateSignalFromResponse(_responseBuffer);
    }
  }

  void updateSignalFromResponse(const char *response)
  {
    int csq = parseCsq(response);
    if (csq < 0) return;

    _signalCsq = (uint8_t)csq;
    _signalKnown = csq != 99;
    _signalUpdatedAtMs = millis();
  }

  void clearSignalQuality()
  {
    _signalKnown = false;
    _signalCsq = 99;
    _signalUpdatedAtMs = 0;
  }

  Job *allocJob()
  {
    if (!takeMutex(portMAX_DELAY)) return nullptr;
    for (uint8_t i = 0; i < QueueDepth; ++i)
    {
      if (!_jobUsed[i])
      {
        _jobUsed[i] = true;
        memset(&_jobPool[i], 0, sizeof(Job));
        giveMutex();
        return &_jobPool[i];
      }
    }
    giveMutex();
    return nullptr;
  }

  void freeJob(Job *job)
  {
    if (job == nullptr) return;
    if (!takeMutex(portMAX_DELAY)) return;
    if (job >= _jobPool && job < _jobPool + QueueDepth)
    {
      _jobUsed[job - _jobPool] = false;
    }
    giveMutex();
  }

  bool takeMutex(TickType_t timeout) const
  {
    return _mutex == nullptr || xSemaphoreTake(_mutex, timeout) == pdTRUE;
  }

  void giveMutex() const
  {
    if (_mutex != nullptr) xSemaphoreGive(_mutex);
  }

  static int parseCsq(const char *response)
  {
    if (response == nullptr) return -1;

    const char *pos = strstr(response, "+CSQ:");
    if (pos == nullptr) return -1;
    pos += 5;

    while (*pos == ' ' || *pos == '\t') pos++;
    if (*pos < '0' || *pos > '9') return -1;

    int value = 0;
    while (*pos >= '0' && *pos <= '9')
    {
      value = value * 10 + (*pos - '0');
      pos++;
    }

    if (value < 0 || value > 99) return -1;
    return value;
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
