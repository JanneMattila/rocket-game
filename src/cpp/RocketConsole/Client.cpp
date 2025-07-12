#include <thread>
#include <numeric>
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

    // TODO: Add clock synchronization

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

void Client::SyncClock()
{
    std::vector<int64_t> clockOffsets;
    if (m_connectionState != NetworkConnectionState::CONNECTED)
    {
        m_logger->Log(LogLevel::WARNING, "SyncClock: Not connected to server");
        return;
    }

    if (m_connectionSalt == 0)
    {
        m_logger->Log(LogLevel::WARNING, "SyncClock: Connection salt is not set");
        return;
    }

    for (size_t i = 0; i < 5; i++)
    {
        m_serverClockOffset = 0; // Reset the server clock offset

        m_logger->Log(LogLevel::DEBUG, "SyncClock: Sending clock sync request");
        NetworkPacket networkPacket;
        networkPacket.WriteInt8(static_cast<int8_t>(NetworkPacketType::CLOCK));
        networkPacket.WriteInt64(m_connectionSalt);

        // Get current time
        auto sendNow = std::chrono::steady_clock::now();
        auto sendNowEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(sendNow.time_since_epoch()).count();
        networkPacket.WriteInt64(sendNowEpoch);

        if (m_network->Send(networkPacket, m_serverAddr) != 0)
        {
            m_connectionState = NetworkConnectionState::DISCONNECTED;
            m_logger->Log(LogLevel::DEBUG, "SyncClock: Failed to send clock sync packet");
            return;
        }

        // Get the current time and give 500ms time for server to respond

        m_logger->Log(LogLevel::DEBUG, "SyncClock: Waiting for clock sync response");

        while (true)
        {
            sockaddr_in clientAddr{};
            int result = 0;
            std::unique_ptr<NetworkPacket> responsePacket = m_network->Receive(clientAddr, result);
            if (result != 0)
            {
                m_logger->Log(LogLevel::DEBUG, "SyncClock: Failed to receive clock sync response");
                return;
            }
            if (!NetworkUtilities::IsSameAddress(clientAddr, m_serverAddr))
            {
                m_logger->Log(LogLevel::WARNING, "SyncClock: Received data from unknown address");
                return;
            }
            if (responsePacket->ReadAndValidateCRC())
            {
                m_logger->Log(LogLevel::WARNING, "SyncClock: Packet validation failed");
                return;
            }
            if (responsePacket->ReadNetworkPacketType() != NetworkPacketType::CLOCK_RESPONSE)
            {
                m_logger->Log(LogLevel::WARNING, "SyncClock: Invalid packet type");
                return;
            }

            auto receiveNow = std::chrono::steady_clock::now();
            auto receiveNowEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(receiveNow.time_since_epoch()).count();

            // Round trip time calculation
            auto roundTripTime = receiveNowEpoch - sendNowEpoch;
            m_logger->Log(LogLevel::DEBUG, "SyncClock: Round trip time", { KV(roundTripTime) });

            // Read server time from the response packet
            uint64_t serverTimeMs = responsePacket->ReadUInt64();
            auto serverClockOffset1 = static_cast<int64_t>(serverTimeMs) - sendNowEpoch - roundTripTime / 2;
            auto serverClockOffset2 = static_cast<int64_t>(serverTimeMs) - receiveNowEpoch - roundTripTime / 2;

            m_serverClockOffset = (serverClockOffset1 + serverClockOffset2) / 2;

            m_logger->Log(LogLevel::INFO, "SyncClock: Clock synchronized", { KV(m_serverClockOffset) });
            clockOffsets.push_back(m_serverClockOffset);
            break;
        }
    }

    // Calculate average clock offset
    if (!clockOffsets.empty())
    {
        int64_t totalOffset = std::accumulate(clockOffsets.begin(), clockOffsets.end(), int64_t{ 0 });
        m_serverClockOffset = totalOffset / static_cast<int64_t>(clockOffsets.size());
        m_logger->Log(LogLevel::INFO, "SyncClock: Average clock offset calculated", { KV(m_serverClockOffset) });
    }
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
    const size_t packetsToKeep = MAX_SEND_PACKETS_STORED; // Keep the last 32 packets
    int acknowledgedCount = 0;
    size_t packetsToRemove = m_sendPackets.size() - packetsToKeep;
    int packetLoss = 0;
    std::chrono::steady_clock::duration roundTripTime{};
    m_sendPackets.erase(
        std::remove_if(
            m_sendPackets.begin(),
            m_sendPackets.end(),
            [this, &acknowledgedCount, &roundTripTime, &packetsToRemove, &packetLoss](const PacketInfo& pi) {

                if (packetsToRemove > 0)
                {
                    packetsToRemove--;
                    if (pi.acknowledged)
                    {
                        acknowledgedCount++;
                        roundTripTime += pi.roundTripTime;
                    }
                    else
                    {
                        packetLoss++;
                    }
                    return true; // Remove this packet
                }
                else
                {
                    // Keep last packetsToKeep packets
                    return false;
                }
            }
        ),
        m_sendPackets.end()
    );

    // TODO: Remove old packets from send packets.
    // Keep only the last 32 packets and if there are older unacknowledged packets, remove them
    // but calculate packet loss statistics.

    if (m_sendPackets.size() > 32)
    {
        size_t packetLoss = m_sendPackets.size() - 32;
        m_logger->Log(LogLevel::WARNING, "HandleGameState: Packet loss", { KV(packetLoss) });
        m_sendPackets.erase(m_sendPackets.begin(), m_sendPackets.end() - 32);
    }


    // Average round trip time
    if (acknowledgedCount > 0)
    {
        roundTripTime /= acknowledgedCount;
        auto roundTripTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(roundTripTime).count();
        m_logger->Log(LogLevel::INFO, "HandleGameState: Average round trip time in ms", { KV(roundTripTimeMs), KV(acknowledgedCount)});
        m_roundTripTimeMs = static_cast<uint64_t>(roundTripTimeMs);
    }
    else
    {
        m_logger->Log(LogLevel::DEBUG, "HandleGameState: No packets acknowledged");
    }

    // Keep only 32 received packets
    if (m_receivedPackets.size() > MAX_RECEIVED_PACKETS_STORED)
    {
        m_receivedPackets.erase(
            m_receivedPackets.begin(),
            m_receivedPackets.begin() + (m_receivedPackets.size() - MAX_RECEIVED_PACKETS_STORED)
        );
    }

    auto sendPacketsRemaining = m_sendPackets.size();
    auto receivedPacketsRemaining = m_receivedPackets.size();
    m_logger->Log(LogLevel::DEBUG, "HandleGameState", { KV(m_remoteSequenceNumberLarge), KV(m_remoteSequenceNumberSmall), KV(sendPacketsRemaining), KV(receivedPacketsRemaining) });

    std::vector<PlayerState> playerStates = gamePacket->DeserializePlayerStates();
    ReceiveQueue.push(playerStates);

    return 1;
}

