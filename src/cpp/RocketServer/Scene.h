#pragma once
#include <vector>
#include "PlayerState.h"

struct Scene
{
    double fps = 0.0;
    double deltaTime = 0.0;
    uint64_t roundTripTimeMs = 0;
    std::vector<PlayerState> players;
};
