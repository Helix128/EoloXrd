#ifndef MODEM_DEBUG_COMMANDS_H
#define MODEM_DEBUG_COMMANDS_H

#include <Arduino.h>
#include "DebugCommandRouter.h"
#include "SerialOutput.h"
#ifdef FEATURE_MODEM
#include "../Board/ModemService.h"
#include "../Board/RTCManager.h"
#endif

class ModemDebugCommands : public ConsoleCommandHandler {
private:
#ifdef FEATURE_MODEM
    ModemService* _modem = nullptr;
    RTCManager* _rtc = nullptr;

    struct TimeSyncContext {
        RTCManager* rtc = nullptr;
        char url[128] = "";
    };
#endif

    void printHelp(Print& out) {
#ifdef FEATURE_MODEM
        out.println("Comandos modem:");
        out.println("  m begin              inicia el servicio async del modem");
        out.println("  m end                cierra red/UART desde el servicio");
        out.println("  m status             estado, cola, job activo e historial");
        out.println("  m at <AT...>         encola comando AT");
        out.println("  m get <URL>          encola HTTP GET");
        out.println("  m post <URL> <body>  encola HTTP POST");
        out.println("  m timesync [URL]     encola HTTP GET y ajusta RTC");
        out.println("  m ntp                obsoleto; usa m timesync");
        out.println("  m diag               encola diagnostico AT basico");
#else
        out.println("Modem no compilado. Usa el environment eolo_standard.");
#endif
    }

#ifdef FEATURE_MODEM
    static const char* statusName(ModemJobStatus status) {
        switch (status) {
        case ModemJobStatus::Queued: return "Queued";
        case ModemJobStatus::Running: return "Running";
        case ModemJobStatus::Succeeded: return "Succeeded";
        case ModemJobStatus::Failed: return "Failed";
        case ModemJobStatus::Canceled: return "Canceled";
        }
        return "?";
    }

    static void printAsyncResult(const ModemJobResult& result, void*) {
        cmdOut.printf("\n[m] job #%lu %s tag=%s attempts=%u bytes=%u time=%lu ms",
                      (unsigned long)result.id,
                      statusName(result.status),
                      result.tag,
                      (unsigned int)result.attempts,
                      (unsigned int)result.bytes,
                      (unsigned long)result.durationMs);
        if (result.status != ModemJobStatus::Succeeded) {
            cmdOut.printf(" error=%s\n", result.errorText);
            return;
        }

        cmdOut.println();
        if (result.responsePreview[0] != '\0') {
            cmdOut.println("--- Respuesta ---");
            cmdOut.println(result.responsePreview);
            cmdOut.println("---");
        }
    }

    static void timeSyncCallback(const ModemJobResult& result, void* context) {
        TimeSyncContext* ctx = static_cast<TimeSyncContext*>(context);
        RTCManager* rtc = ctx != nullptr ? ctx->rtc : nullptr;
        const char* url = ctx != nullptr ? ctx->url : RTCManager::DefaultTimeServerUrl;

        bool ok = result.status == ModemJobStatus::Succeeded &&
                  rtc != nullptr &&
                  rtc->applyTimeServerResponse(result.responsePreview, url);

        if (ok) {
            cmdOut.printf("\n[m] timesync OK. Hora: %s\n", rtc->getTimeString().c_str());
        } else {
            cmdOut.printf("\n[m] timesync fallo (%s)\n", result.errorText);
        }

        delete ctx;
    }

    bool enqueueAt(Print& out, const char* command, uint32_t timeoutMs, const char* tag) {
        ModemJobId id = _modem->enqueueAtCommand(command, timeoutMs, tag, printAsyncResult);
        if (id == 0) {
            out.printf("No se pudo encolar AT (%s)\n", _modem->lastErrorText());
            return false;
        }
        out.printf("AT encolado job #%lu\n", (unsigned long)id);
        return true;
    }

    void runDiag(Print& out) {
        static const char* commands[] = {
            "AT",
            "ATI",
            "AT+CPIN?",
            "AT+CSQ",
            "AT+COPS?",
            "AT+CREG?",
            "AT+CGREG?",
            "AT+CEREG?",
            "AT+CGACT?",
            "AT+NETOPEN?",
            "AT+IPADDR"
        };

        out.println("Encolando diagnostico modem...");
        for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
            char tag[24];
            snprintf(tag, sizeof(tag), "diag-%02u", (unsigned int)(i + 1));
            ModemJobId id = _modem->enqueueAtCommand(commands[i], i == 1 ? 10000 : 5000, tag, printAsyncResult);
            if (id == 0) {
                out.printf("Diag detenido: cola llena o error (%s)\n", _modem->lastErrorText());
                return;
            }
        }
    }
#endif

public:
#ifdef FEATURE_MODEM
    void attachModemService(ModemService* modem) {
        _modem = modem;
    }

    void attachRTC(RTCManager* rtc) {
        _rtc = rtc;
    }
