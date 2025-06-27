#include <thread>
#include "Client.h"
#include "NetworkPacketType.h"
#include "Utils.h"
#include "NetworkUtilities.h"

Client::Client(std::shared_ptr<Logger> logger, std::unique_ptr<Network> network)
	: m_logger(logger), m_network(std::move(network)) {
}

Client::~Client() {
}

int Client::Initialize(std::string server, int port)
{
	return m_network->Initialize(server, port, m_serverAddr);
}

int Client::EstablishConnection()
{
    m_clientSalt = 0;
    m_serverSalt = 0;
    m_connectionSalt = 0;

    uint64_t clientSalt = Utils::GetRandomNumberUInt64();
    std::unique_ptr<NetworkPacket> networkPacket = std::make_unique<NetworkPacket>();

    m_connectionState = NetworkConnectionState::CONNECTING;
    networkPacket->WriteInt8(static_cast<int8_t>(NetworkPacketType::CONNECTION_REQUEST));
    networkPacket->WriteUInt64(clientSalt);

    // Pad the packet to 1000 bytes
    for (size_t i = 0; i < 1000
        - sizeof(uint32_t) /* crc32 */
        - sizeof(uint8_t) /* packet type */
        - sizeof(uint64_t) /* client salt */; i++)
    {
        networkPacket->WriteInt8(0);
    }

    int result = 0;

    m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Send connection request with client salt", { KV(clientSalt) });

    if (m_network->Send(*networkPacket, m_serverAddr) != 0)
    {
        m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Failed to send connection request");
        return 1;
    }

    m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Waiting for challenge");

    // Sleep for a short period to allow the server to respond
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    sockaddr_in clientAddr{};
    std::unique_ptr<NetworkPacket> challengePacket = m_network->Receive(clientAddr, result);

    if (result != 0)
    {
        m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Timeout waiting for response to connection request");
        return 1;
    }

    if (!NetworkUtilities::IsSameAddress(clientAddr, m_serverAddr))
    {
        m_connectionState = NetworkConnectionState::DISCONNECTED;
        m_logger->Log(LogLevel::WARNING, "EstablishConnection: Received data from unknown address");
        return 1;
    }

    if (challengePacket->ReadAndValidateCRC())
    {
        m_connectionState = NetworkConnectionState::DISCONNECTED;
        m_logger->Log(LogLevel::WARNING, "EstablishConnection: Packet validation failed");
        return 1;
    }

    if (challengePacket->ReadNetworkPacketType() != NetworkPacketType::CHALLENGE)
    {
        m_connectionState = NetworkConnectionState::DISCONNECTED;
        m_logger->Log(LogLevel::WARNING, "EstablishConnection: Invalid packet type");
        return 1;
    }

    uint64_t receivedClientSalt = challengePacket->ReadUInt64();
    uint64_t serverSalt = challengePacket->ReadUInt64();
    m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Received challenge with client and server salt", { KV(receivedClientSalt), KV(serverSalt) });

    if (receivedClientSalt != clientSalt)
    {
        m_connectionState = NetworkConnectionState::DISCONNECTED;
        m_logger->Log(LogLevel::WARNING, "EstablishConnection: Client salt mismatch", { KV(clientSalt), KV(receivedClientSalt) });
        return 1;
    }

    uint64_t connectionSalt = clientSalt ^ serverSalt;
    networkPacket->Clear();

    // Create a new packet with the connection salt
    networkPacket->WriteInt8(static_cast<int8_t>(NetworkPacketType::CHALLENGE_RESPONSE));
    networkPacket->WriteUInt64(connectionSalt);
    // Pad the packet to 1000 bytes
    for (size_t i = 0; i < 1000
        - sizeof(uint32_t) /* crc32 */
        - sizeof(uint8_t) /* packet type */
        - sizeof(uint64_t) /* connection salt */; i++)
    {
        networkPacket->WriteInt8(0);
    }

    m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Sending connection salt", { KV(connectionSalt) });

    if (m_network->Send(*networkPacket, m_serverAddr) != 0)
    {
        m_connectionState = NetworkConnectionState::DISCONNECTED;
        m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Failed to send challenge request");
        return 1;
    }

    m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Waiting for challenge response");

    // Sleep for a short period to allow the server to respond
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::unique_ptr<NetworkPacket> challengeResponsePacket = m_network->Receive(clientAddr, result);

    if (result != 0)
    {
        m_connectionState = NetworkConnectionState::DISCONNECTED;
        m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Timeout waiting for response to challenge request");
        return 1;
    }

    if (!NetworkUtilities::IsSameAddress(clientAddr, m_serverAddr))
    {
        m_connectionState = NetworkConnectionState::DISCONNECTED;
        m_logger->Log(LogLevel::WARNING, "EstablishConnection: Received data from unknown address");
        return 1;
    }

    if (challengeResponsePacket->ReadAndValidateCRC())
    {
        m_connectionState = NetworkConnectionState::DISCONNECTED;
        m_logger->Log(LogLevel::WARNING, "EstablishConnection: Challenge packet validation failed");
        return 1;
    }

    if (challengeResponsePacket->ReadNetworkPacketType() != NetworkPacketType::CONNECTION_ACCEPTED)
    {
        m_connectionState = NetworkConnectionState::DISCONNECTED;
        m_logger->Log(LogLevel::WARNING, "EstablishConnection: Connection was not accepted");
        return 1;
    }

    m_connectionState = NetworkConnectionState::CONNECTED;
    m_clientSalt = clientSalt;
    m_serverSalt = serverSalt;
    m_connectionSalt = clientSalt ^ serverSalt;
    m_logger->Log(LogLevel::INFO, "EstablishConnection: Connected");

    return 0;
}

