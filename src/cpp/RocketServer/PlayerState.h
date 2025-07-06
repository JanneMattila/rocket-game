#pragma once
#include <cstdint>
#include "Keyboard.h"

// https://new.gafferongames.com/post/serialization_strategies/
union FloatInt
{
    float floatValue;
    uint32_t intValue;
};

struct Vector
{
    FloatInt x;
    FloatInt y;
};

struct PlayerState
{
    uint8_t playerID;
    Vector pos;
    Vector vel;
    FloatInt speed;
    FloatInt rotation;
    FloatInt health;
    Keyboard keyboard;
};
