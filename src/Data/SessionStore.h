#ifndef EOLO_SESSION_STORE_H
#define EOLO_SESSION_STORE_H

#include "Session.h"
#include "../Board/RTCManager.h"
#include <Preferences.h>

class SessionStore
{
    Preferences preferences;

public:
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
    preferences.begin("eolo_session", false);

    preferences.putULong("startDate", session.startDate.unixtime());
    preferences.putUInt("duration", session.duration);
    preferences.putULong("elapsedTime", session.elapsedTime);
    preferences.putFloat("targetFlow", session.targetFlow);
    preferences.putFloat("capturedVol", session.capturedVolume);
    preferences.putBool("usePlantower", session.usePlantower);

    preferences.end();

    LOG_LN("Sesión guardada en Flash:");
    LOG_OUT(" startDate: ");
    LOG_OUT_LN(session.startDate.timestamp());
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
    preferences.begin("eolo_session", false);

    if (!preferences.isKey("startDate"))
    {
        preferences.end();
        LOG_LN("No se encontró sesión guardada en Flash");
        return false;
    }

    uint32_t startUnix = preferences.getULong("startDate", 0);
    uint32_t durationVal = preferences.getUInt("duration", 0);
    unsigned long elapsedTime = preferences.getULong("elapsedTime", 0);
    float targetFlowVal = preferences.getFloat("targetFlow", 5.0f);
    float capturedVolumeVal = preferences.getFloat("capturedVol", 0.0f);
    bool usePlantowerVal = preferences.getBool("usePlantower", true);

    preferences.end();

    session.startDate = DateTime(startUnix);
    session.duration = durationVal;
    session.elapsedTime = 0;

    if (session.startDate < rtc.now())
    {
        session.startDate = rtc.now();
        session.duration = durationVal - elapsedTime;
        session.elapsedTime = 0;

        LOG_OUT("Tiempo transcurrido restaurado: ");
        LOG_OUT(elapsedTime);
        LOG_OUT_LN("s");
    }

    session.targetFlow = targetFlowVal;
    session.capturedVolume = capturedVolumeVal;
    session.usePlantower = usePlantowerVal;

    LOG_LN("Sesión cargada desde Flash:");
    LOG_OUT(" startDate: ");
    LOG_OUT_LN(session.startDate.timestamp());
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
    preferences.begin("eolo_session", false);
    bool hasSession = preferences.isKey("startDate");
    preferences.end();
    return hasSession;
}

inline void SessionStore::clear()
{
    preferences.begin("eolo_session", false);
    preferences.clear();
    preferences.end();
    LOG_LN("Sesión eliminada de Flash");
}

#endif

#endif
