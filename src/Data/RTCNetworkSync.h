#ifndef EOLO_RTC_NETWORK_SYNC_H
#define EOLO_RTC_NETWORK_SYNC_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#ifdef FEATURE_MODEM
#include "../Board/ModemService.h"
#endif

struct Context;

enum class RTCNetworkSyncStatus : uint8_t
{
    Idle,
    Running,
    Success,
    Failed
};

class RTCNetworkSync
{
    TaskHandle_t taskHandle = nullptr;
    volatile RTCNetworkSyncStatus status = RTCNetworkSyncStatus::Idle;

    static void worker(void *arg);

#ifdef FEATURE_MODEM
    static void timeSyncCallback(const ModemJobResult &result, void *context);
#endif

public:
    bool syncFromTimeServer(Context &ctx, const char *url);
    bool start(Context &ctx);
    RTCNetworkSyncStatus getStatus() const { return status; }
};

#endif // EOLO_RTC_NETWORK_SYNC_H


#if defined(CONTEXT_CLASS_DEFINED) && !defined(EOLO_RTC_NETWORK_SYNC_IMPL_DONE)
#define EOLO_RTC_NETWORK_SYNC_IMPL_DONE

#include "Context.h"
#include "../Config.h"

#ifdef FEATURE_MODEM
inline void RTCNetworkSync::timeSyncCallback(const ModemJobResult &result, void *context)
{
    Context *ctx = static_cast<Context *>(context);
    if (ctx == nullptr) return;

    bool ok = result.status == ModemJobStatus::Succeeded &&
              ctx->components.rtc.applyTimeServerResponse(result.responsePreview, RTCManager::DefaultTimeServerUrl);

    ctx->rtcSync.status = ok ? RTCNetworkSyncStatus::Success : RTCNetworkSyncStatus::Failed;
    if (!ok)
        LOG_F("Sincronizacion RTC fallida (%s)\n", result.errorText);
    ctx->markUiDirty();
}
#endif

inline bool RTCNetworkSync::syncFromTimeServer(Context &ctx, const char *url)
{
#ifdef FEATURE_MODEM
    if (url == nullptr || url[0] == '\0')
        url = RTCManager::DefaultTimeServerUrl;

    LOG_F("Encolando sincronizacion RTC desde %s...\n", url);
    ModemJobId id = ctx.components.modemService.enqueueHttpGet(url, "rtc-timesync", timeSyncCallback, &ctx);
    if (id == 0)
    {
        LOG_F("No se pudo encolar sincronizacion RTC (%s)\n", ctx.components.modemService.lastErrorText());
        return false;
    }
    return true;
#else
    (void)ctx;
    (void)url;
    return false;
#endif
}

inline void RTCNetworkSync::worker(void *arg)
{
    Context *ctx = static_cast<Context *>(arg);
    bool queued = ctx->rtcSync.syncFromTimeServer(*ctx, RTCManager::DefaultTimeServerUrl);
    if (!queued)
        ctx->rtcSync.status = RTCNetworkSyncStatus::Failed;
    ctx->markUiDirty();
    ctx->rtcSync.taskHandle = nullptr;
    vTaskDelete(nullptr);
}

inline bool RTCNetworkSync::start(Context &ctx)
{
#ifdef FEATURE_MODEM
    if (taskHandle != nullptr || status == RTCNetworkSyncStatus::Running)
        return false;

    status = RTCNetworkSyncStatus::Running;
    ctx.markUiDirty();
    // Prio 1 (servicio de fondo): sincronizacion NTP no critica en tiempo real.
    // Stack 8192: HTTP GET + JSON parsing para la respuesta del servidor de tiempo.
    BaseType_t created = xTaskCreatePinnedToCore(
        worker,
        "EoloRTCSync",
        8192,
        &ctx,
        1,
        &taskHandle,
        0);

    if (created != pdPASS)
    {
        taskHandle = nullptr;
        status = RTCNetworkSyncStatus::Failed;
        ctx.markUiDirty();
        return false;
    }

    return true;
#else
    status = RTCNetworkSyncStatus::Failed;
    ctx.markUiDirty();
    return false;
#endif
}

#endif