void Client::SendGameState()
{
    std::optional<PlayerState> playerState = SendQueue.pop();
    if (!playerState.has_value())
    {
        m_logger->Log(LogLevel::DEBUG, "SendGameState: No player state to send");
        return;
    }

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
    sendNetworkPacket.SerializePlayerState(playerState.value());

    // Add all previous keyboard states
    size_t sendPacketsSize = m_sendPackets.size();
    size_t limitedSendPacketsSize = sendPacketsSize;
    if (limitedSendPacketsSize > MAX_SEND_PACKETS_STORED)
    {
        m_logger->Log(LogLevel::EXCEPTION, "SendGameState: Too many packets to send", { KV(limitedSendPacketsSize) });
        assert(limitedSendPacketsSize <= MAX_SEND_PACKETS_STORED);
        limitedSendPacketsSize = MAX_SEND_PACKETS_STORED;
    }

    std::vector<uint8_t> keyboardStates;
    for (size_t i = 0; i < limitedSendPacketsSize; ++i)
    {
        const PacketInfo& sendPacket = m_sendPackets[sendPacketsSize - limitedSendPacketsSize + i];
        // Only send until acknowledged packets
        if (sendPacket.acknowledged) break;

        keyboardStates.push_back(NetworkUtilities::PackKeyboard(sendPacket.keyboard));
    }

    sendNetworkPacket.WriteInt8(static_cast<int8_t>(keyboardStates.size()));
    for (const auto& k : keyboardStates)
    {
        sendNetworkPacket.WriteInt8(k);
    }

    m_network->Send(sendNetworkPacket, m_serverAddr);

    PacketInfo pi;
    pi.seqNum = m_localSequenceNumberLarge;
    pi.sendTicks = std::chrono::steady_clock::now();
    pi.keyboard = playerState->keyboard;
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
