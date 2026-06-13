#ifndef EOLO_CORE_SENSORS_PLANTOWER_PARSER_H
#define EOLO_CORE_SENSORS_PLANTOWER_PARSER_H

#include <stdint.h>
#include <Eolo/Types/PlantowerData.h>

class PlantowerParser
{
public:
    enum State
    {
        HEAD1,
        HEAD2,
        LENGTH_H,
        LENGTH_L,
        DATA,
        CHECKSUM
    };

    static constexpr uint16_t ExpectedLen = 28;

    PlantowerParser() : _state(HEAD1) {}

    bool processByte(uint8_t ch)
    {
        switch (_state)
        {
        case HEAD1:
            if (ch == 0x42)
            {
                _state = HEAD2;
                _calcChecksum = 0x42;
            }
            break;
        case HEAD2:
            if (ch == 0x4d)
            {
                _state = LENGTH_H;
                _calcChecksum += 0x4d;
            }
            else
            {
                _state = HEAD1;
            }
            break;
        case LENGTH_H:
            _packetLen = (ch << 8);
            _calcChecksum += ch;
            _state = LENGTH_L;
            break;
        case LENGTH_L:
            _packetLen |= ch;
            _calcChecksum += ch;
            if (_packetLen != ExpectedLen)
            {
                reset();
            }
            else
            {
                _state = DATA;
                _bufIdx = 0;
            }
            break;
        case DATA:
            _buffer[_bufIdx++] = ch;
            if (_bufIdx < _packetLen - 2)
            {
                _calcChecksum += ch;
            }
            else
            {
                _state = CHECKSUM;
                _bufIdx = 0;
            }
            break;
        case CHECKSUM:
            if (_bufIdx == 0)
            {
                _recvChecksum = (ch << 8);
                _bufIdx++;
            }
            else
            {
                _recvChecksum |= ch;
                bool isValid = (_calcChecksum == _recvChecksum);
                if (isValid)
                {
                    _data.pm1_0 = (_buffer[6] << 8) | _buffer[7];
                    _data.pm2_5 = (_buffer[8] << 8) | _buffer[9];
                    _data.pm10_0 = (_buffer[10] << 8) | _buffer[11];
                    _data.valid = true;
                }
                reset();
                return isValid;
            }
            break;
        }
        return false;
    }

    PlantowerData getData() const { return _data; }

private:
    void reset()
    {
        _state = HEAD1;
        _bufIdx = 0;
    }

    State _state;
    uint8_t _buffer[32] = {0};
    uint8_t _bufIdx = 0;
    uint16_t _calcChecksum = 0;
    uint16_t _packetLen = 0;
    uint16_t _recvChecksum = 0;
    PlantowerData _data = {0, 0, 0, false};
};

#endif // EOLO_CORE_SENSORS_PLANTOWER_PARSER_H
