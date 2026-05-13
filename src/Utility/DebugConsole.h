#ifndef DEBUG_CONSOLE_HPP
#define DEBUG_CONSOLE_HPP

#include <Arduino.h>
#include "ConsoleInput.h"
#include "DebugCommandRouter.h"
#include "ModemDebugCommands.h"
#include "SystemDebugCommands.h"
#ifdef FEATURE_MODEM
#include "../Board/Modem.h"
#endif

class DebugConsole {
private:
    ConsoleInput _input;
    SystemDebugCommands _systemCommands;
    ModemDebugCommands _modemCommands;
    DebugCommandRouter _router;

public:
    DebugConsole()
        : _input(Serial),
#ifdef FEATURE_MODEM
          _systemCommands(true),
#else
          _systemCommands(false),
#endif
          _router(Serial) {
        _router.addHandler(_systemCommands);
        _router.addHandler(_modemCommands);
    }

#ifdef FEATURE_MODEM
    void attachModem(Modem* modem) {
        _modemCommands.attachModem(modem);
    }
#endif

    void poll() {
        _router.poll(_input);
    }
};

#endif