int Client::ExecuteGame(volatile std::sig_atomic_t& running)
{
	// Main loop
    const auto gameUpdateInterval = std::chrono::duration<double>(1.0 / 60.0); // 1/60 second
    auto gameUpdateTime = std::chrono::steady_clock::now();

    int idleTime = 0;
    auto idle = std::chrono::steady_clock::now();
    // TODO: Add timer to send stats to server every 10 seconds

	while (running)
	{
		sockaddr_in serverAddr{};
		int result = 0;

        auto current = std::chrono::steady_clock::now();
        auto elapsed = current - gameUpdateTime;
        if (elapsed >= gameUpdateInterval)
        {
            SendGameState();
            gameUpdateTime = current;
        }

		std::unique_ptr<NetworkPacket> networkPacket = m_network->Receive(serverAddr, result);
		if (result == -1)
		{
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - idle).count() >= 5000 /* 5 seconds */)
            {
                idle = now;
                m_logger->Log(LogLevel::DEBUG, "Waiting for data");
                idleTime++;
                if (idleTime > 20)
                {
                    m_logger->Log(LogLevel::INFO, "No data received for a while, exiting");
                    running = 0;
                }
            }

			continue;
		}

		if (result != 0)
		{
			m_logger->Log(LogLevel::DEBUG, "Failed to receive data");
			continue;
		}

		if (!NetworkUtilities::IsSameAddress(serverAddr, m_serverAddr))
		{
			m_logger->Log(LogLevel::DEBUG, "Received data from unknown address");
			continue;
		}

		size_t size = networkPacket->Size();
		if (size < CRC32::CRC_SIZE)
		{
			m_logger->Log(LogLevel::WARNING, "Received too small packet", { KV(size) });
			continue;
		}

		if (networkPacket->ReadAndValidateCRC())
		{
			m_logger->Log(LogLevel::WARNING, "Invalid packet");
			continue;
		}

        idleTime = 0;
        NetworkPacketType packetType = networkPacket->ReadNetworkPacketType();
        auto packetTypeInt = static_cast<int>(packetType);
        m_logger->Log(LogLevel::DEBUG, "Received packet type", { KV(packetTypeInt) });

		switch (packetType)
		{
		case NetworkPacketType::GAME_STATE:
			m_logger->Log(LogLevel::DEBUG, "Game state packet received");
            HandleGameState(std::move(networkPacket));
			break;
		case NetworkPacketType::DISCONNECT:
            m_logger->Log(LogLevel::INFO, "Received disconnected packet from server");
            running = false;
			break;
		default:
			m_logger->Log(LogLevel::WARNING, "Unknown packet type");
			break;
		}
	}

	return 0;
}

