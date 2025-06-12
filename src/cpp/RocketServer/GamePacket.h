#pragma once
#include "NetworkPacket.h"
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
    Keyboard keyboard;
};

class GamePacket :
    public NetworkPacket
{
private:

public:
    //std::vector<uint8_t> ToBytes() override;
    //void ReadFromBytes(const std::vector<uint8_t>& data);
    void Create(const PlayerState& playerState);
};

