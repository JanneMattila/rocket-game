#include "pch.h"
#include <vector>
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

constexpr auto SEQUENCE_NUMBER_MAX = 65535;
constexpr auto SEQUENCE_NUMBER_HALF = 32768;

namespace RocketServerTests
{
	TEST_CLASS(SequenceNumberTests)
	{
	private:
		// https://gafferongames.com/post/reliability_ordering_and_congestion_avoidance_over_udp/
		inline uint16_t SequenceNumberDiff(uint16_t previous, uint16_t next)
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

		inline void BuildAckList(const std::vector<uint16_t>& data, const uint16_t& ack, uint32_t& ackBits)
		{
			ackBits = 0;
			for (int i = 1; i <= 32; ++i)
			{
				uint16_t seq = static_cast<uint16_t>(ack - i);
				// Check if seq is in data
				if (std::find(data.begin(), data.end(), seq) != data.end())
				{
					ackBits |= (1u << (32 - i));
				}
			}
		}
	public:
		TEST_METHOD(Next_Sequence_Number_Normal1_Test)
		{
			// Arrange
			uint16_t previous = 64000;
			uint16_t next = 64001;
			int expected = 1;

			// Act
			int actual = SequenceNumberDiff(previous, next);

			// Assert
			Assert::AreEqual(expected, actual, L"Validation should have failed");
		}

		TEST_METHOD(Next_Sequence_Number_Normal2_Test)
		{
			// Arrange
			uint16_t previous = 62000;
			uint16_t next = 64001;
			int expected = 2001;

			// Act
			int actual = SequenceNumberDiff(previous, next);

			// Assert
			Assert::AreEqual(expected, actual, L"Validation should have failed");
		}

		TEST_METHOD(Next_Sequence_Number_Rollover1_Test)
		{
			// Arrange
			uint16_t previous = 65535;
			uint16_t next = 0;
			int expected = 1;

			// Act
			int actual = SequenceNumberDiff(previous, next);

			// Assert
			Assert::AreEqual(expected, actual, L"Validation should have failed");
		}

		TEST_METHOD(Next_Sequence_Number_Rollover2_Test)
		{
			// Arrange
			uint16_t previous = 65035;
			uint16_t next = 150;
			int expected = 651; // 500 + 150 + zero index = 651

			// Act
			int actual = SequenceNumberDiff(previous, next);

			// Assert
			Assert::AreEqual(expected, actual, L"Validation should have failed");
		}

		// All 32 previous packets present: ackBits should be all 1s
		TEST_METHOD(BuildAckList_AllPresent)
		{
			uint16_t ack = 100;
			std::vector<uint16_t> data;
			for (int i = 1; i <= 32; ++i)
				data.push_back(ack - i);
			uint32_t ackBits = 0;
			BuildAckList(data, ack, ackBits);
			Assert::AreEqual(0xFFFFFFFFu, ackBits, L"All bits should be set");
		}

		// No previous packets present: ackBits should be all 0s
		TEST_METHOD(BuildAckList_NonePresent)
		{
			uint16_t ack = 100;
			std::vector<uint16_t> data; // empty
			uint32_t ackBits = 0xFFFFFFFF;
			BuildAckList(data, ack, ackBits);
			Assert::AreEqual(0u, ackBits, L"No bits should be set");
		}

		// Some packets missing (holes)
		TEST_METHOD(BuildAckList_SomeMissing)
		{
			uint16_t ack = 100;
			std::vector<uint16_t> data = {99, 97, 95}; // Only 99, 97, 95 present
			uint32_t ackBits = 0;
			BuildAckList(data, ack, ackBits);
			// 99: bit 31, 97: bit 29, 95: bit 27
			uint32_t expected = (1u << 31) | (1u << 29) | (1u << 27);
			Assert::AreEqual(expected, ackBits, L"Only bits for 99, 97, 95 should be set");
		}

		// Wraparound at 0
		TEST_METHOD(BuildAckList_Wraparound)
		{
			uint16_t ack = 2;
			std::vector<uint16_t> data = {1, 0, 65535, 65534};
			uint32_t ackBits = 0;
			BuildAckList(data, ack, ackBits);
			// 1: bit 31, 0: bit 30, 65535: bit 29, 65534: bit 28
			uint32_t expected = (1u << 31) | (1u << 30) | (1u << 29) | (1u << 28);
			Assert::AreEqual(expected, ackBits, L"Bits for 1, 0, 65535, 65534 should be set");
		}

		// First and last bit set
		TEST_METHOD(BuildAckList_FirstAndLastBit)
		{
			uint16_t ack = 100;
			std::vector<uint16_t> data = {99, 68}; // 99: bit 31, 68: bit 0
			uint32_t ackBits = (1u << 31) | 1u;
			uint32_t actual = 0;
			BuildAckList(data, ack, actual);
			Assert::AreEqual(ackBits, actual, L"Only highest and lowest bits should be set");
		}

		// Duplicates in data
		TEST_METHOD(BuildAckList_Duplicates)
		{
			uint16_t ack = 100;
			std::vector<uint16_t> data = {99, 99, 98, 98};
			uint32_t ackBits = 0;
			BuildAckList(data, ack, ackBits);
			// 99: bit 31, 98: bit 30
			uint32_t expected = (1u << 31) | (1u << 30);
			Assert::AreEqual(expected, ackBits, L"Bits for 99 and 98 should be set, duplicates ignored");
		}

		// Random order in data
		TEST_METHOD(BuildAckList_RandomOrder)
		{
			uint16_t ack = 100;
			std::vector<uint16_t> data = {95, 99, 97};
			uint32_t ackBits = 0;
			BuildAckList(data, ack, ackBits);
			// 99: bit 31, 97: bit 29, 95: bit 27
			uint32_t expected = (1u << 31) | (1u << 29) | (1u << 27);
			Assert::AreEqual(expected, ackBits, L"Order in data should not matter");
		}

		// Only one packet present
		TEST_METHOD(BuildAckList_OnePresent)
		{
			uint16_t ack = 100;
			std::vector<uint16_t> data = {90}; // 90: bit 9
			uint32_t ackBits = 0;
			BuildAckList(data, ack, ackBits);
			uint32_t expected = (1u << (32 - (100 - 90)));
			Assert::AreEqual(expected, ackBits, L"Only bit for 90 should be set");
		}
	};
}
