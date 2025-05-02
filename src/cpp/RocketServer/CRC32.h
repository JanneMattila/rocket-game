#pragma once
#include <cstdint>
#include <cstddef>

class CRC32
{
public:
    CRC32();
    void reset();
    void update(const uint8_t* data, size_t length);
    uint32_t value() const;
    static uint32_t calculate(const uint8_t* data, size_t length);
private:
    uint32_t crc;
    static uint32_t table[256];
    static void generate_table();
    static bool table_initialized;
};
