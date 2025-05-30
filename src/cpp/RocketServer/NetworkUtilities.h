#pragma once
#include <cstdint>
#include <vector>

constexpr auto SEQUENCE_NUMBER_MAX = 65535;
constexpr auto SEQUENCE_NUMBER_HALF = 32768;

class NetworkUtilities
{
public:
    // https://gafferongames.com/post/reliability_ordering_and_congestion_avoidance_over_udp/
    static inline uint16_t SequenceNumberDiff(uint16_t previous, uint16_t next)
    {
        if ((previous > next) && (previous - next <= SEQUENCE_NUMBER_HALF))
        {
            return previous - next;
        }
        else if ((previous < next) && (next - previous > SEQUENCE_NUMBER_HALF))
        {
            return next - previous;
        }
        return next - previous;
    }

    static inline void BuildAckList(const std::vector<uint16_t>& data, const uint16_t& ack, uint32_t& ackBits)
    {
        ackBits = 0;
        if (data.empty())
        {
            // No packets, so no ack bits to set
            return;
        }

        size_t idx = data.size() - 1;
        uint16_t expected = static_cast<uint16_t>(ack - 1);

        for (int bit = 31; bit >= 0; bit--, expected = static_cast<uint16_t>(expected - 1))
        {
            while (idx > 0 && data[idx] > expected)
            {
                idx--;
            }
            if (idx >= 0 && data[idx] == expected)
            {
                ackBits |= (1u << bit);
            }
        }
    }
};
