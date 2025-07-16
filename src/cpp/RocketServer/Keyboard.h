#pragma once
#include <cstdint>

struct Keyboard
{
    uint8_t up;
    uint8_t down;
    uint8_t left;
    uint8_t right;
    uint8_t space;

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

    bool operator==(const Keyboard& other) const
    {
        return up == other.up && 
               down == other.down &&
               left == other.left && 
               right == other.right &&
               space == other.space;
    }
};
