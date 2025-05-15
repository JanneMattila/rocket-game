#pragma once
#include "NetworkPacket.h"
class GamePacket :
    public NetworkPacket
{
private:
    static constexpr int ExpectedMessageSize =
        sizeof(int32_t) + // CRC32
        sizeof(int64_t) + // ticks
        sizeof(int32_t) * 7 + // pos(2) + vel(2) + rotation + speed
        sizeof(uint8_t); // keyboard

    int64_t Ticks = 0;
    float PositionX = 0;
    float PositionY = 0;
    float VelocityX = 0;
    float VelocityY = 0;
    float Rotation = 0;
    float Speed = 0;
    float Delta = 0;
    uint8_t IsUp = 0;
    uint8_t IsDown = 0;
    uint8_t IsLeft = 0;
    uint8_t IsRight = 0;
    uint8_t IsFiring = 0;

public:
    std::vector<uint8_t> ToBytes() override;
    void ReadFromBytes(const std::vector<uint8_t>& data);
};

