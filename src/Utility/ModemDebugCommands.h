#ifndef MODEM_DEBUG_COMMANDS_H
#define MODEM_DEBUG_COMMANDS_H

#include <Arduino.h>
#include "DebugCommandRouter.h"
#ifdef FEATURE_MODEM
#include "../Board/Modem.h"
#endif

class ModemDebugCommands : public ConsoleCommandHandler {
private:
#ifdef FEATURE_MODEM
    Modem* _modem = nullptr;
#endif

    void printHelp(Print& out) {
#ifdef FEATURE_MODEM
        out.println("Comandos modem:");
        out.println("  m begin              enciende el modem e inicializa UART/AT");
        out.println("  m end                cierra red y apaga el modem");
        out.println("  m status             imprime diagnóstico completo");
        out.println("  m scan               AT+COPS=? para listar redes");
        out.println("  m auto               vuelve a selección automática");
        out.println("  m operator <MCCMNC>  registra operador por código numérico");
        out.println("  m connect [APN]      conecta datos, default gigsky-02");
        out.println("  m at <AT...>         comando AT crudo");
#else
        out.println("Modem no compilado. Usa el environment eolo_standard.");
#endif
    }

#ifdef FEATURE_MODEM
    void printResult(Print& out, bool ok, const char* successText, const char* failureText) {
        if (ok) {
            out.println(successText);
        } else {
            out.printf("%s (%s)\n", failureText, _modem != nullptr ? _modem->lastErrorText() : "sin modem");
        }
    }
#endif

public:
#ifdef FEATURE_MODEM
    void attachModem(Modem* modem) {
        _modem = modem;
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
            out.println("Modem no disponible en esta consola");
            return true;
        }

        if (args == "begin") {
            out.println("Inicializando modem...");
            printResult(out, _modem->begin(), "Modem listo", "No hubo respuesta AT");
        } else if (args == "end") {
            _modem->end();
            out.println("Modem apagado");
        } else if (args == "status") {
            _modem->printDiagnostics(out);
        } else if (args == "scan") {
            out.println("Buscando operadores con AT+COPS=? ...");
            String response;
            bool ok = _modem->scanOperators(response);
            response.trim();
            out.println(response.length() > 0 ? response.c_str() : "(sin respuesta)");
            printResult(out, ok, "Scan terminado", "Scan sin resultado válido");
        } else if (args == "auto") {
            out.println("Seleccionando operador automático...");
            printResult(out, _modem->selectOperatorAuto(), "Operador automático OK", "No se pudo seleccionar operador automático");
        } else if (args.startsWith("operator ")) {
            String code = args.substring(strlen("operator "));
            code.trim();
            out.printf("Seleccionando operador %s...\n", code.c_str());
            printResult(out, _modem->selectOperatorNumeric(code.c_str()), "Operador seleccionado", "No se pudo seleccionar operador");
        } else if (args == "connect" || args.startsWith("connect ")) {
            String apn = "";
            if (args.startsWith("connect ")) {
                apn = args.substring(strlen("connect "));
                apn.trim();
            }
            out.printf("Conectando PDP con APN %s...\n", apn.length() > 0 ? apn.c_str() : "default");
            printResult(out, _modem->connect(apn.length() > 0 ? apn.c_str() : nullptr), "Datos conectados", "No se pudo conectar datos");
        } else if (args == "open" || args.startsWith("open ")) {
            String apn = "";
            if (args.startsWith("open ")) {
                apn = args.substring(strlen("open "));
                apn.trim();
            }
            out.printf("Abriendo red con APN %s...\n", apn.length() > 0 ? apn.c_str() : "default");
            printResult(out, _modem->openNetwork(apn.length() > 0 ? apn.c_str() : nullptr), "Red abierta", "No se pudo abrir red");
        } else if (args.startsWith("at ")) {
            String command = args.substring(strlen("at "));
            command.trim();
            String response;
            out.printf("> %s\n", command.c_str());
            bool ok = _modem->rawAT(command.c_str(), response, 10000);
            response.trim();
            out.println(response.length() > 0 ? response.c_str() : "(sin respuesta)");
            if (!ok) {
                out.printf("AT falló (%s)\n", _modem->lastErrorText());
            }
        } else {
            out.println("Comando modem desconocido. Usa: m help");
        }
#endif

        return true;
    }
};

#endif
