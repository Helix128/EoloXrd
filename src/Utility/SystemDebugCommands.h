#ifndef SYSTEM_DEBUG_COMMANDS_H
#define SYSTEM_DEBUG_COMMANDS_H

#include <Arduino.h>
#include "DebugCommandRouter.h"
#include "Profiler.h"
#include "RS485Monitor.h"

class SystemDebugCommands : public ConsoleCommandHandler {
private:
    bool _verboseAlerts = PROFILE_VERBOSE;
    bool _modemCommandsAvailable;

    void printHelp(Print& out) {
        out.println("Comandos de depuración:");
        out.println("  h/?  ayuda");
        out.println("  p    resumen del profiler");
        out.println("  r    reiniciar el profiler y las estadísticas de RS485");
        out.println("  v    alternar alertas verbosas del profiler/RS485");
        if (_modemCommandsAvailable) {
            out.println("  m help              ayuda modem");
        } else {
            out.println("  m ... modem no compilado en este firmware");
        }
    }

public:
    SystemDebugCommands(bool modemCommandsAvailable) : _modemCommandsAvailable(modemCommandsAvailable) {}

    bool handle(const String& line, Print& out) {
        if (line == "h" || line == "?") {
            printHelp(out);
            return true;
        }

        if (line == "p") {
            ProfilerRegistry::getInstance().printSummary(true);
            return true;
        }

        if (line == "r") {
            ProfilerRegistry::getInstance().reset();
            RS485Monitor::getInstance().reset();
            out.println("Estadísticas de depuración reiniciadas");
            return true;
        }

        if (line == "v") {
            _verboseAlerts = !_verboseAlerts;
            ProfilerRegistry::getInstance().setVerboseAlerts(_verboseAlerts);
            RS485Monitor::getInstance().setVerboseAlerts(_verboseAlerts);
            out.printf("Alertas verbosas %s\n", _verboseAlerts ? "activadas" : "desactivadas");
            return true;
        }

        return false;
    }
};

#endif
