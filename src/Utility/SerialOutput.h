#ifndef SERIAL_OUTPUT_H
#define SERIAL_OUTPUT_H

#include <Arduino.h>

class SerialOutputState {
private:
    static bool& terminalActiveRef() {
        static bool active = false;
        return active;
    }

public:
    static void setTerminalActive(bool active) {
        terminalActiveRef() = active;
    }

    static bool terminalActive() {
        return terminalActiveRef();
    }
};

class SerialOutput : public Print {
private:
    bool _silentInTerminal;

public:
    explicit SerialOutput(bool silentInTerminal) : _silentInTerminal(silentInTerminal) {}

    size_t write(uint8_t byte) override {
        if (_silentInTerminal && SerialOutputState::terminalActive()) {
            return 1;
        }
        return Serial.write(byte);
    }

    size_t write(const uint8_t* buffer, size_t size) override {
        if (_silentInTerminal && SerialOutputState::terminalActive()) {
            return size;
        }
        return Serial.write(buffer, size);
    }

    void flush() {
        if (!_silentInTerminal || !SerialOutputState::terminalActive()) {
            Serial.flush();
        }
    }
};

static SerialOutput& stdOutRef() {
    static SerialOutput out(true);
    return out;
}

static SerialOutput& cmdOutRef() {
    static SerialOutput out(false);
    return out;
}

#define stdOut stdOutRef()
#define cmdOut cmdOutRef()

#endif
