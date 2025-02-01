#pragma once

#include <inttypes.h>
#include <stdlib.h>

class CircularBuffer
{
public:
    bool init(size_t bufferSize, size_t bufferNumber)
    {
        _buffer = (uint8_t*)malloc(bufferSize * bufferNumber);
        if (_buffer == nullptr)
        {
            return false;
        }
        return true;
    }

    ~CircularBuffer()
    {
        if (_buffer)
        {
            free(_buffer);
        }
    }

private:
    uint8_t* _buffer = nullptr;
    uint8_t* _beg    = nullptr;
    uint8_t* _end    = nullptr;
};