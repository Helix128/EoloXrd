#ifndef SYSTEM_DEBUG_COMMANDS_H
#define SYSTEM_DEBUG_COMMANDS_H

#include <Arduino.h>
#include <stdlib.h>
#include "DebugCommandRouter.h"
#include "DebugFlags.h"
#include "Profiler.h"
#include "RS485Monitor.h"
#include "../Board/RTCManager.h"

class SystemDebugCommands : public ConsoleCommandHandler {
private:
    bool _modemCommandsAvailable;
    RTCManager* _rtc = nullptr;

    void printHelp(Print& out) {
        out.println("Comandos de depuración:");
        out.println("  h/?/help          ayuda");
        out.println("  p/profile         resumen del profiler");
        out.println("  r/reset           reiniciar el profiler y las estadísticas de RS485");
        out.println("  v/verbose [on|off|status|toggle]  alternar logs verbosos y alertas");
        out.println("  time              mostrar hora RTC");
        out.println("  time set YYYY-MM-DD HH:MM:SS");
        out.println("  time epoch <unix_utc> <offset_seconds>");
#if defined(FEATURE_HEADLESS) && defined(EOLO_TARGET_DRON)
        out.println("  switches/sw       estado de switches de captura");
        out.println("  drone status      diagnostico compacto EOLO Dron");
#endif
        out.println("  !                 entrar al terminal silencioso");
        out.println("  exit              salir del terminal silencioso");
        if (_modemCommandsAvailable) {
            out.println("  m help            ayuda modem (solo terminal)");
        } else {
            out.println("  m ... modem no compilado en este firmware");
        }
    }

    bool parseUint32(const String& text, uint32_t& value) const {
        if (text.length() == 0)
            return false;

        char* end = nullptr;
        unsigned long parsed = strtoul(text.c_str(), &end, 10);
        if (end == text.c_str() || *end != '\0')
            return false;

        value = (uint32_t)parsed;
        return parsed <= UINT32_MAX;
    }

    bool parseInt32(const String& text, int32_t& value) const {
        if (text.length() == 0)
            return false;

        char* end = nullptr;
        long parsed = strtol(text.c_str(), &end, 10);
        if (end == text.c_str() || *end != '\0')
            return false;

        value = (int32_t)parsed;
        return parsed >= INT32_MIN && parsed <= INT32_MAX;
    }

    void handleTimeCommand(const String& args, Print& out) {
        if (_rtc == nullptr) {
            out.println("RTC no disponible en esta consola");
            return;
        }

        if (args.length() == 0 || args == "show") {
            out.println(_rtc->getTimeString());
            return;
        }

        if (args.startsWith("set ")) {
            String value = args.substring(strlen("set "));
            value.trim();

            DateTime parsed;
            if (!RTCManager::parseDateTimeString(value.c_str(), parsed)) {
                out.println("Uso: time set YYYY-MM-DD HH:MM:SS");
                return;
            }

            if (_rtc->adjust(parsed)) {
                out.printf("RTC ajustado: %s\n", _rtc->getTimeString().c_str());
            } else {
                out.println("No se pudo ajustar RTC");
            }
            return;
        }

        if (args.startsWith("epoch ")) {
            String rest = args.substring(strlen("epoch "));
            rest.trim();
            int sep = rest.indexOf(' ');
            if (sep <= 0) {
                out.println("Uso: time epoch <unix_utc> <offset_seconds>");
                return;
            }

            String unixText = rest.substring(0, sep);
            String offsetText = rest.substring(sep + 1);
            unixText.trim();
            offsetText.trim();

            uint32_t unixUtc = 0;
            int32_t offsetSeconds = 0;
            DateTime parsed;
            if (!parseUint32(unixText, unixUtc) ||
                !parseInt32(offsetText, offsetSeconds) ||
                !RTCManager::fromUnixWithOffset(unixUtc, offsetSeconds, parsed)) {
                out.println("Uso: time epoch <unix_utc> <offset_seconds>");
                return;
            }

            if (_rtc->adjust(parsed)) {
                out.printf("RTC ajustado: %s\n", _rtc->getTimeString().c_str());
            } else {
                out.println("No se pudo ajustar RTC");
            }
            return;
        }

        out.println("Comando time desconocido. Usa: time, time set ... o time epoch ...");
    }

    void handleVerboseCommand(const String& args, Print& out) {
        bool enabled = EoloDebug::verboseLogsEnabled();

        if (args.length() == 0 || args == "toggle") {
            enabled = !enabled;
        } else if (args == "on" || args == "1" || args == "yes") {
            enabled = true;
        } else if (args == "off" || args == "0" || args == "no") {
            enabled = false;
        } else if (args == "status") {
            out.printf("Verbose %s\n", enabled ? "activado" : "desactivado");
            return;
        } else {
            out.println("Uso: verbose [on|off|status]");
            return;
        }

        EoloDebug::setVerboseLogsEnabled(enabled);
        ProfilerRegistry::getInstance().setVerboseAlerts(enabled);
        RS485Monitor::getInstance().setVerboseAlerts(enabled);
        out.printf("Verbose %s\n", enabled ? "activado" : "desactivado");
    }

public:
    SystemDebugCommands(bool modemCommandsAvailable) : _modemCommandsAvailable(modemCommandsAvailable) {}

    void attachRTC(RTCManager* rtc) {
        _rtc = rtc;
    }

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

        if (line == "v" || line == "verbose" || line.startsWith("v ") || line.startsWith("verbose ")) {
            String args = String("");
            int space = line.indexOf(' ');
            if (space >= 0) {
                args = line.substring(space + 1);
                args.trim();
            }
            handleVerboseCommand(args, out);
            return true;
        }

        if (line == "time" || line.startsWith("time ")) {
            String args = line == "time" ? String("") : line.substring(strlen("time "));
            args.trim();
            handleTimeCommand(args, out);
            return true;
        }

        return false;
    }
};

#endif
