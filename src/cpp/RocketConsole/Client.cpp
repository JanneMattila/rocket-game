#include "Client.h"
#include "NetworkPacketType.h"
#include "Utils.h"
#include <NetworkUtilities.h>

static std::string addrToString(const sockaddr_in& addr) {
	char buf[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
	return std::string(buf) + ":" + std::to_string(ntohs(addr.sin_port));
}

static bool operator==(const sockaddr_in& a, const sockaddr_in& b) {
	return a.sin_family == b.sin_family &&
		a.sin_addr.s_addr == b.sin_addr.s_addr &&
		a.sin_port == b.sin_port;
}


Client::Client(std::shared_ptr<Logger> logger, std::unique_ptr<ClientNetwork> network)
	: m_logger(logger), m_network(std::move(network)) {
}

Client::~Client() {
}

int Client::Initialize(std::string server, int port)
{
	return m_network->Initialize(server, port);
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

    if (m_network->Send(*networkPacket) != 0)
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

    if (!m_network->IsServerAddress(clientAddr))
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

    if (m_network->Send(*networkPacket) != 0)
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

    if (!m_network->IsServerAddress(clientAddr))
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
	while (running)
	{
		sockaddr_in serverAddr{};
		int result = 0;

        SendGameState();

		std::unique_ptr<NetworkPacket> networkPacket = m_network->Receive(serverAddr, result);
		if (result == -1)
		{
			m_logger->Log(LogLevel::DEBUG, "Waiting for data");
			continue;
		}
		if (result != 0)
		{
			m_logger->Log(LogLevel::DEBUG, "Failed to receive data");
			continue;
		}

		if (!m_network->IsServerAddress(serverAddr))
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

		switch (networkPacket->ReadNetworkPacketType())
		{
		case NetworkPacketType::GAME_STATE:
			m_logger->Log(LogLevel::DEBUG, "Game state packet received");
            HandleGameState(std::move(networkPacket));
			break;
		case NetworkPacketType::DISCONNECT:
			m_logger->Log(LogLevel::DEBUG, "Disconnect packet received");
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
    int64_t connectionSalt = networkPacket->ReadUInt64();
    if (m_connectionSalt != connectionSalt)
    {
        m_logger->Log(LogLevel::WARNING, "HandleGameState: Incorrect salt", { KV(m_connectionSalt), KV(connectionSalt)});
        return 0;
    }

    uint16_t seqNum = networkPacket->ReadInt16();
    uint16_t ack = networkPacket->ReadInt16();
    uint32_t ackBits = networkPacket->ReadInt32();

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
    }
    else if (diff == 0)
    {
        // TODO: Add stats about duplicate received packets
    }

    diff = NetworkUtilities::SequenceNumberDiff(m_localSequenceNumberSmall, ack);
    auto localSequenceNumberLarge = m_localSequenceNumberLarge + diff;

    NetworkUtilities::VerifyAck(m_sendPackets, localSequenceNumberLarge, ackBits);

    // Clear all acknowledged packets away from send packets
    m_sendPackets.erase(
        std::remove_if(
            m_sendPackets.begin(),
            m_sendPackets.end(),
            [this](const PacketInfo& pi) {
                // Remove if acknowledged
                return pi.acknowledged;
            }
        ),
        m_sendPackets.end()
    );

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

    return 1;
}

void Client::SendGameState()
{
    m_localSequenceNumberLarge++;
    m_localSequenceNumberSmall = m_localSequenceNumberLarge % SEQUENCE_NUMBER_MAX;

    uint32_t ackBits{};
    NetworkUtilities::ComputeAckBits(m_receivedPackets, m_remoteSequenceNumberSmall, ackBits);

    NetworkPacket sendNetworkPacket;
    sendNetworkPacket.WriteInt8(static_cast<int8_t>(NetworkPacketType::GAME_STATE));
    sendNetworkPacket.WriteInt64(m_connectionSalt);
    sendNetworkPacket.WriteInt16(m_localSequenceNumberSmall);
    sendNetworkPacket.WriteInt16(m_remoteSequenceNumberSmall);
    sendNetworkPacket.WriteInt32(ackBits);
    m_network->Send(sendNetworkPacket);

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
            m_network->Send(sendNetworkPacket);
        }
	}
	return 0;
}
