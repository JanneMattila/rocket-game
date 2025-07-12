#pragma once
#include <cstdint>
#include <chrono>

struct PacketInfo
{
    uint64_t seqNum{};
    bool acknowledged{};
    std::chrono::steady_clock::time_point sendTicks{};
    std::chrono::steady_clock::time_point receiveTicks{};
    std::chrono::steady_clock::duration roundTripTime{};
    Keyboard keyboard{};
};
