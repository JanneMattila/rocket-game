#include "pch.h"
#include <vector>
#include "CppUnitTest.h"
#include "NetworkUtilities.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace RocketServerTests
{
    TEST_CLASS(AckTests)
    {
    private:
    public:
        // All 32 previous packets present: ackBits should be all 1s
        TEST_METHOD(BuildAckList_AllPresent)
        {
            uint16_t ack = 100;
            std::vector<uint16_t> data;
            for (int i = 32; i >= 1; --i)
                data.push_back(ack - i); // 68, 69, ..., 99
            uint32_t ackBits = 0;
            NetworkUtilities::BuildAckList(data, ack, ackBits);
            Assert::AreEqual(0xFFFFFFFFu, ackBits, L"All bits should be set");
        }

        // Some packets missing (holes)
        TEST_METHOD(BuildAckList_SomeMissing)
        {
            uint16_t ack = 100;
            std::vector<uint16_t> data = { 95, 97, 99 };
            uint32_t ackBits = 0;
            NetworkUtilities::BuildAckList(data, ack, ackBits);
            uint32_t expected = (1u << 31) | (1u << 29) | (1u << 27);
            Assert::AreEqual(expected, ackBits, L"Only bits for 99, 97, 95 should be set");
        }
        // First and last bit set
        TEST_METHOD(BuildAckList_FirstAndLastBit)
        {
            uint16_t ack = 100;
            std::vector<uint16_t> data = { 68, 99 };
            uint32_t ackBits = (1u << 31) | 1u;
            uint32_t actual = 0;
            NetworkUtilities::BuildAckList(data, ack, actual);
            Assert::AreEqual(ackBits, actual, L"Only highest and lowest bits should be set");
        }

        // Only one packet present
        TEST_METHOD(BuildAckList_OnePresent)
        {
            uint16_t ack = 100;
            std::vector<uint16_t> data = { 90 };
            uint32_t ackBits = 0;
            NetworkUtilities::BuildAckList(data, ack, ackBits);
            uint32_t expected = (1u << (32 - (100 - 90)));
            Assert::AreEqual(expected, ackBits, L"Only bit for 90 should be set");
        }

        // Test that VerifyAck updates acknowledged and receiveTicks for ack and ackBits
        TEST_METHOD(VerifyAck_AckAndAckBits)
        {
            // Arrange
            using namespace std::chrono;

            uint16_t ack = 100;
            // Create packets for seqNum 68..100 (inclusive)
            std::vector<PacketInfo> data;
            for (uint16_t i = 68; i <= ack; ++i)
            {
                PacketInfo pi;
                pi.seqNum = i;
                pi.acknowledged = false;
                pi.sendTicks = steady_clock::now();
                pi.receiveTicks = steady_clock::time_point{};
                data.push_back(pi);
            }

            // Set ackBits so that bits for 99, 97, 95 are set
            uint32_t ackBits = (1u << 31) | (1u << 29) | (1u << 27);

            // Act
            NetworkUtilities::VerifyAck(data, ack, ackBits);

            // Assert
            auto sequence = { 100, 99, 97, 95 };
            for (const PacketInfo& pi : data)
            {
                // If packet is on from sequence, then it should be acknowledged
                // else it should not be
                if (std::find(sequence.begin(), sequence.end(), pi.seqNum) != sequence.end())
                {
                    Assert::IsTrue(pi.acknowledged, L"These seqnums should be acknowledged");
                }
                else
                {
                    Assert::IsFalse(pi.acknowledged, L"Other packets should not be acknowledged");
                }
            }
        }
    };
}