#endif

    bool handle(const String& line, Print& out) {
        if (!(line == "m" || line.startsWith("m "))) {
            return false;
        }

        String args = line.substring(1);
        args.trim();
        if (args.length() == 0 || args == "help" || args == "?") {
            printHelp(out);
            return true;
        }

#ifndef FEATURE_MODEM
        out.println("Modem no compilado. Usa el environment eolo_standard.");
#else
        if (_modem == nullptr) {
            out.println("ModemService no disponible en esta consola");
            return true;
        }

        if (args == "begin") {
            out.println(_modem->begin() ? "Servicio modem listo" : _modem->lastErrorText());
        } else if (args == "end") {
            _modem->shutdownNow();
            out.println("Modem cerrado");
        } else if (args == "status") {
            _modem->printStatus(out);
        } else if (args.startsWith("at ")) {
            String command = args.substring(strlen("at "));
            command.trim();
            if (command.length() == 0) {
                out.println("Uso: m at <AT...>");
            } else {
                enqueueAt(out, command.c_str(), 10000, "console-at");
            }
        } else if (args.startsWith("get ")) {
            String url = args.substring(strlen("get "));
            url.trim();
            if (url.length() == 0) {
                out.println("Uso: m get <URL>");
            } else {
                ModemJobId id = _modem->enqueueHttpGet(url.c_str(), "console-get", printAsyncResult);
                if (id == 0) out.printf("No se pudo encolar GET (%s)\n", _modem->lastErrorText());
                else out.printf("GET encolado job #%lu\n", (unsigned long)id);
            }
        } else if (args.startsWith("post ")) {
            String rest = args.substring(strlen("post "));
            rest.trim();
            int sep = rest.indexOf(' ');
            if (sep <= 0) {
                out.println("Uso: m post <URL> <body>");
            } else {
                String url = rest.substring(0, sep);
                String body = rest.substring(sep + 1);
                body.trim();
                ModemJobId id = _modem->enqueueHttpPost(url.c_str(), body.c_str(), "console-post", printAsyncResult);
                if (id == 0) out.printf("No se pudo encolar POST (%s)\n", _modem->lastErrorText());
                else out.printf("POST encolado job #%lu\n", (unsigned long)id);
            }
        } else if (args == "timesync" || args.startsWith("timesync ")) {
            if (_rtc == nullptr) {
                out.println("RTC no disponible (usa attachRTC)");
            } else {
                String urlArg = args == "timesync" ? String(RTCManager::DefaultTimeServerUrl)
                                                   : args.substring(strlen("timesync "));
                urlArg.trim();
                if (urlArg.length() == 0) urlArg = RTCManager::DefaultTimeServerUrl;

                TimeSyncContext* ctx = new TimeSyncContext();
                if (ctx == nullptr) {
                    out.println("Sin memoria para timesync");
                } else {
                    ctx->rtc = _rtc;
                    strncpy(ctx->url, urlArg.c_str(), sizeof(ctx->url) - 1);
                    ctx->url[sizeof(ctx->url) - 1] = '\0';
                    ModemJobId id = _modem->enqueueHttpGet(ctx->url, "timesync", timeSyncCallback, ctx);
                    if (id == 0) {
                        out.printf("No se pudo encolar timesync (%s)\n", _modem->lastErrorText());
                        delete ctx;
                    } else {
                        out.printf("Timesync encolado job #%lu\n", (unsigned long)id);
                    }
                }
            }
        } else if (args == "ntp" || args.startsWith("ntp ")) {
            out.println("m ntp esta obsoleto. Usa: m timesync [URL]. No hay soporte NTP/AT+CNTP.");
        } else if (args == "diag") {
            runDiag(out);
        } else if (args == "scan") {
            enqueueAt(out, "AT+COPS=?", 180000, "scan");
        } else if (args == "auto") {
            enqueueAt(out, "AT+COPS=0", 120000, "operator-auto");
        } else if (args.startsWith("operator ")) {
            String code = args.substring(strlen("operator "));
            code.trim();
            char command[48];
            snprintf(command, sizeof(command), "AT+COPS=1,2,\"%s\"", code.c_str());
            enqueueAt(out, command, 120000, "operator");
        } else if (args == "connect" || args.startsWith("connect ") || args == "open" || args.startsWith("open ")) {
            out.println("Conexion de datos es interna al servicio; usa m get o m timesync para abrir red bajo demanda.");
        } else if (args.startsWith("ping ")) {
            out.println("Ping directo no forma parte de la cola publica; usa m at AT+CPING=... si lo necesitas.");
        } else if (args == "time") {
            if (_rtc == nullptr) out.println("RTC no disponible (usa attachRTC)");
            else out.println(_rtc->getTimeString());
        } else {
            out.println("Comando modem desconocido. Usa: m help");
        }
#endif

        return true;
    }
};

#endif
