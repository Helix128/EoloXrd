#ifndef I2C_DEBUG_COMMANDS_H
#define I2C_DEBUG_COMMANDS_H

#include <Arduino.h>
#include "DebugCommandRouter.h"
#include "../Board/I2CBus.h"

class I2CDebugCommands : public ConsoleCommandHandler {
private:
    void (*_displayReinitFn)() = nullptr;

    void printHelp(Print& out) {
        out.println("Comandos I2C:");
        out.println("  i2c scan       escanear bus I2C, listar dispositivos");
        out.println("  i2c reset      reiniciar bus I2C y reinicializar pantalla");
    }

public:
    void attachDisplayReinit(void (*fn)()) {
        _displayReinitFn = fn;
    }

    bool handle(const String& line, Print& out) override {
        if (!(line == "i2c" || line.startsWith("i2c "))) return false;

        String args = line.substring(3);
        args.trim();

        if (args.length() == 0 || args == "help" || args == "?") {
            printHelp(out);
            return true;
        }

        if (args == "scan") {
            out.println("Escaneando bus I2C...");
            uint8_t count = I2CBus::getInstance().scan();
            out.printf("Total: %u dispositivo(s) encontrado(s)\n", (unsigned int)count);
        } else if (args == "reset") {
            out.println("Reiniciando bus I2C...");
            bool busOk = I2CBus::getInstance().reset();
            out.printf("Bus I2C: %s\n", busOk ? "OK" : "fallo");
            if (_displayReinitFn != nullptr) {
                out.println("Reinicializando pantalla...");
                _displayReinitFn();
                out.println("Pantalla OK.");
            } else {
                out.println("Pantalla no adjuntada.");
            }
        } else {
            out.println("Comando i2c desconocido. Usa: i2c help");
        }

        return true;
    }
};

#endif
