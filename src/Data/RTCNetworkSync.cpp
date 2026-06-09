#include "RTCNetworkSync.h"
#include "Context.h"
#include "../Config.h"

#ifdef FEATURE_MODEM
void RTCNetworkSync::timeSyncCallback(const ModemJobResult &result, void *context)
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

bool RTCNetworkSync::syncFromTimeServer(Context &ctx, const char *url)
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

void RTCNetworkSync::worker(void *arg)
{
    Context *ctx = static_cast<Context *>(arg);
    bool queued = ctx->rtcSync.syncFromTimeServer(*ctx, RTCManager::DefaultTimeServerUrl);
    if (!queued)
        ctx->rtcSync.status = RTCNetworkSyncStatus::Failed;
    ctx->markUiDirty();
    ctx->rtcSync.taskHandle = nullptr;
    vTaskDelete(nullptr);
}

bool RTCNetworkSync::start(Context &ctx)
{
#ifdef FEATURE_MODEM
    if (taskHandle != nullptr || status == RTCNetworkSyncStatus::Running)
        return false;

    status = RTCNetworkSyncStatus::Running;
    ctx.markUiDirty();
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
