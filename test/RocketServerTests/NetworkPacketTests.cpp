#include "pch.h"
#include "CppUnitTest.h"
#include "NetworkPacket.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace RocketServerTests
{
    TEST_CLASS(NetworkPacketTests)
    {
    private:
    public:
        TEST_METHOD(Validation_Failed_Test)
        {
            // Arrange
            std::vector<uint8_t> data = { 1, 2, 3, 4, 10, 20, 30 };
            NetworkPacket networkPacket(data);
            int expected = 1;

            // Act
            int actual = networkPacket.ReadAndValidateCRC();

            // Assert
            Assert::AreEqual(expected, actual, L"Validation should have failed");
        }

        TEST_METHOD(Validation_Succeeded_Test)
        {
            // Arrange
            std::vector<uint8_t> data = { 158, 203, 209, 104, 10, 20, 30 };
            NetworkPacket networkPacket(data);
            int expected = 0;

            // Act
            int actual = networkPacket.ReadAndValidateCRC();

            // Assert
            Assert::AreEqual(expected, actual, L"Validation should have succeeded");
        }
    };
}
