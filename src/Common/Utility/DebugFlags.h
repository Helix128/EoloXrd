#ifndef EOLO_DEBUG_FLAGS_H
#define EOLO_DEBUG_FLAGS_H

namespace EoloDebug
{
inline bool &verboseLogsRef()
{
    static bool enabled = false;
    return enabled;
}

inline bool verboseLogsEnabled()
{
    return verboseLogsRef();
}

inline void setVerboseLogsEnabled(bool enabled)
{
    verboseLogsRef() = enabled;
}

inline void toggleVerboseLogs()
{
    verboseLogsRef() = !verboseLogsRef();
}
} // namespace EoloDebug

#endif // EOLO_DEBUG_FLAGS_H
