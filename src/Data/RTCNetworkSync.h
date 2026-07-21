#ifndef EOLO_RTC_NETWORK_SYNC_H
#define EOLO_RTC_NETWORK_SYNC_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <atomic>
#include <stddef.h>
#include <stdint.h>
#ifdef FEATURE_MODEM
#include "../Board/ModemService.h"
#endif

class ModemService;
class RTCManager;

enum class RTCNetworkSyncStatus : uint8_t
{
    Idle,
    Running,
    Success,
    Failed
};

class RTCNetworkSync
{
    static constexpr size_t MaxSourceUrl = 512;

    struct Request
    {
        ModemService *modem = nullptr;
        RTCManager *rtc = nullptr;
        char sourceUrl[MaxSourceUrl] = "";
    } request;

    TaskHandle_t taskHandle = nullptr;
    // Written by RTC sync callback/worker, read by main loop/UI.
    std::atomic<RTCNetworkSyncStatus> status{RTCNetworkSyncStatus::Idle};

    static void worker(void *arg);

#ifdef FEATURE_MODEM
    static void timeSyncCallback(const ModemJobResult &result, void *context);
#endif

public:
    // El servicio sólo conoce sus dos adaptadores físicos. Context conserva
    // wrappers de aplicación, pero no entra en la cola ni en el callback.
    bool syncFromTimeServer(ModemService &modem, RTCManager &rtc, const char *url);
    bool start(ModemService &modem, RTCManager &rtc, const char *url);
    RTCNetworkSyncStatus getStatus() const { return status.load(); }
};

#endif // EOLO_RTC_NETWORK_SYNC_H


#if !defined(EOLO_RTC_NETWORK_SYNC_IMPL_DONE)
#define EOLO_RTC_NETWORK_SYNC_IMPL_DONE

#include "../Config/Legacy.h"
#include "../Board/RTCManager.h"
#include <string.h>

#ifdef FEATURE_MODEM
inline void RTCNetworkSync::timeSyncCallback(const ModemJobResult &result, void *context)
{
    RTCNetworkSync *self = static_cast<RTCNetworkSync *>(context);
    if (self == nullptr) return;

    bool ok = result.status == ModemJobStatus::Succeeded &&
              self->request.rtc != nullptr &&
              self->request.rtc->applyTimeServerResponse(result.responsePreview,
                                                         self->request.sourceUrl);

    self->status = ok ? RTCNetworkSyncStatus::Success : RTCNetworkSyncStatus::Failed;
    if (!ok)
        LOG_F("Sincronizacion RTC fallida (%s)\n", result.errorText);
}
#endif

inline bool RTCNetworkSync::syncFromTimeServer(ModemService &modem,
                                                RTCManager &rtc,
                                                const char *url)
{
#ifdef FEATURE_MODEM
    const char *sourceUrl = (url != nullptr && url[0] != '\0')
                                ? url
                                : RTCManager::DefaultTimeServerUrl;
    request.modem = &modem;
    request.rtc = &rtc;
    if (sourceUrl != request.sourceUrl)
    {
        strncpy(request.sourceUrl, sourceUrl, sizeof(request.sourceUrl) - 1);
        request.sourceUrl[sizeof(request.sourceUrl) - 1] = '\0';
    }

    status = RTCNetworkSyncStatus::Running;
    LOG_F("Encolando sincronizacion RTC desde %s...\n", sourceUrl);
    ModemJobId id = modem.enqueueHttpGet(sourceUrl, "rtc-timesync", timeSyncCallback, this);
    if (id == 0)
    {
        status = RTCNetworkSyncStatus::Failed;
        LOG_F("No se pudo encolar sincronizacion RTC (%s)\n", modem.lastErrorText());
        return false;
    }
    return true;
#else
    (void)modem;
    (void)rtc;
    (void)url;
    return false;
#endif
}

inline void RTCNetworkSync::worker(void *arg)
{
    RTCNetworkSync *self = static_cast<RTCNetworkSync *>(arg);
    bool queued = false;
#ifdef FEATURE_MODEM
    if (self != nullptr && self->request.modem != nullptr && self->request.rtc != nullptr)
        queued = self->syncFromTimeServer(*self->request.modem,
                                          *self->request.rtc,
                                          self->request.sourceUrl);
#else
    (void)self;
#endif
    if (self != nullptr && !queued)
        self->status = RTCNetworkSyncStatus::Failed;
    if (self != nullptr)
        self->taskHandle = nullptr;
    vTaskDelete(nullptr);
}

inline bool RTCNetworkSync::start(ModemService &modem,
                                  RTCManager &rtc,
                                  const char *url)
{
#ifdef FEATURE_MODEM
    if (taskHandle != nullptr || status == RTCNetworkSyncStatus::Running)
        return false;

    request.modem = &modem;
    request.rtc = &rtc;
    const char *sourceUrl = (url != nullptr && url[0] != '\0')
                                ? url
                                : RTCManager::DefaultTimeServerUrl;
    strncpy(request.sourceUrl, sourceUrl, sizeof(request.sourceUrl) - 1);
    request.sourceUrl[sizeof(request.sourceUrl) - 1] = '\0';
    status = RTCNetworkSyncStatus::Running;
    // Prio 1 (servicio de fondo): sincronizacion NTP no critica en tiempo real.
    // Stack 8192: HTTP GET + JSON parsing para la respuesta del servidor de tiempo.
    BaseType_t created = xTaskCreatePinnedToCore(
        worker,
        "EoloRTCSync",
        8192,
        this,
        1,
        &taskHandle,
        0);

    if (created != pdPASS)
    {
        taskHandle = nullptr;
        status = RTCNetworkSyncStatus::Failed;
        return false;
    }

    return true;
#else
    (void)modem;
    (void)rtc;
    (void)url;
    status = RTCNetworkSyncStatus::Failed;
    return false;
#endif
}

#endif
