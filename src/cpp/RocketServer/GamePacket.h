#pragma once
#include "NetworkPacket.h"
#include <chrono>

struct Vector
{
    int x;
    int y;
};

struct PlayerPacket
{
    Vector pos;
    Vector vel;
    uint8_t keyboard;
};

struct PacketInfo
{
    uint64_t seqNum{};
    bool acknowledged{};
    std::chrono::steady_clock::time_point sendTicks{};
    std::chrono::steady_clock::time_point receiveTicks{};
    std::chrono::steady_clock::duration roundTripTime{};
};

class GamePacket :
    public NetworkPacket
{
private:
    // TODO: Study 16 and use wrap-around:
    // https://gafferongames.com/post/reliability_ordering_and_congestion_avoidance_over_udp/
    int32_t _seqNum = 0;
    int32_t _ack = 0;
    uint32_t _ackBits = 0;
    int64_t _ticks = 0;
    int8_t _numPlayers = 0;
    float PositionX = 0;
    float PositionY = 0;
    float VelocityX = 0;
    float VelocityY = 0;
    float Rotation = 0;
    float Speed = 0;
    float Delta = 0;
    uint8_t _isUp = 0;
    uint8_t _isDown = 0;
    uint8_t _isLeft = 0;
    uint8_t _isRight = 0;
    uint8_t _isFiring = 0;

public:
    std::vector<uint8_t> ToBytes() override;
    void ReadFromBytes(const std::vector<uint8_t>& data);
};

