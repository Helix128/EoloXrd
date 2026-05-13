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
        out.println("  h/?/help          ayuda");
        out.println("  p/profile         resumen del profiler");
        out.println("  r/reset           reiniciar el profiler y las estadísticas de RS485");
        out.println("  v/verbose         alternar alertas verbosas del profiler/RS485");
        out.println("  !                 entrar al terminal silencioso");
        out.println("  exit              salir del terminal silencioso");
        if (_modemCommandsAvailable) {
            out.println("  m help            ayuda modem (solo terminal)");
        } else {
            out.println("  m ... modem no compilado en este firmware");
        }
    }

public:
    SystemDebugCommands(bool modemCommandsAvailable) : _modemCommandsAvailable(modemCommandsAvailable) {}

    bool handle(const String& line, Print& out) {
        if (line == "h" || line == "?" || line == "help") {
            printHelp(out);
            return true;
        }

        if (line == "p" || line == "profile") {
            ProfilerRegistry::getInstance().printSummary(out, true);
            return true;
        }

        if (line == "r" || line == "reset") {
            ProfilerRegistry::getInstance().reset();
            RS485Monitor::getInstance().reset();
            out.println("Estadísticas de depuración reiniciadas");
            return true;
        }

        if (line == "v" || line == "verbose") {
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