int Client::HandleGameState(std::unique_ptr<NetworkPacket> networkPacket)
{
    GamePacket* gamePacket = static_cast<GamePacket*>(networkPacket.get());
    int64_t connectionSalt = gamePacket->ReadUInt64();
    if (m_connectionSalt != connectionSalt)
    {
        m_logger->Log(LogLevel::WARNING, "HandleGameState: Incorrect salt", { KV(m_connectionSalt), KV(connectionSalt)});
        return 0;
    }

    uint16_t seqNum = gamePacket->ReadInt16();
    uint16_t ack = gamePacket->ReadInt16();
    uint32_t ackBits = gamePacket->ReadInt32();

    uint16_t diff = NetworkUtilities::SequenceNumberDiff(m_remoteSequenceNumberSmall, seqNum);

    if (diff > 0)
    {
        m_remoteSequenceNumberLarge += diff;
        m_remoteSequenceNumberSmall = seqNum;

        // TODO: This or NetworkUtilities::StoreAcks(...)
        m_receivedPackets.push_back(m_remoteSequenceNumberLarge);
    }
    else if (diff < 0)
    {
        // TODO: Add stats about out-of-order received packets
        m_logger->Log(LogLevel::WARNING, "HandleGameState out-of-order packets");
    }
    else if (diff == 0)
    {
        // TODO: Add stats about duplicate received packets
        m_logger->Log(LogLevel::WARNING, "HandleGameState duplicate packets");
    }

    diff = NetworkUtilities::SequenceNumberDiff(m_localSequenceNumberSmall, ack);
    auto localSequenceNumberLarge = m_localSequenceNumberLarge - diff;

    NetworkUtilities::VerifyAck(m_sendPackets, localSequenceNumberLarge, ackBits);

    // Clear all acknowledged packets away from send packets
    int acknowledgedCount = 0;
    std::chrono::steady_clock::duration roundTripTime{};
    m_sendPackets.erase(
        std::remove_if(
            m_sendPackets.begin(),
            m_sendPackets.end(),
            [this, &acknowledgedCount, &roundTripTime](const PacketInfo& pi) {
                // Remove if acknowledged
                acknowledgedCount++;
                roundTripTime += pi.roundTripTime;
                return pi.acknowledged;
            }
        ),
        m_sendPackets.end()
    );

    // Average round trip time
    if (acknowledgedCount > 0)
    {
        roundTripTime /= acknowledgedCount;
        auto roundTripTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(roundTripTime).count();
        m_logger->Log(LogLevel::INFO, "HandleGameState: Average round trip time in ms", { KV(roundTripTimeMs), KV(acknowledgedCount)});
    }
    else
    {
        m_logger->Log(LogLevel::DEBUG, "HandleGameState: No packets acknowledged");
    }

    // Keep only 60 received packets
    if (m_receivedPackets.size() > 60)
    {
        m_receivedPackets.erase(
            m_receivedPackets.begin(),
            m_receivedPackets.begin() + (m_receivedPackets.size() - 60)
        );
    }

    auto sendPacketsRemaining = m_sendPackets.size();
    auto receivedPacketsRemaining = m_receivedPackets.size();
    m_logger->Log(LogLevel::DEBUG, "HandleGameState", { KV(m_remoteSequenceNumberLarge), KV(m_remoteSequenceNumberSmall), KV(sendPacketsRemaining), KV(receivedPacketsRemaining) });

    std::vector<PlayerState> playerStates = gamePacket->DeserializePlayerStates();

    return 1;
}

void Client::SendGameState()
{
    m_localSequenceNumberLarge++;
    m_localSequenceNumberSmall = m_localSequenceNumberLarge % SEQUENCE_NUMBER_MAX;

    uint32_t ackBits{};
    NetworkUtilities::ComputeAckBits(m_receivedPackets, m_remoteSequenceNumberSmall, ackBits);

    GamePacket sendNetworkPacket;
    sendNetworkPacket.WriteInt8(static_cast<int8_t>(NetworkPacketType::GAME_STATE));
    sendNetworkPacket.WriteInt64(m_connectionSalt);
    sendNetworkPacket.WriteInt16(m_localSequenceNumberSmall);
    sendNetworkPacket.WriteInt16(m_remoteSequenceNumberSmall);
    sendNetworkPacket.WriteInt32(ackBits);

    // Serialize player state
    PlayerState playerState{};
    playerState.playerID = 1; // Assuming player ID is 1 for this example
    playerState.pos.x.floatValue = 100.0f; // Example position
    playerState.pos.y.floatValue = 200.0f; // Example position
    playerState.vel.x.floatValue = 1.0f; // Example velocity
    playerState.vel.y.floatValue = 1.0f; // Example velocity
    playerState.speed.floatValue = 10.0f; // Example speed
    playerState.rotation.floatValue = 0.0f; // Example rotation
    playerState.keyboard.up = true; // Example keyboard state
    playerState.keyboard.down = false;
    playerState.keyboard.left = false;
    playerState.keyboard.right = true;
    playerState.keyboard.space = false;
    sendNetworkPacket.SerializePlayerState(playerState);

    m_network->Send(sendNetworkPacket, m_serverAddr);

    PacketInfo pi;
    pi.seqNum = m_localSequenceNumberLarge;
    pi.sendTicks = std::chrono::steady_clock::now();
    m_sendPackets.push_back(pi);

    m_logger->Log(LogLevel::DEBUG, "SendGameState", { KV(m_localSequenceNumberLarge), KV(m_localSequenceNumberSmall) });
}

int Client::QuitGame()
{
	m_logger->Log(LogLevel::DEBUG, "Game is stopping");

	if (!m_serverInitializedShutdown)
	{
		// Send disconnect packets to server
        for (size_t i = 0; i < 10; i++)
        {
            NetworkPacket sendNetworkPacket;
            sendNetworkPacket.WriteInt8(static_cast<int8_t>(NetworkPacketType::DISCONNECT));
            sendNetworkPacket.WriteInt64(m_connectionSalt);
            m_network->Send(sendNetworkPacket, m_serverAddr);
        }
	}
	return 0;
}
