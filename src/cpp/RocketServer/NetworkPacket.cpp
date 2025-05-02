#include "NetworkPacket.h"
#include <cstring>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h> // For htonl/ntohl/htonll/ntohll (Linux)
#endif

// Helper for 64-bit host/network order conversion
static int64_t htonll(int64_t value) {
#ifdef _WIN32
    return htonl((uint32_t)(value >> 32)) | ((int64_t)htonl((uint32_t)value) << 32);
#else
    return htobe64(value);
#endif
}
static int64_t ntohll(int64_t value) {
#ifdef _WIN32
    return ntohl((uint32_t)(value >> 32)) | ((int64_t)ntohl((uint32_t)value) << 32);
#else
    return be64toh(value);
#endif
}

void NetworkPacket::WriteInt32(std::vector<uint8_t>& buf, int32_t value) {
    int32_t net = htonl(value);
    uint8_t* p = reinterpret_cast<uint8_t*>(&net);
    buf.insert(buf.end(), p, p + sizeof(int32_t));
}

void NetworkPacket::WriteInt64(std::vector<uint8_t>& buf, int64_t value) {
    int64_t net = htonll(value);
    uint8_t* p = reinterpret_cast<uint8_t*>(&net);
    buf.insert(buf.end(), p, p + sizeof(int64_t));
}

void NetworkPacket::WriteKeyboard(std::vector<uint8_t>& buf, uint8_t up, uint8_t down, uint8_t left, uint8_t right, uint8_t firing) {
    uint8_t k = (up << 5) | (down << 4) | (left << 3) | (right << 2) | firing;
    buf.push_back(k);
}

std::vector<uint8_t> NetworkPacket::ToBytes() const {
    std::vector<uint8_t> buf;
    buf.resize(sizeof(int32_t)); // Reserve space for CRC32
    WriteInt64(buf, Ticks);
    WriteInt32(buf, static_cast<int32_t>(PositionX * 1000));
    WriteInt32(buf, static_cast<int32_t>(PositionY * 1000));
    WriteInt32(buf, static_cast<int32_t>(VelocityX * 1000));
    WriteInt32(buf, static_cast<int32_t>(VelocityY * 1000));
    WriteInt32(buf, static_cast<int32_t>(Rotation * 1000));
    WriteInt32(buf, static_cast<int32_t>(Speed * 1000));
    WriteInt32(buf, static_cast<int32_t>(Delta * 1000));
    WriteKeyboard(buf, IsUp, IsDown, IsLeft, IsRight, IsFiring);
    // Calculate CRC32
    CRC32 crc;
    crc.reset();
    uint8_t magic = ProtocolMagicNumber;
    crc.update(&magic, 1);
    crc.update(buf.data() + sizeof(int32_t), buf.size() - sizeof(int32_t));
    uint32_t crcval = crc.value();
    // Write CRC32 to the start
    for (int i = 0; i < 4; ++i) buf[i] = (crcval >> (i * 8)) & 0xFF;
    return buf;
}

int64_t NetworkPacket::ReadInt64(const std::vector<uint8_t>& buf, size_t& offset) {
    int64_t net = 0;
    std::memcpy(&net, buf.data() + offset, sizeof(int64_t));
    offset += sizeof(int64_t);
    return ntohll(net);
}

int32_t NetworkPacket::ReadInt32(const std::vector<uint8_t>& buf, size_t& offset) {
    int32_t net = 0;
    std::memcpy(&net, buf.data() + offset, sizeof(int32_t));
    offset += sizeof(int32_t);
    return ntohl(net);
}

float NetworkPacket::ReadInt32ToFloat(const std::vector<uint8_t>& buf, size_t& offset) {
    return ReadInt32(buf, offset) / 1000.0f;
}

NetworkPacket NetworkPacket::FromBytes(const std::vector<uint8_t>& data) {
    if (data.size() != ExpectedMessageSize)
        throw std::runtime_error("Invalid message size");
    // CRC32 check
    uint32_t received_crc = 0;
    for (int i = 0; i < 4; ++i) received_crc |= (data[i] << (i * 8));
    CRC32 crc;
    crc.reset();
    uint8_t magic = ProtocolMagicNumber;
    crc.update(&magic, 1);
    crc.update(data.data() + 4, data.size() - 4);
    uint32_t calc_crc = crc.value();
    if (received_crc != calc_crc)
        throw std::runtime_error("CRC32 mismatch");
    size_t offset = 4;
    NetworkPacket pkt;
    pkt.Ticks = ReadInt64(data, offset);
    pkt.PositionX = ReadInt32ToFloat(data, offset);
    pkt.PositionY = ReadInt32ToFloat(data, offset);
    pkt.VelocityX = ReadInt32ToFloat(data, offset);
    pkt.VelocityY = ReadInt32ToFloat(data, offset);
    pkt.Rotation = ReadInt32ToFloat(data, offset);
    pkt.Speed = ReadInt32ToFloat(data, offset);
    pkt.Delta = ReadInt32ToFloat(data, offset);
    uint8_t k = data[offset++];
    pkt.IsUp = (k >> 5) & 1;
    pkt.IsDown = (k >> 4) & 1;
    pkt.IsLeft = (k >> 3) & 1;
    pkt.IsRight = (k >> 2) & 1;
    pkt.IsFiring = k & 1;
    return pkt;
}
