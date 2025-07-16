#pragma once
#include <cstdint>
#include "Keyboard.h"
#include "FloatInt.h"

struct Vector
{
    FloatInt x{};
    FloatInt y{};
};

struct PlayerState
{
    uint8_t playerID{};
    Vector pos{};
    Vector vel{};
    FloatInt speed{};
    FloatInt rotation{};
    FloatInt health{};
    Keyboard keyboard{};
    double deltaTime{};
};
