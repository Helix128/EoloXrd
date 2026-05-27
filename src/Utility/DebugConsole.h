#ifndef DEBUG_CONSOLE_HPP
#define DEBUG_CONSOLE_HPP

#include <Arduino.h>
#include "DebugCommandRouter.h"
#include "ModemDebugCommands.h"
#include "I2CDebugCommands.h"
#include "SerialOutput.h"
#include "SystemDebugCommands.h"
#include "../Board/RTCManager.h"
#if defined(FEATURE_HEADLESS) && defined(EOLO_TARGET_DRON)
#include "CaptureSwitchDebugCommands.h"
#endif
#ifdef FEATURE_MODEM
#include "../Board/ModemService.h"
#endif

class DebugConsole {
private:
    enum Mode {
        Normal,
        Terminal
    };

    Mode _mode = Normal;
    String _terminalLine;
    static const size_t MAX_LINE_LENGTH = 160;
    SystemDebugCommands _systemCommands;
    ModemDebugCommands _modemCommands;
    I2CDebugCommands _i2cCommands;
#if defined(FEATURE_HEADLESS) && defined(EOLO_TARGET_DRON)
    CaptureSwitchDebugCommands _captureSwitchCommands;
#endif
    DebugCommandRouter _router;

    bool isQuickCommand(char c) const {
        return c == 'h' || c == '?' || c == 'p' || c == 'r' || c == 'v';
    }

    void enterTerminal() {
        _mode = Terminal;
        _terminalLine = "";
        SerialOutputState::setTerminalActive(true);
        cmdOut.println("Modo terminal activado. Escribe exit para volver.");
    }

    void exitTerminal() {
        _mode = Normal;
        _terminalLine = "";
        SerialOutputState::setTerminalActive(false);
        cmdOut.println("Modo terminal desactivado.");
    }

    void pollNormal() {
        while (Serial.available() > 0) {
            char c = (char)Serial.read();
            if (isQuickCommand(c)) {
                _router.route(String(c));
            } else if (c == '!') {
                enterTerminal();
            }
        }
    }

    void pollTerminal() {
        while (Serial.available() > 0) {
            char c = (char)Serial.read();

            if (c == '\r' || c == '\n') {
                cmdOut.println();
                String line = _terminalLine;
                _terminalLine = "";
                line.trim();
                if (line.length() == 0) {
                    continue;
                }
                if (line == "exit") {
                    exitTerminal();
                    continue;
                }
                _router.route(line);
                continue;
            }

            if (c == '\b' || c == 127) {
                if (_terminalLine.length() > 0) {
                    _terminalLine.remove(_terminalLine.length() - 1);
                    cmdOut.print("\b \b");
                }
                continue;
            }

            if (_terminalLine.length() < MAX_LINE_LENGTH) {
                _terminalLine += c;
                cmdOut.print(c);
            }
        }
    }

public:
    DebugConsole()
#ifdef FEATURE_MODEM
        : _systemCommands(true),
#else
        : _systemCommands(false),
#endif
          _router(cmdOut) {
        _router.addHandler(_systemCommands);
        _router.addHandler(_modemCommands);
        _router.addHandler(_i2cCommands);
#if defined(FEATURE_HEADLESS) && defined(EOLO_TARGET_DRON)
        _router.addHandler(_captureSwitchCommands);
#endif
    }

#ifdef FEATURE_MODEM
    void attachModemService(ModemService* modem) {
        _modemCommands.attachModemService(modem);
    }
#endif

    void attachRTC(RTCManager* rtc) {
        _systemCommands.attachRTC(rtc);
#ifdef FEATURE_MODEM
        _modemCommands.attachRTC(rtc);
#endif
    }

#if defined(FEATURE_HEADLESS) && defined(EOLO_TARGET_DRON)
    void attachCaptureSwitches(CaptureSwitches* switches) {
        _captureSwitchCommands.attachCaptureSwitches(switches);
    }
#endif

    void attachDisplayReinit(void (*fn)()) {
        _i2cCommands.attachDisplayReinit(fn);
    }

    void poll() {
        if (_mode == Terminal) {
            pollTerminal();
        } else {
            pollNormal();
        }
    }
};

#endif
