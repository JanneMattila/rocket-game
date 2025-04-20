#pragma once
#include <cstdint>
#include <vector>
#include <stdexcept>
#include "CRC32.h"

class NetworkPacket {
public:
    static constexpr uint8_t ProtocolMagicNumber = 0xFE;
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

    std::vector<uint8_t> ToBytes() const;
    static NetworkPacket FromBytes(const std::vector<uint8_t>& data);

private:
    static void WriteInt32(std::vector<uint8_t>& buf, int32_t value);
    static void WriteInt64(std::vector<uint8_t>& buf, int64_t value);
    static void WriteKeyboard(std::vector<uint8_t>& buf, uint8_t up, uint8_t down, uint8_t left, uint8_t right, uint8_t firing);
    static int64_t ReadInt64(const std::vector<uint8_t>& buf, size_t& offset);
    static float ReadInt32ToFloat(const std::vector<uint8_t>& buf, size_t& offset);
    static int32_t ReadInt32(const std::vector<uint8_t>& buf, size_t& offset);
};
