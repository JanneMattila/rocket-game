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
#include "Keyboard.h"

class NetworkPacket {
protected:
    static constexpr uint8_t PROTOCOL_MAGIC_NUMBER = 0xFE;

    std::vector<uint8_t> m_buffer;
    CRC32 m_crc;
    size_t m_offset;

public:
    NetworkPacket();
    NetworkPacket(std::vector<uint8_t>& data);
    virtual std::vector<uint8_t> ToBytes();
    virtual NetworkPacket FromBytes(const std::vector<uint8_t>& data);
    virtual int ReadAndValidateCRC();

    size_t Size();
    void Clear();
    void CalculateCRC();
    void WriteInt8(int8_t value);
    void WriteInt16(int16_t value);
    void WriteInt32(int32_t value);
    void WriteInt64(int64_t value);
    void WriteUInt64(uint64_t value);
    void WriteKeyboard(const Keyboard& keyboard);
    NetworkPacketType ReadNetworkPacketType();
    int8_t ReadInt8();
    int16_t ReadInt16();
    int32_t ReadInt32();
    uint64_t ReadUInt64();
    float ReadInt32ToFloat();
};
