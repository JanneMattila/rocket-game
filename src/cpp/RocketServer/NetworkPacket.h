#pragma once
#include <cstdint>
#include <vector>
#include <stdexcept>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include "CRC32.h"
#include "NetworkPacketType.h"

class NetworkPacket {
protected:
    static constexpr uint8_t PROTOCOL_MAGIC_NUMBER = 0xFE;

    std::vector<uint8_t> m_buffer;
    CRC32 m_crc;

public:
    static constexpr uint8_t PAYLOAD_START_INDEX = CRC32::CRC_SIZE + 2 /* protocol magic number and packet type */;

    NetworkPacket();
    NetworkPacket(std::vector<uint8_t>& data);
    virtual std::vector<uint8_t> ToBytes();
    virtual NetworkPacket FromBytes(const std::vector<uint8_t>& data);
    int Validate();

    size_t Size();
    void Clear();
    void CalculateCRC();
    void WriteInt8(int8_t value);
    void WriteInt32(int32_t value);
    void WriteInt64(int64_t value);
    void WriteKeyboard(uint8_t up, uint8_t down, uint8_t left, uint8_t right, uint8_t firing);
    int8_t ReadInt8(size_t& offset);
    int32_t ReadInt32(size_t& offset);
    float ReadInt32ToFloat(size_t& offset);
    int64_t ReadInt64(size_t& offset);
    NetworkPacketType GetNetworkPacketType();
};
