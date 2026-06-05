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

#endif
