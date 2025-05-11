#include "NetworkPacket.h"
#include <cstring>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif
#include "NetworkPacketType.h"

NetworkPacket::NetworkPacket()
{
	Clear();
}

NetworkPacket::NetworkPacket(std::vector<uint8_t>& data)
	: m_buffer(data)
{
}

size_t NetworkPacket::Size()
{
	return m_buffer.size();
}

void NetworkPacket::Clear()
{
	m_buffer.clear();
	WriteInt32(0); // Placeholder for CRC32
	WriteInt8(PROTOCOL_MAGIC_NUMBER); // Protocol magic number
}

void NetworkPacket::WriteInt8(int8_t value)
{
	m_buffer.insert(m_buffer.end(), value);
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

void NetworkPacket::WriteKeyboard(uint8_t up, uint8_t down, uint8_t left, uint8_t right, uint8_t firing)
{
	uint8_t k = (up << 5) | (down << 4) | (left << 3) | (right << 2) | firing;
	m_buffer.push_back(k);
}

int8_t NetworkPacket::ReadInt8(size_t& offset)
{
	int8_t value = m_buffer[offset];
	offset += sizeof(int8_t);
	return value;
}

int64_t NetworkPacket::ReadInt64(size_t& offset)
{
	int64_t net = 0;
	std::memcpy(&net, m_buffer.data() + offset, sizeof(int64_t));
	offset += sizeof(int64_t);
	return ntohll(net);
}

int32_t NetworkPacket::ReadInt32(size_t& offset)
{
	int32_t net = 0;
	std::memcpy(&net, m_buffer.data() + offset, sizeof(int32_t));
	offset += sizeof(int32_t);
	return ntohl(net);
}

float NetworkPacket::ReadInt32ToFloat(size_t& offset)
{
	return ReadInt32(offset) / 1000.0f;
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

NetworkPacketType NetworkPacket::GetNetworkPacketType()
{
	size_t offset = CRC32::CRC_SIZE + 1 /* protocol magic number */;
	int8_t networkPacketType = ReadInt8(offset);
	return static_cast<NetworkPacketType>(networkPacketType);
}

int NetworkPacket::Validate() {
	// CRC32 check
	uint32_t received_crc = 0;
	for (int i = 0; i < 4; ++i) received_crc |= (m_buffer[i] << (i * 8));
	m_crc.reset();
	uint8_t magic = PROTOCOL_MAGIC_NUMBER;
	m_crc.update(&magic, 1);
	m_crc.update(m_buffer.data() + 4, m_buffer.size() - 4);
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
	m_crc.update(m_buffer.data() + 4, m_buffer.size() - 4);
	uint32_t crc = m_crc.value();
	std::memcpy(m_buffer.data(), &crc, sizeof(crc));
}