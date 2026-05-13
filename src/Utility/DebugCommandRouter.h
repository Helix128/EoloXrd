#ifndef DEBUG_COMMAND_ROUTER_H
#define DEBUG_COMMAND_ROUTER_H

#include <Arduino.h>
#include "ConsoleInput.h"

class ConsoleCommandHandler {
public:
    virtual bool handle(const String& line, Print& out) = 0;
};

class DebugCommandRouter {
private:
    static const size_t MAX_HANDLERS = 6;
    ConsoleCommandHandler* _handlers[MAX_HANDLERS] = {};
    size_t _handlerCount = 0;
    Print& _out;

    void route(String line) {
        line.trim();
        if (line.length() == 0) return;

        for (size_t i = 0; i < _handlerCount; i++) {
            if (_handlers[i]->handle(line, _out)) return;
        }
    }

public:
    DebugCommandRouter(Print& out) : _out(out) {}

    void addHandler(ConsoleCommandHandler& handler) {
        if (_handlerCount < MAX_HANDLERS) {
            _handlers[_handlerCount++] = &handler;
        }
    }

    void poll(ConsoleInput& input) {
        String line;
        while (input.readLine(line)) {
            route(line);
        }
    }
};

#endif
