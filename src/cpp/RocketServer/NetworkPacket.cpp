#include <cstring>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <endian.h>
#define ntohll(x) be64toh(x)
#define htonll(x) htobe64(x)
#endif
#include "NetworkPacket.h"
#include "NetworkUtilities.h"

NetworkPacket::NetworkPacket()
	: m_offset(0)
{
	Clear();
}

NetworkPacket::NetworkPacket(std::vector<uint8_t>& data)
	: m_buffer(data), m_offset(0)
{
}

size_t NetworkPacket::Size()
{
	return m_buffer.size();
}

void NetworkPacket::Clear()
{
	m_buffer.clear();
    m_buffer.reserve(1024);
	WriteInt32(0); // Placeholder for CRC32
	m_offset = 0;
}

void NetworkPacket::WriteInt8(int8_t value)
{
	m_buffer.insert(m_buffer.end(), value);
}

void NetworkPacket::WriteInt16(int16_t value)
{
    int16_t net = htons(value);
    uint8_t* p = reinterpret_cast<uint8_t*>(&net);
    m_buffer.insert(m_buffer.end(), p, p + sizeof(int16_t));
}

void NetworkPacket::WriteInt32(int32_t value)
{
	int32_t net = htonl(value);
	uint8_t* p = reinterpret_cast<uint8_t*>(&net);
	m_buffer.insert(m_buffer.end(), p, p + sizeof(int32_t));
}

void NetworkPacket::WriteInt64(int64_t value)
{
	int64_t net = htonll(value);
	uint8_t* p = reinterpret_cast<uint8_t*>(&net);
	m_buffer.insert(m_buffer.end(), p, p + sizeof(int64_t));
}

void NetworkPacket::WriteUInt64(uint64_t value)
{
    uint64_t net = htonll(value);
    uint8_t* p = reinterpret_cast<uint8_t*>(&net);
    m_buffer.insert(m_buffer.end(), p, p + sizeof(uint64_t));
}

void NetworkPacket::WriteKeyboard(const Keyboard& keyboard)
{
	uint8_t k = NetworkUtilities::PackKeyboard(keyboard);
	m_buffer.push_back(k);
}

int8_t NetworkPacket::ReadInt8()
{
	int8_t value = m_buffer[m_offset];
	m_offset += sizeof(int8_t);
	return value;
}

int16_t NetworkPacket::ReadInt16()
{
    int16_t net = 0;
    std::memcpy(&net, m_buffer.data() + m_offset, sizeof(int16_t));
    m_offset += sizeof(int16_t);
    return ntohs(net);
}

int32_t NetworkPacket::ReadInt32()
{
    int32_t net = 0;
    std::memcpy(&net, m_buffer.data() + m_offset, sizeof(int32_t));
    m_offset += sizeof(int32_t);
    return ntohl(net);
}

uint64_t NetworkPacket::ReadUInt64()
{
    uint64_t net = 0;
	std::memcpy(&net, m_buffer.data() + m_offset, sizeof(uint64_t));
	m_offset += sizeof(uint64_t);
	return ntohll(net);
}

float NetworkPacket::ReadInt32ToFloat()
{
	return ReadInt32() / 1000.0f;
}

std::vector<uint8_t> NetworkPacket::ToBytes()
{
	return m_buffer;
}

NetworkPacket NetworkPacket::FromBytes(const std::vector<uint8_t>& data)
{
	m_buffer = data;
	return *this;
}

NetworkPacketType NetworkPacket::ReadNetworkPacketType()
{
	int8_t networkPacketType = ReadInt8();
	return static_cast<NetworkPacketType>(networkPacketType);
}

int NetworkPacket::ReadAndValidateCRC()
{
	// CRC32 check
	uint32_t received_crc = ReadInt32();
	m_crc.reset();
	uint8_t magic = PROTOCOL_MAGIC_NUMBER;
	m_crc.update(&magic, 1);
	m_crc.update(m_buffer.data() + CRC32::CRC_SIZE, m_buffer.size() - CRC32::CRC_SIZE);
	uint32_t calc_crc = m_crc.value();
	if (received_crc != calc_crc)
	{
		return 1;
	}
	return 0;
}

void NetworkPacket::CalculateCRC()
{
	m_crc.reset();
	uint8_t magic = PROTOCOL_MAGIC_NUMBER;
	m_crc.update(&magic, 1);
	m_crc.update(m_buffer.data() + CRC32::CRC_SIZE, m_buffer.size() - CRC32::CRC_SIZE);
	uint32_t crc = htonl(m_crc.value());
	std::memcpy(m_buffer.data(), &crc, sizeof(crc));
}