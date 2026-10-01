#ifndef EOLO_SESSION_STORE_H
#define EOLO_SESSION_STORE_H

#include "Session.h"
#include "../Board/RTCManager.h"
#include "PreferencesSettingsStore.h"

class SessionStore
{
    PreferencesSettingsStore settings;

public:
    SessionStore() : settings("eolo_session") {}
    void save(const Session &session);
    bool load(Session &session, RTCManager &rtc);
    bool canLoad();
    void clear();
};

#ifndef EOLO_SESSIONSTOREIMPL_H
#define EOLO_SESSIONSTOREIMPL_H

inline void SessionStore::save(const Session &session)
{
    LOG_LN("Guardando sesión en Flash...");
    settings.putUInt("startDate", session.startUnix);
    settings.putUInt("duration", session.duration);
    settings.putUInt("elapsedTime", static_cast<uint32_t>(session.elapsedTime));
    settings.putFloat("targetFlow", session.targetFlow);
    settings.putFloat("capturedVol", session.capturedVolume);
    settings.putBool("usePlantower", session.usePlantower);
    settings.putBytes("flowSecCount", &session.flowSectionCount, sizeof(session.flowSectionCount));
    settings.putBytes("flowSections", session.flowSections, sizeof(session.flowSections));

    LOG_LN("Sesión guardada en Flash:");
    LOG_OUT(" startDate: ");
    LOG_OUT_LN(DateTime(session.startUnix).timestamp());
    LOG_OUT(" duration: ");
    LOG_OUT_LN(session.duration);
    LOG_OUT(" elapsedTime: ");
    LOG_OUT_LN(session.elapsedTime);
    LOG_OUT(" targetFlow: ");
    LOG_OUT_LN(session.targetFlow);
    LOG_OUT(" capturedVolume: ");
    LOG_OUT_LN(session.capturedVolume);
    LOG_OUT(" usePlantower: ");
    LOG_LN(session.usePlantower);
}

inline bool SessionStore::load(Session &session, RTCManager &rtc)
{
    if (!settings.contains("startDate"))
    {
        LOG_LN("No se encontró sesión guardada en Flash");
        return false;
    }

    uint32_t startUnix = settings.getUInt("startDate", 0);
    uint32_t durationVal = settings.getUInt("duration", 0);
    unsigned long elapsedTime = settings.getUInt("elapsedTime", 0);
    float targetFlowVal = settings.getFloat("targetFlow", 5.0f);
    float capturedVolumeVal = settings.getFloat("capturedVol", 0.0f);
    bool usePlantowerVal = settings.getBool("usePlantower", true);
    uint8_t flowSectionCountVal = 0;
    settings.getBytes("flowSecCount", &flowSectionCountVal, sizeof(flowSectionCountVal));
    FlowSection flowSectionsVal[MaxFlowSections];
    for (uint8_t i = 0; i < MaxFlowSections; i++)
      flowSectionsVal[i] = FlowSection();
    if (flowSectionCountVal > MaxFlowSections)
      flowSectionCountVal = 0;
    size_t sectionBytes = settings.getBytes("flowSections", flowSectionsVal, sizeof(flowSectionsVal))
                              ? sizeof(flowSectionsVal) : 0;
    if (sectionBytes != sizeof(flowSectionsVal))
      flowSectionCountVal = 0;

    session.startUnix = startUnix;
    session.duration = durationVal;
    session.elapsedTime = 0;

    const uint32_t nowUnix = rtc.now().unixtime();
    if (session.startUnix < nowUnix)
    {
        session.startUnix = nowUnix;
        session.duration = elapsedTime < durationVal ? durationVal - elapsedTime : 0;
        session.elapsedTime = 0;

        LOG_OUT("Tiempo transcurrido restaurado: ");
        LOG_OUT(elapsedTime);
        LOG_OUT_LN("s");
    }

    session.targetFlow = targetFlowVal;
    session.capturedVolume = capturedVolumeVal;
    session.usePlantower = usePlantowerVal;
    session.flowSectionCount = flowSectionCountVal;
    for (uint8_t i = 0; i < MaxFlowSections; i++)
      session.flowSections[i] = i < flowSectionCountVal ? flowSectionsVal[i] : FlowSection();

    LOG_LN("Sesión cargada desde Flash:");
    LOG_OUT(" startDate: ");
    LOG_OUT_LN(DateTime(session.startUnix).timestamp());
    LOG_OUT(" duration: ");
    LOG_OUT_LN(session.duration);
    LOG_OUT(" elapsedTime: ");
    LOG_OUT_LN(elapsedTime);
    LOG_OUT(" targetFlow: ");
    LOG_OUT_LN(session.targetFlow);
    LOG_OUT(" capturedVolume: ");
    LOG_OUT_LN(capturedVolumeVal);
    LOG_OUT(" usePlantower: ");
    LOG_OUT_LN(session.usePlantower);

    save(session);
    return true;
}

inline bool SessionStore::canLoad()
{
    return settings.contains("startDate");
}

inline void SessionStore::clear()
{
    settings.clear();
    LOG_LN("Sesión eliminada de Flash");
}

#endif

#endif
