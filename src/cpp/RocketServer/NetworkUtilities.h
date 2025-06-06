#pragma once
#include <cstdint>
#include <vector>
#include <chrono>
#include <assert.h>
#include "GamePacket.h"

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

    static inline void ComputeAckBits(const std::vector<uint64_t>& data, const uint64_t& ack, uint32_t& ackBits)
    {
        ackBits = 0;
        if (data.empty())
        {
            // No packets, so no ack bits to set
            return;
        }

        size_t idx = data.size() - 1;
        uint64_t expected = static_cast<uint64_t>(ack - 1);

        for (int bit = 31; bit >= 0; bit--, expected = static_cast<uint64_t>(expected - 1))
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

    static inline void StoreAcks(std::vector<uint64_t>& data, const uint64_t& ack, const uint32_t& ackBits)
    {
        // Always include the main ack value
        std::vector<uint64_t> acks;
        acks.push_back(ack);

        // Add values from ackBits
        uint64_t expected = ack - 1;
        for (int bit = 31; bit >= 0; --bit, --expected)
        {
            if (ackBits & (1u << bit))
            {
                acks.push_back(expected);
            }
        }

        // Insert new acks into data if not already present
        for (uint64_t seq : acks)
        {
            if (std::find(data.begin(), data.end(), seq) == data.end())
            {
                data.push_back(seq);
            }
        }
    }

    static inline void VerifyAck(std::vector<PacketInfo>& data, const uint16_t& ack, uint32_t& ackBits)
    {
        assert(data.size() != 0 && "Data cannot be empty when verifying acks");

        size_t idx = data.size() - 1;
        bool isAck = true;
        uint16_t expected = static_cast<uint16_t>(ack);

        for (int bit = 31; bit >= 0; bit--, expected = static_cast<uint16_t>(expected - 1))
        {
            while (idx > 0 && data[idx].seqNum > expected)
            {
                idx--;
            }
            if (idx >= 0 && data[idx].seqNum == expected)
            {
                if (!data[idx].acknowledged)
                {
                    // First acknowledgement time of the packet is relevant
                    data[idx].acknowledged = isAck;
                    data[idx].receiveTicks = std::chrono::steady_clock::now();
                    data[idx].roundTripTime = data[idx].receiveTicks - data[idx].sendTicks;
                }
            }

            isAck = (ackBits & (1u << bit)) != 0;
        }
    }
};
