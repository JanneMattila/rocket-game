#pragma once
#include <cstdint>
#include <chrono>
#include "PlayerState.h"

struct GameStateSnapshot
{
    uint64_t seqNum{};
    std::vector<PlayerState> players;
    double deltaTime{};
};
