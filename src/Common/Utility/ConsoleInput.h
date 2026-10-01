#ifndef CONSOLE_INPUT_H
#define CONSOLE_INPUT_H

#include <Arduino.h>

class ConsoleInput {
private:
    Stream& _input;
    String _line;
    static const size_t MAX_LINE_LENGTH = 160;

public:
    ConsoleInput(Stream& input) : _input(input) {}

    bool readLine(String& outLine) {
        while (_input.available() > 0) {
            char c = (char)_input.read();

            if (c == '\r' || c == '\n') {
                outLine = _line;
                _line = "";
                return true;
            }

            if (_line.length() == 0 && (c == 'h' || c == '?' || c == 'p' || c == 'r' || c == 'v')) {
                outLine = String(c);
                return true;
            }

            if (_line.length() < MAX_LINE_LENGTH) {
                _line += c;
            }
        }

        return false;
    }
};

#endif
