#pragma once
#include <cstdint>

struct Keyboard
{
    bool up;
    bool down;
    bool left;
    bool right;
    bool space;

    void ReadFromByte(uint8_t byte)
    {
        up = (byte & 0x01) != 0;
        down = (byte & 0x02) != 0;
        left = (byte & 0x04) != 0;
        right = (byte & 0x08) != 0;
        space = (byte & 0x10) != 0;
    }

    uint8_t ToByte() const
    {
        uint8_t byte = 0;
        if (up) byte |= 0x01;
        if (down) byte |= 0x02;
        if (left) byte |= 0x04;
        if (right) byte |= 0x08;
        if (space) byte |= 0x10;
        return byte;
    }
};
