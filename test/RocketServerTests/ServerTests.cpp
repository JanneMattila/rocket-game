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
        std::vector<uint8_t> CreateConnectionRequestPacket(int64_t clientSalt)
        {
            std::vector<uint8_t> data(1000, 0); // Size 1000 as expected by the server

            // Set protocol magic number
            data[0] = 0xFE;

            // Set type as CONNECTION_REQUEST
            data[1] = static_cast<uint8_t>(NetworkPacketType::CONNECTION_REQUEST);

            // Fill CRC placeholders (4 bytes) - we're bypassing validation in our tests
            data[2] = 0xAA;
            data[3] = 0xBB;
            data[4] = 0xCC;
            data[5] = 0xDD;

            // Set client salt at PAYLOAD_START_INDEX (6)
            for (int i = 0; i < 8; i++) {
                data[6 + i] = (clientSalt >> (i * 8)) & 0xFF;
            }

            return data;
        }

        // Helper to create challenge response packet
        std::vector<uint8_t> CreateChallengeResponsePacket(int64_t salt)
        {
            std::vector<uint8_t> data(1000, 0); // Size 1000 as expected by the server

            // Set protocol magic number
            data[0] = 0xFE;

            // Set type as CHALLENGE_RESPONSE
            data[1] = static_cast<uint8_t>(NetworkPacketType::CHALLENGE_RESPONSE);

            // Fill CRC placeholders
            data[2] = 0xAA;
            data[3] = 0xBB;
            data[4] = 0xCC;
            data[5] = 0xDD;

            // Set salt at PAYLOAD_START_INDEX (6)
            for (int i = 0; i < 8; i++) {
                data[6 + i] = (salt >> (i * 8)) & 0xFF;
            }

            return data;
        }

        // Simulates ExecuteGame running in a separate thread for testing
        void RunServerForTestsAsync(Server* server, int executionTimeMs, std::promise<int>& resultPromise)
        {
            auto future = std::async(std::launch::async, [server, executionTimeMs]() {
                std::thread t([server]() {
                    server->ExecuteGame();
                    });

                // Let the server run for a specific time
                std::this_thread::sleep_for(std::chrono::milliseconds(executionTimeMs));
                server->PrepareToQuitGame();

                t.join();
                return 0;
                });

            resultPromise.set_value(future.get());
        }

        // Extract the NetworkPacket from send operations
        NetworkPacket ExtractSentPacket(std::unique_ptr<ServerNetworkStub>& networkStub, sockaddr_in& clientAddr)
        {
            // We use shared_ptr here since we need the data to persist beyond the original stack
            auto sharedPacket = std::make_shared<NetworkPacket>();
            networkStub->SendCaptureCallback = [&clientAddr, sharedPacket](NetworkPacket& packet, sockaddr_in& addr) {
                // Capture the packet
                *sharedPacket = packet;
                // Record where it was sent
                clientAddr = addr;
                return 0;
                };

            return *sharedPacket;
        }

    public:
        TEST_METHOD(Initialization_Test)
        {
            auto udpPort = 12345;
            auto logger = std::make_shared<::Logger>();
            logger->SetLogLevel(LogLevel::DEBUG);

            std::unique_ptr<ServerNetworkStub> network = std::make_unique<ServerNetworkStub>();
            auto server = std::make_unique<Server>(logger, std::move(network));

            Assert::AreEqual(0, server->Initialize(udpPort), L"Server initialization should succeed");
        }

        TEST_METHOD(Initialization_Failed_Test)
        {
            auto udpPort = 12345;
            auto logger = std::make_shared<::Logger>();
            logger->SetLogLevel(LogLevel::DEBUG);

            std::unique_ptr<ServerNetworkStub> network = std::make_unique<ServerNetworkStub>();

            // Set up network to fail initialization
            network->InitializeReturnValues = { 1 };  // Return error code 1

            auto server = std::make_unique<Server>(logger, std::move(network));
            int result = server->Initialize(udpPort);

            Assert::AreEqual(1, result, L"Server initialization should fail with error code 1");
        }

        TEST_METHOD(Connection_Request_Test)
        {
            auto udpPort = 12345;
            auto logger = std::make_shared<::Logger>();
            logger->SetLogLevel(LogLevel::DEBUG);

            std::unique_ptr<ServerNetworkStub> network = std::make_unique<ServerNetworkStub>();

            // Generate fixed client salt for testing
            const int64_t CLIENT_SALT = 0x1234567890ABCDEF;

            // Create a connection request packet
            auto requestPacket = CreateConnectionRequestPacket(CLIENT_SALT);

            // Set up network to return the packet and then nothing
            network->ReceiveReturnValues = { 0, -1 }; // Success, then waiting
            network->ReceiveDataReturnValues = { requestPacket };

            // Set up network to capture sent packets
            sockaddr_in sentAddr{};
            NetworkPacket sentPacket;
            network->SendCaptureCallback = [&sentAddr, &sentPacket](NetworkPacket& packet, sockaddr_in& addr) {
                sentPacket = packet;
                sentAddr = addr;
                return 0;
                };

            auto server = std::make_unique<Server>(logger, std::move(network));
            server->Initialize(udpPort);

            // Run server for a short time
            std::promise<int> resultPromise;
            RunServerForTestsAsync(server.get(), 100, resultPromise);
            resultPromise.get_future().wait();

            // Verify that the server sent a challenge packet
            Assert::AreEqual(NetworkPacketType::CHALLENGE,
                sentPacket.GetNetworkPacketType(),
                L"Server should respond with CHALLENGE packet");
        }

        TEST_METHOD(Challenge_Response_Accepted_Test)
        {
            auto udpPort = 12345;
            auto logger = std::make_shared<::Logger>();
            logger->SetLogLevel(LogLevel::DEBUG);

            std::unique_ptr<ServerNetworkStub> network = std::make_unique<ServerNetworkStub>();
            sockaddr_in clientAddr{};
            clientAddr.sin_family = AF_INET;
            clientAddr.sin_port = htons(54321);
            inet_pton(AF_INET, "127.0.0.1", &clientAddr.sin_addr);

            // Generate fixed salts for testing
            const int64_t CLIENT_SALT = 0x1234567890ABCDEF;
            const int64_t SERVER_SALT = 0xFEDCBA0987654321;
            const int64_t COMBINED_SALT = CLIENT_SALT ^ SERVER_SALT;

            // Create connection and challenge packets
            auto requestPacket = CreateConnectionRequestPacket(CLIENT_SALT);
            auto challengeResponsePacket = CreateChallengeResponsePacket(COMBINED_SALT);

            // Set up network to return packets in sequence
            network->ReceiveReturnValues = { 0, 0, -1 }; // Success, success, then waiting
            network->ReceiveDataReturnValues = { requestPacket, challengeResponsePacket };

            // Set up to track sent packets
            std::vector<NetworkPacket> sentPackets;
            std::vector<sockaddr_in> sentAddresses;

            network->SendCaptureCallback = [&sentPackets, &sentAddresses](NetworkPacket& packet, sockaddr_in& addr) {
                NetworkPacket copy;
                copy.Clear();

                // Save the packet type
                copy.WriteInt8(static_cast<int8_t>(packet.GetNetworkPacketType()));

                // If it's CONNECTION_ACCEPTED, save the player ID too
                if (packet.GetNetworkPacketType() == NetworkPacketType::CONNECTION_ACCEPTED) {
                    copy.WriteInt64(packet.ReadInt64());
                }

                sentPackets.push_back(copy);
                sentAddresses.push_back(addr);
                return 0;
                };

            auto server = std::make_unique<Server>(logger, std::move(network));
            server->Initialize(udpPort);

            // Run server for a short time
            std::promise<int> resultPromise;
            RunServerForTestsAsync(server.get(), 200, resultPromise);
            resultPromise.get_future().wait();

            // Verify the server sent CHALLENGE and CONNECTION_ACCEPTED packets
            Assert::AreEqual((size_t)2, sentPackets.size(), L"Server should have sent 2 packets");
            Assert::AreEqual(NetworkPacketType::CHALLENGE,
                sentPackets[0].GetNetworkPacketType(),
                L"First response should be CHALLENGE");
            Assert::AreEqual(NetworkPacketType::CONNECTION_ACCEPTED,
                sentPackets[1].GetNetworkPacketType(),
                L"Second response should be CONNECTION_ACCEPTED");
        }

        TEST_METHOD(Challenge_Response_Rejected_Test)
        {
            auto udpPort = 12345;
            auto logger = std::make_shared<::Logger>();
            logger->SetLogLevel(LogLevel::DEBUG);

            std::unique_ptr<ServerNetworkStub> network = std::make_unique<ServerNetworkStub>();
            sockaddr_in clientAddr{};
            clientAddr.sin_family = AF_INET;
            clientAddr.sin_port = htons(54321);
            inet_pton(AF_INET, "127.0.0.1", &clientAddr.sin_addr);

            // Generate fixed salts for testing
            const int64_t CLIENT_SALT = 0x1234567890ABCDEF;
            const int64_t SERVER_SALT = 0xFEDCBA0987654321;
            const int64_t INCORRECT_SALT = 0x0000000000000000; // Wrong salt

            // Create connection and challenge packets
            auto requestPacket = CreateConnectionRequestPacket(CLIENT_SALT);
            auto challengeResponsePacket = CreateChallengeResponsePacket(INCORRECT_SALT);

            // Set up network to return packets in sequence
            network->ReceiveReturnValues = { 0, 0, -1 }; // Success, success, then waiting
            network->ReceiveDataReturnValues = { requestPacket, challengeResponsePacket };

            // Set up to track sent packets
            std::vector<NetworkPacket> sentPackets;
            std::vector<sockaddr_in> sentAddresses;

            network->SendCaptureCallback = [&sentPackets, &sentAddresses](NetworkPacket& packet, sockaddr_in& addr) {
                NetworkPacket copy;
                copy.Clear();
                copy.WriteInt8(static_cast<int8_t>(packet.GetNetworkPacketType()));
                sentPackets.push_back(copy);
                sentAddresses.push_back(addr);
                return 0;
                };

            auto server = std::make_unique<Server>(logger, std::move(network));
            server->Initialize(udpPort);

            // Run server for a short time
            std::promise<int> resultPromise;
            RunServerForTestsAsync(server.get(), 200, resultPromise);
            resultPromise.get_future().wait();

            // Verify the server sent CHALLENGE and CONNECTION_DENIED packets
            Assert::AreEqual((size_t)2, sentPackets.size(), L"Server should have sent 2 packets");
            Assert::AreEqual(NetworkPacketType::CHALLENGE,
                sentPackets[0].GetNetworkPacketType(),
                L"First response should be CHALLENGE");
            Assert::AreEqual(NetworkPacketType::CONNECTION_DENIED,
                sentPackets[1].GetNetworkPacketType(),
                L"Second response should be CONNECTION_DENIED");
        }

        TEST_METHOD(Invalid_Packet_Size_Test)
        {
            auto udpPort = 12345;
            auto logger = std::make_shared<::Logger>();
            logger->SetLogLevel(LogLevel::DEBUG);

            std::unique_ptr<ServerNetworkStub> network = std::make_unique<ServerNetworkStub>();

            // Create an invalid-sized packet (should be 1000 for connection packets)
            std::vector<uint8_t> tooSmallPacket(10, 0);
            tooSmallPacket[0] = 0xFE; // Magic number
            tooSmallPacket[1] = static_cast<uint8_t>(NetworkPacketType::CONNECTION_REQUEST);

            // Set up network to return the small packet and then wait
            network->ReceiveReturnValues = { 0, -1 };
            network->ReceiveDataReturnValues = { tooSmallPacket };

            // Track if any response is sent (should not happen)
            bool responseSent = false;
            network->SendCaptureCallback = [&responseSent](NetworkPacket& packet, sockaddr_in& addr) {
                responseSent = true;
                return 0;
                };

            auto server = std::make_unique<Server>(logger, std::move(network));
            server->Initialize(udpPort);

            // Run server for a short time
            std::promise<int> resultPromise;
            RunServerForTestsAsync(server.get(), 100, resultPromise);
            resultPromise.get_future().wait();

            // Verify no response was sent for the invalid packet
            Assert::IsFalse(responseSent, L"Server should not respond to invalid-sized packets");
        }

        TEST_METHOD(Invalid_Packet_Validation_Test)
        {
            auto udpPort = 12345;
            auto logger = std::make_shared<::Logger>();
            logger->SetLogLevel(LogLevel::DEBUG);

            std::unique_ptr<ServerNetworkStub> network = std::make_unique<ServerNetworkStub>();

            // Create a properly sized but invalid packet
            std::vector<uint8_t> invalidPacket(1000, 0);
            // Magic number is missing
            invalidPacket[1] = static_cast<uint8_t>(NetworkPacketType::CONNECTION_REQUEST);

            // Setup network with custom validation failure
            network->ReceiveReturnValues = { 0, -1 };
            network->ReceiveDataReturnValues = { invalidPacket };

            // Override Validate() to return error
            network->ValidateReturnValue = 1; // Non-zero indicates validation failure

            // Track if any response is sent (should not happen)
            bool responseSent = false;
            network->SendCaptureCallback = [&responseSent](NetworkPacket& packet, sockaddr_in& addr) {
                responseSent = true;
                return 0;
                };

            auto server = std::make_unique<Server>(logger, std::move(network));
            server->Initialize(udpPort);

            // Run server for a short time
            std::promise<int> resultPromise;
            RunServerForTestsAsync(server.get(), 100, resultPromise);
            resultPromise.get_future().wait();

            // Verify no response was sent for the invalid packet
            Assert::IsFalse(responseSent, L"Server should not respond to packets that fail validation");
        }

        TEST_METHOD(Network_Send_Failure_Test)
        {
            auto udpPort = 12345;
            auto logger = std::make_shared<::Logger>();
            logger->SetLogLevel(LogLevel::DEBUG);

            std::unique_ptr<ServerNetworkStub> network = std::make_unique<ServerNetworkStub>();

            // Generate fixed client salt for testing
            const int64_t CLIENT_SALT = 0x1234567890ABCDEF;

            // Create a connection request packet
            auto requestPacket = CreateConnectionRequestPacket(CLIENT_SALT);

            // Set up network to return the packet and then nothing
            network->ReceiveReturnValues = { 0, -1 }; // Success, then waiting
            network->ReceiveDataReturnValues = { requestPacket };

            // Set up network to fail on send
            network->SendReturnValues = { 1 }; // Error code

            auto server = std::make_unique<Server>(logger, std::move(network));
            server->Initialize(udpPort);

            // Run server for a short time
            std::promise<int> resultPromise;
            RunServerForTestsAsync(server.get(), 100, resultPromise);
            resultPromise.get_future().wait();

            // Server should handle the send failure gracefully (which we confirm by reaching here)
        }

        TEST_METHOD(Multiple_Players_Test)
        {
            auto udpPort = 12345;
            auto logger = std::make_shared<::Logger>();
            logger->SetLogLevel(LogLevel::DEBUG);

            std::unique_ptr<ServerNetworkStub> network = std::make_unique<ServerNetworkStub>();

            // Create connection packets for multiple players
            const int64_t CLIENT_SALT1 = 0x1111111111111111;
            const int64_t CLIENT_SALT2 = 0x2222222222222222;

            auto requestPacket1 = CreateConnectionRequestPacket(CLIENT_SALT1);
            auto requestPacket2 = CreateConnectionRequestPacket(CLIENT_SALT2);

            // Create different client addresses
            sockaddr_in clientAddr1{};
            clientAddr1.sin_family = AF_INET;
            clientAddr1.sin_port = htons(54321);
            inet_pton(AF_INET, "127.0.0.1", &clientAddr1.sin_addr);

            sockaddr_in clientAddr2{};
            clientAddr2.sin_family = AF_INET;
            clientAddr2.sin_port = htons(54322); // Different port
            inet_pton(AF_INET, "127.0.0.1", &clientAddr2.sin_addr);

            // Set up network to simulate two connection requests from different clients
            network->ReceiveReturnValues = { 0, 0, -1 }; // Success, success, then waiting
            network->ReceiveDataReturnValues = { requestPacket1, requestPacket2 };
            network->ReceiveAddressReturnValues = { clientAddr1, clientAddr2 };

            // Track sent packets
            std::vector<NetworkPacket> sentPackets;
            std::vector<sockaddr_in> sentAddresses;

            network->SendCaptureCallback = [&sentPackets, &sentAddresses](NetworkPacket& packet, sockaddr_in& addr) {
                NetworkPacket copy;
                copy.Clear();
                copy.WriteInt8(static_cast<int8_t>(packet.GetNetworkPacketType()));
                sentPackets.push_back(copy);
                sentAddresses.push_back(addr);
                return 0;
                };

            auto server = std::make_unique<Server>(logger, std::move(network));
            server->Initialize(udpPort);

            // Run server for a short time
            std::promise<int> resultPromise;
            RunServerForTestsAsync(server.get(), 200, resultPromise);
            resultPromise.get_future().wait();

            // Verify the server sent responses to both clients
            Assert::AreEqual((size_t)2u, sentPackets.size(), L"Server should have sent responses to both clients");
            Assert::AreEqual(NetworkPacketType::CHALLENGE,
                sentPackets[0].GetNetworkPacketType(),
                L"Response to first client should be CHALLENGE");
            Assert::AreEqual(NetworkPacketType::CHALLENGE,
                sentPackets[1].GetNetworkPacketType(),
                L"Response to second client should be CHALLENGE");

            // Verify packets were sent to correct clients
            Assert::AreEqual((uint16_t)54321, ntohs(sentAddresses[0].sin_port),
                L"First response should be sent to first client");
            Assert::AreEqual((uint16_t)54322, ntohs(sentAddresses[1].sin_port),
                L"Second response should be sent to second client");
        }
    };
}
