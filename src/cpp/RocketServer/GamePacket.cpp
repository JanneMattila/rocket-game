#include "GamePacket.h"


std::vector<uint8_t> GamePacket::ToBytes() {
    WriteInt64(Ticks);
    WriteInt32(static_cast<int32_t>(PositionX * 1000));
    WriteInt32(static_cast<int32_t>(PositionY * 1000));
    WriteInt32(static_cast<int32_t>(VelocityX * 1000));
    WriteInt32(static_cast<int32_t>(VelocityY * 1000));
    WriteInt32(static_cast<int32_t>(Rotation * 1000));
    WriteInt32(static_cast<int32_t>(Speed * 1000));
    WriteInt32(static_cast<int32_t>(Delta * 1000));
    WriteKeyboard(IsUp, IsDown, IsLeft, IsRight, IsFiring);
    
    m_crc.reset();
    uint8_t magic = PROTOCOL_MAGIC_NUMBER;
    m_crc.update(&magic, 1);
    m_crc.update(m_buffer.data() + sizeof(int32_t), m_buffer.size() - sizeof(int32_t));
    uint32_t crcval = m_crc.value();
    // Write CRC32 to the start
    for (int i = 0; i < 4; ++i) m_buffer[i] = (crcval >> (i * 8)) & 0xFF;
    return m_buffer;
}

void GamePacket::ReadFromBytes(const std::vector<uint8_t>& data) {
    if (data.size() != ExpectedMessageSize)
        throw std::runtime_error("Invalid message size");
    // CRC32 check
    uint32_t received_crc = 0;
    for (int i = 0; i < CRC32::CRC_SIZE; ++i) received_crc |= (data[i] << (i * 8));
    m_crc.reset();
    uint8_t magic = PROTOCOL_MAGIC_NUMBER;
    m_crc.update(&magic, 1);
    m_crc.update(data.data() + 4, data.size() - 4);
    uint32_t calc_crc = m_crc.value();
    if (received_crc != calc_crc)
        throw std::runtime_error("CRC32 mismatch");
    size_t offset = CRC32::CRC_SIZE;
    this->Ticks = ReadInt64(offset);
    this->PositionX = ReadInt32ToFloat(offset);
    this->PositionY = ReadInt32ToFloat(offset);
    this->VelocityX = ReadInt32ToFloat(offset);
    this->VelocityY = ReadInt32ToFloat(offset);
    this->Rotation = ReadInt32ToFloat(offset);
    this->Speed = ReadInt32ToFloat(offset);
    this->Delta = ReadInt32ToFloat(offset);
    uint8_t k = data[offset++];
    this->IsUp = (k >> 5) & 1;
    this->IsDown = (k >> 4) & 1;
    this->IsLeft = (k >> 3) & 1;
    this->IsRight = (k >> 2) & 1;
    this->IsFiring = k & 1;
}
