#include "CRC32.h"

// Standard CRC32 polynomial
constexpr uint32_t CRC32_POLY = 0xEDB88320U;

uint32_t CRC32::table[256];
bool CRC32::table_initialized = false;

void CRC32::generate_table()
{
	for (uint32_t i = 0; i < 256; ++i) {
		uint32_t c = i;
		for (size_t j = 0; j < 8; ++j) {
			if (c & 1)
				c = CRC32_POLY ^ (c >> 1);
			else
				c >>= 1;
		}
		table[i] = c;
	}
	table_initialized = true;
}

CRC32::CRC32()
{
	if (!table_initialized) generate_table();
	reset();
}

void CRC32::reset()
{
	crc = 0xFFFFFFFFU;
}

void CRC32::update(const uint8_t* data, size_t length)
{
	for (size_t i = 0; i < length; ++i) {
		crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
	}
}

uint32_t CRC32::value() const
{
	return crc ^ 0xFFFFFFFFU;
}

uint32_t CRC32::calculate(const uint8_t* data, size_t length)
{
	CRC32 c;
	c.update(data, length);
	return c.value();
}
