#include "pch.h"
#include <vector>
#include "CppUnitTest.h"
#include "NetworkUtilities.h"
#include <algorithm>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace RocketServerTests
{
    TEST_CLASS(AckTests)
    {
    private:
    public:
        // All 32 previous packets present: ackBits should be all 1s
        TEST_METHOD(ComputeAckBits_AllPresent)
        {
            uint64_t ack = 100;
            std::vector<uint64_t> data;
            for (int i = 32; i >= 1; --i)
                data.push_back(ack - i); // 68, 69, ..., 99
            uint32_t ackBits = 0;
            NetworkUtilities::ComputeAckBits(data, ack, ackBits);
            Assert::AreEqual(0xFFFFFFFFu, ackBits, L"All bits should be set");
        }

        // Some packets missing (holes)
        TEST_METHOD(ComputeAckBits_SomeMissing)
        {
            uint64_t ack = 100;
            std::vector<uint64_t> data = { 95, 97, 99 };
            uint32_t ackBits = 0;
            NetworkUtilities::ComputeAckBits(data, ack, ackBits);
            uint32_t expected = (1u << 31) | (1u << 29) | (1u << 27);
            Assert::AreEqual(expected, ackBits, L"Only bits for 99, 97, 95 should be set");
        }
        // First and last bit set
        TEST_METHOD(ComputeAckBits_FirstAndLastBit)
        {
            uint64_t ack = 100;
            std::vector<uint64_t> data = { 68, 99 };
            uint32_t ackBits = (1u << 31) | 1u;
            uint32_t actual = 0;
            NetworkUtilities::ComputeAckBits(data, ack, actual);
            Assert::AreEqual(ackBits, actual, L"Only highest and lowest bits should be set");
        }

        // Only one packet present
        TEST_METHOD(ComputeAckBits_OnePresent)
        {
            uint64_t ack = 100;
            std::vector<uint64_t> data = { 90 };
            uint32_t ackBits = 0;
            NetworkUtilities::ComputeAckBits(data, ack, ackBits);
            uint32_t expected = (1u << (32 - (100 - 90)));
            Assert::AreEqual(expected, ackBits, L"Only bit for 90 should be set");
        }

        // StoreAcks: data already contains some acks, new acks should be added, no duplicates
        TEST_METHOD(StoreAcks_AddsNewAcks_NoDuplicates)
        {
            uint64_t ack = 13;
            // Existing data contains 10, 11, 12
            std::vector<uint64_t> data = { 10, 11, 12 };
            // ackBits: set bit for 12 (ack-1), 11 (ack-2)
            uint32_t ackBits = (1u << 31) | (1u << 30);

            // Act
            NetworkUtilities::StoreAcks(data, ack, ackBits);

            // Should now contain 10, 11, 12, 13 (13 is the ack itself)
            std::vector<uint64_t> expected = { 10, 11, 12, 13 };
            std::sort(data.begin(), data.end());
            Assert::AreEqual(expected.size(), data.size(), L"Data should have 4 unique values");
            for (size_t i = 0; i < expected.size(); ++i)
                Assert::AreEqual(expected[i], data[i], L"Data should contain all expected values");
        }

        // StoreAcks: data is empty, all acks should be added
        TEST_METHOD(StoreAcks_EmptyData_AllAcksAdded)
        {
            uint64_t ack = 5;
            std::vector<uint64_t> data;
            // ackBits: set bits for 4, 3, 2
            uint32_t ackBits = (1u << 31) | (1u << 30) | (1u << 29);

            // Act
            NetworkUtilities::StoreAcks(data, ack, ackBits);

            // Should now contain 2, 3, 4, 5
            std::vector<uint64_t> expected = { 2, 3, 4, 5 };
            std::sort(data.begin(), data.end());
            Assert::AreEqual(expected.size(), data.size(), L"Data should have 4 values");
            for (size_t i = 0; i < expected.size(); ++i)
                Assert::AreEqual(expected[i], data[i], L"Data should contain all expected values");
        }

        // StoreAcks: ackBits is zero, only ack should be added if not present
        TEST_METHOD(StoreAcks_AckBitsZero_OnlyAckAdded)
        {
            uint64_t ack = 42;
            std::vector<uint64_t> data = { 40, 41 };
            uint32_t ackBits = 0;

            // Act
            NetworkUtilities::StoreAcks(data, ack, ackBits);

            // Should now contain 40, 41, 42
            std::vector<uint64_t> expected = { 40, 41, 42 };
            std::sort(data.begin(), data.end());
            Assert::AreEqual(expected.size(), data.size(), L"Data should have 3 values");
            for (size_t i = 0; i < expected.size(); ++i)
                Assert::AreEqual(expected[i], data[i], L"Data should contain all expected values");
        }

        // StoreAcks: all acks already present, nothing should change
        TEST_METHOD(StoreAcks_AllAcksAlreadyPresent_NoChange)
        {
            uint64_t ack = 7;
            std::vector<uint64_t> data = { 5, 6, 7 };
            // ackBits: set bit for 6 (ack-1), 5 (ack-2)
            uint32_t ackBits = (1u << 31) | (1u << 30);

            // Act
            NetworkUtilities::StoreAcks(data, ack, ackBits);

            // Should still contain 5, 6, 7 (no duplicates)
            std::vector<uint64_t> expected = { 5, 6, 7 };
            std::sort(data.begin(), data.end());
            Assert::AreEqual(expected.size(), data.size(), L"Data should have 3 values");
            for (size_t i = 0; i < expected.size(); ++i)
                Assert::AreEqual(expected[i], data[i], L"Data should contain all expected values");
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
