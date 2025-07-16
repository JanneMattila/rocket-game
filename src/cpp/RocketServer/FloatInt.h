#pragma once
#include <cstdint>

// https://new.gafferongames.com/post/serialization_strategies/
union FloatInt
{
    float floatValue{};
    uint32_t intValue;
};
