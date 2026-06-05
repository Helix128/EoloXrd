#ifndef CAPTURE_SWITCH_DEBUG_COMMANDS_H
#define CAPTURE_SWITCH_DEBUG_COMMANDS_H

#include <Arduino.h>
#include <string.h>
#include "DebugCommandRouter.h"
#include "../Board/CaptureSwitches.h"

class CaptureSwitchDebugCommands : public ConsoleCommandHandler {
private:
    CaptureSwitches* _switches = nullptr;

    void printHelp(Print& out) {
        out.println("Comandos switches:");
        out.println("  switches       resumen de switches de captura");
        out.println("  switches -v    detalle de pines y codigos");
        out.println("  sw             alias de switches");
        out.println("  switches help  mostrar esta ayuda");
    }

public:
    void attachCaptureSwitches(CaptureSwitches* switches) {
        _switches = switches;
    }

    bool handle(const String& line, Print& out) override {
        if (!(line == "switches" || line.startsWith("switches ") || line == "sw")) {
            return false;
        }

        String args;
        if (line.startsWith("switches ")) {
            args = line.substring(strlen("switches "));
            args.trim();
        }

        if (args == "help" || args == "?") {
            printHelp(out);
            return true;
        }

        bool verbose = args == "-v" || args == "verbose" || args == "--verbose";
        if (args.length() > 0 && !verbose) {
            out.println("Comando switches desconocido. Usa: switches help");
            return true;
        }

        if (_switches == nullptr) {
            out.println("Switches de captura no adjuntados.");
            return true;
        }

        CaptureSwitchSnapshot snap = _switches->snapshot();
        if (verbose)
            CaptureSwitches::printSnapshot(out, snap);
        else
            CaptureSwitches::printSummary(out, snap);
        return true;
    }
};

#endif
