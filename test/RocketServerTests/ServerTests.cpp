#include "pch.h"
#include "CppUnitTest.h"
#include "Logger.h"
#include "Server.h"
#include "ServerNetworkStub.h"
#include "NetworkPacketType.h"
#include "Utils.h"
#include <thread>
#include <future>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Specialization of ToString for NetworkPacketType
namespace Microsoft {
    namespace VisualStudio {
        namespace CppUnitTestFramework {
            template<>
            static std::wstring ToString<NetworkPacketType>(const NetworkPacketType& packetType) {
                switch (packetType) {
				case NetworkPacketType::CONNECTION_REQUEST: return L"CONNECTION_REQUEST";
				case NetworkPacketType::CHALLENGE_RESPONSE: return L"CHALLENGE_RESPONSE";
				case NetworkPacketType::CONNECTION_ACCEPTED: return L"CONNECTION_ACCEPTED";
				case NetworkPacketType::CONNECTION_DENIED: return L"CONNECTION_DENIED";
				case NetworkPacketType::CHALLENGE: return L"CHALLENGE";
                default: return L"Unknown NetworkPacketType";
                }
            }
        }
    }
}

namespace RocketServerTests
{
    TEST_CLASS(ServerTests)
    {
    private:
        // Helper to create a basic connection request packet
        std::unique_ptr<NetworkPacket> CreateConnectionPacket(NetworkPacketType networkPacketType, int64_t salt)
        {
            std::unique_ptr<NetworkPacket> networkPacket = std::make_unique<NetworkPacket>();
			networkPacket->WriteInt8(static_cast<int8_t>(networkPacketType));
			networkPacket->WriteInt64(salt); // Client salt

			// Pad the rest of the packet with zeros
			for (size_t i = 0; i < 1000 - sizeof(int8_t) - sizeof(int64_t); i++)
            {
				networkPacket->WriteInt8(0x00);
			}

            return std::move(networkPacket);
        }

    public:
        TEST_METHOD(Initialization_Succeed_Test)
        {
            // Arrange
            std::unique_ptr<ServerNetworkStub> network = std::make_unique<ServerNetworkStub>();
            std::unique_ptr<Server> server = std::make_unique<Server>(std::make_shared<::Logger>(), std::move(network));
			int expected = 0; // Expected return value for successful initialization

			// Act
            int actual = server->Initialize(12345);

            Assert::AreEqual(expected, actual, L"Server initialization should succeed");
        }

        TEST_METHOD(Initialization_Failed_Test)
        {
			// Arrange
            std::unique_ptr<ServerNetworkStub> network = std::make_unique<ServerNetworkStub>();
            network->InitializeReturnValues = { 1 };  // Return error code 1
            std::unique_ptr<Server> server = std::make_unique<Server>(std::make_shared<::Logger>(), std::move(network));
			int expected = 1; // Expected return value for failed initialization

			// Act
            int actual = server->Initialize(12345);

			// Assert
            Assert::AreEqual(expected, actual, L"Server initialization should fail with error code 1");
        }

        TEST_METHOD(Connection_Request_Test)
        {
			// Arrange
            std::shared_ptr<ServerNetworkStub> network = std::make_shared<ServerNetworkStub>();
            const int64_t CLIENT_SALT = 0x1234567890ABCDEF;
            std::unique_ptr<NetworkPacket> connectionRequestPacket = CreateConnectionPacket(NetworkPacketType::CONNECTION_REQUEST, CLIENT_SALT);
            auto server = std::make_unique<Server>(std::make_shared<::Logger>(), network);
			sockaddr_in clientAddr{};
			int expected = 0; // Expected return value for successful handling
            size_t sendExpected = 1; // Expected send count
            
			// Act
            int actual = server->HandleConnectionRequest(std::move(connectionRequestPacket), clientAddr);

            // Assert
            Assert::AreEqual(expected, actual, L"Connection");

            size_t sendActual = network->SendData.size();
			Assert::AreEqual(sendExpected, sendActual, L"Server should send a response after handling connection request");

            NetworkPacket sendPacket(network->SendData[0]);
            NetworkPacketType packetType = sendPacket.GetNetworkPacketType();
            Assert::AreEqual(NetworkPacketType::CHALLENGE, packetType, L"Packet type should be CHALLENGE");
        }
    };
}
