#include "pch.h"
#include <vector>
#include "CppUnitTest.h"
#include "NetworkUtilities.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace RocketServerTests
{
	TEST_CLASS(SequenceNumberTests)
	{
	private:

	public:
		TEST_METHOD(Next_Sequence_Number_Normal1_Test)
		{
			// Arrange
			uint16_t previous = 64000;
			uint16_t next = 64001;
            uint16_t expected = 1;

			// Act
            uint16_t actual = NetworkUtilities::SequenceNumberDiff(previous, next);

			// Assert
			Assert::AreEqual(expected, actual, L"Validation should have failed");
		}

		TEST_METHOD(Next_Sequence_Number_Normal2_Test)
		{
			// Arrange
			uint16_t previous = 62000;
			uint16_t next = 64001;
            uint16_t expected = 2001;

			// Act
            uint16_t actual = NetworkUtilities::SequenceNumberDiff(previous, next);

			// Assert
			Assert::AreEqual(expected, actual, L"Validation should have failed");
		}

		TEST_METHOD(Next_Sequence_Number_Rollover1_Test)
		{
			// Arrange
			uint16_t previous = 65535;
			uint16_t next = 0;
            uint16_t expected = 1;

			// Act
            uint16_t actual = NetworkUtilities::SequenceNumberDiff(previous, next);

			// Assert
			Assert::AreEqual(expected, actual, L"Validation should have failed");
		}

		TEST_METHOD(Next_Sequence_Number_Rollover2_Test)
		{
			// Arrange
			uint16_t previous = 65035;
			uint16_t next = 150;
            uint16_t expected = 651; // 500 + 150 + zero index = 651

			// Act
            uint16_t actual = NetworkUtilities::SequenceNumberDiff(previous, next);

			// Assert
			Assert::AreEqual(expected, actual, L"Validation should have failed");
		}

        TEST_METHOD(Remote_Sequence_Number_To_Local_Test)
        {
            // Arrange
            uint16_t previousRemote = 65535;
            uint16_t nextRemote = 0;
            uint64_t convertedRemote = 100000;
            uint64_t expectedConvertedRemote = 100001;

            // Act
            uint16_t diff = NetworkUtilities::SequenceNumberDiff(previousRemote, nextRemote);
            uint64_t actualConvertedRemote = convertedRemote + diff;

            // Assert
            Assert::AreEqual(expectedConvertedRemote, actualConvertedRemote, L"Validation should have failed");
        }

        TEST_METHOD(Sequence_Number_To_Ordered_List)
        {
            // Arrange
            std::vector<uint16_t> before = { 100, 101, 103, 104 };
            uint16_t data = 102;
            uint64_t expectedIndex = 2;

            // Act
            auto it = std::lower_bound(before.begin(), before.end(), data);
            uint64_t actualIndex = std::distance(before.begin(), it);

            // Assert
            Assert::AreEqual(expectedIndex, actualIndex, L"Validation should have failed");
        }
	};
}
