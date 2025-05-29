#include "pch.h"
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
	};
}
