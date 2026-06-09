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
