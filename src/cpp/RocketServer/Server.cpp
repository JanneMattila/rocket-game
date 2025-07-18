#include "Server.h"
#include "NetworkPacketType.h"
#include "Utils.h"
#include "NetworkUtilities.h"

Server::Server(std::shared_ptr<Logger> logger, std::shared_ptr<NetworkBase> network)
	: m_logger(logger), m_network(network) {
}

Server::~Server() {
}

int Server::Initialize(int port)
{
    sockaddr_in addr{};
	return m_network->Initialize("" /* server*/, port, addr);
}

int Server::ExecuteGame(volatile std::sig_atomic_t& running)
{
	int idleTime = 0;
	auto idle = std::chrono::steady_clock::now();
	m_logger->Log(LogLevel::INFO, "Server is running");
	while (running)
	{
		sockaddr_in clientAddr{};
		int result = 0;

		std::unique_ptr<NetworkPacket> networkPacket = m_network->Receive(clientAddr, result);

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

		idleTime = 0;
		std::string address = NetworkUtilities::AddressToString(clientAddr);
		size_t size = networkPacket->Size();

		m_logger->Log(LogLevel::DEBUG, "Received bytes from client", { KV(size), KVS(address) });

		if (size < CRC32::CRC_SIZE)
		{
			m_logger->Log(LogLevel::WARNING, "Received too small packet", { KV(size) });
			continue;
		}

		if (networkPacket->ReadAndValidateCRC())
		{
			m_logger->Log(LogLevel::WARNING, "Packet validation failed");
			continue;
		}

		NetworkPacketType packetType = networkPacket->ReadNetworkPacketType();
		auto packetTypeInt = static_cast<int>(packetType);
		m_logger->Log(LogLevel::DEBUG, "Received packet type", { KV(packetTypeInt) });

		switch (packetType)
		{
		case NetworkPacketType::CONNECTION_REQUEST:
			if (size != 1000)
			{
				m_logger->Log(LogLevel::WARNING, "Received invalid packet size for connection request", { KV(size) });
				continue;
			}

			if (HandleConnectionRequest(std::move(networkPacket), clientAddr) != 0)
			{
				continue;
			}
		    break;
		case NetworkPacketType::CHALLENGE_RESPONSE:
			if (size != 1000)
			{
				m_logger->Log(LogLevel::WARNING, "Received invalid packet size for challenge", { KV(size) });
				continue;
			}

			if (HandleChallengeResponse(std::move(networkPacket), clientAddr) != 0)
			{
				continue;
			}
			break;
        case NetworkPacketType::CLOCK:
            if (HandleClockSync(std::move(networkPacket), clientAddr) != 0)
            {
                continue;
            }
            break;
		case NetworkPacketType::GAME_STATE:
            if (HandleGameState(std::move(networkPacket), clientAddr) != 0)
            {
                continue;
            }
			break;
		case NetworkPacketType::DISCONNECT:
            if (HandleDisconnect(std::move(networkPacket), clientAddr) != 0)
            {
                continue;
            }
		    break;
		default:
			break;
		}
	}

	return 0;
}

int Server::HandleConnectionRequest(std::unique_ptr<NetworkPacket> networkPacket, sockaddr_in& clientAddr)
{
	// Check if the client is already connected
	int playerID = 0;

	for (Player& player : m_players) {
		if (NetworkUtilities::IsSameAddress(player.Address, clientAddr)) {
			m_logger->Log(LogLevel::DEBUG, "Client has already started connection request", { KV(player.playerID) });
			playerID = player.playerID;
			break;
		}
	}

	if (playerID == 0)
	{
		// Find a free player ID
		for (int i = 1; i < MAX_PLAYERS; i++)
		{
			bool found = true;
			for (const Player& player : m_players) {
				if (player.playerID == i) {
					found = false;
					playerID = i;
					break;
				}
			}

			if (found) {
				break;
			}
		}
	}

	if (playerID == 0)
	{
		// This is the first player
		playerID = 1;
	}

    uint64_t clientSalt = networkPacket->ReadUInt64();
    uint64_t serverSalt = Utils::GetRandomNumberUInt64();
    m_logger->Log(LogLevel::DEBUG, "HandleConnectionRequest: Received connection request with client salt", { KV(clientSalt) });

    // TODO: Add clock synchronization

	Player player;
	player.ConnectionState = NetworkConnectionState::CONNECTING;
	player.ClientSalt = clientSalt;
	player.ServerSalt = serverSalt;
	player.ConnectionSalt = player.ClientSalt ^ player.ServerSalt;
	player.playerID = playerID;
	player.Address = clientAddr;
	player.Created = std::chrono::steady_clock::now();
    player.pos.x.floatValue = 200.0f;
    player.pos.y.floatValue = 200.0f;



	m_players.push_back(player);

	m_logger->Log(LogLevel::DEBUG, "HandleConnectionRequest: Player challenge", { KV(playerID) });

	networkPacket->Clear();
	networkPacket->WriteInt8(static_cast<int8_t>(NetworkPacketType::CHALLENGE));
	networkPacket->WriteUInt64(clientSalt);
	networkPacket->WriteUInt64(serverSalt);

    m_logger->Log(LogLevel::INFO, "HandleConnectionRequest: Sending challenge", { KV(clientSalt), KV(serverSalt) });

	if (m_network->Send(*networkPacket, clientAddr) != 0)
	{
		m_logger->Log(LogLevel::DEBUG, "HandleConnectionRequest: Failed to send challenge");
		return 1;
	}
	return 0;
}

int Server::HandleChallengeResponse(std::unique_ptr<NetworkPacket> networkPacket, sockaddr_in& clientAddr)
{
	int64_t salt = networkPacket->ReadUInt64();

	for (Player& player : m_players)
	{
		if (NetworkUtilities::IsSameAddress(player.Address, clientAddr))
		{
			networkPacket->Clear();

			if (player.ConnectionSalt == salt)
			{
				m_logger->Log(LogLevel::DEBUG, "HandleChallengeRequest: Player connection accepted", { KV(player.playerID) });

				player.ConnectionState = NetworkConnectionState::CONNECTED;

                // TODO: Add clock synchronization

				networkPacket->WriteInt8(static_cast<int8_t>(NetworkPacketType::CONNECTION_ACCEPTED));
				networkPacket->WriteInt64(player.playerID);

				if (m_network->Send(*networkPacket, clientAddr) != 0)
				{
					m_logger->Log(LogLevel::WARNING, "HandleChallengeRequest: Failed to send connection accepted");
					return 1;
				}
			}
			else
			{
				m_logger->Log(LogLevel::WARNING, "HandleChallengeRequest: Player connection not accepted", { KV(player.playerID) });

				// Remove player from m_players
				auto it = std::find_if(m_players.begin(), m_players.end(),
					[&clientAddr](const Player& p) {
						return NetworkUtilities::IsSameAddress(p.Address, clientAddr);
					});

				if (it != m_players.end()) {
					m_players.erase(it);
				}

				networkPacket->WriteInt8(static_cast<int8_t>(NetworkPacketType::CONNECTION_DENIED));

				if (m_network->Send(*networkPacket, clientAddr) != 0)
				{
					m_logger->Log(LogLevel::WARNING, "HandleChallengeRequest: Failed to send connection not accepted");
					return 1;
				}
			}
			return 0;
		}
	}

	m_logger->Log(LogLevel::WARNING, "HandleChallengeRequest: Player not found");
	return 1;
}

int Server::HandleClockSync(std::unique_ptr<NetworkPacket> networkPacket, sockaddr_in& clientAddr)
{
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

    int64_t connectionSalt = networkPacket->ReadUInt64();
    int64_t clientTime = networkPacket->ReadUInt64();
    for (Player& player : m_players)
    {
        if (player.ConnectionSalt == connectionSalt &&
            NetworkUtilities::IsSameAddress(player.Address, clientAddr))
        {
            player.serverClockOffset = now - clientTime;
            m_logger->Log(LogLevel::INFO, "HandleClockSync: Clock synchronized", { KV(player.serverClockOffset) });

            // Send clock sync response
            NetworkPacket responsePacket;
            responsePacket.WriteInt8(static_cast<int8_t>(NetworkPacketType::CLOCK_RESPONSE));
            responsePacket.WriteInt64(now);
            if (m_network->Send(responsePacket, player.Address) != 0)
            {
                m_logger->Log(LogLevel::WARNING, "HandleClockSync: Failed to send clock sync response");
                return 1;
            }
            return 0;
        }
    }
    m_logger->Log(LogLevel::WARNING, "HandleClockSync: Player not found");
    return 1;
}

int Server::HandleGameState(std::unique_ptr<NetworkPacket> networkPacket, sockaddr_in& clientAddr)
{
    int64_t connectionSalt = networkPacket->ReadUInt64();

    for (Player& player : m_players)
    {
        if (player.ConnectionSalt == connectionSalt &&
            NetworkUtilities::IsSameAddress(player.Address, clientAddr))
        {
            GamePacket* gamePacket = static_cast<GamePacket*>(networkPacket.get());

            uint16_t seqNum = gamePacket->ReadInt16();
            uint16_t ack = gamePacket->ReadInt16();
            uint32_t ackBits = gamePacket->ReadInt32();

            uint16_t diff = NetworkUtilities::SequenceNumberDiff(player.remoteSequenceNumberSmall, seqNum);
            if (diff > 0)
            {
                player.remoteSequenceNumberLarge += diff;
                player.remoteSequenceNumberSmall = seqNum;

                // TODO: This or NetworkUtilities::StoreAcks(...)
                player.receivedPackets.push_back(player.remoteSequenceNumberLarge);
            }
            else if (diff < 0)
            {
                // TODO: Add stats about out-of-order received packets
                m_logger->Log(LogLevel::WARNING, "HandleGameState out-of-order packets");
            }
            else if (diff == 0)
            {
                // TODO: Add stats about duplicate received packets or replayed packet
                m_logger->Log(LogLevel::WARNING, "HandleGameState duplicate packets");
            }

            diff = NetworkUtilities::SequenceNumberDiff(player.localSequenceNumberSmall, ack);
            auto localSequenceNumberLarge = player.localSequenceNumberLarge - diff;

            NetworkUtilities::VerifyAck(player.sendPackets, localSequenceNumberLarge, ackBits);

            // Clear all acknowledged packets away from send packets
            player.sendPackets.erase(
                std::remove_if(
                    player.sendPackets.begin(),
                    player.sendPackets.end(),
                    [this](const PacketInfo& pi) {
                        // Remove if acknowledged
                        return pi.acknowledged;
                    }
                ),
                player.sendPackets.end()
            );

            // Keep only 60 received packets
            if (player.receivedPackets.size() > 60)
            {
                player.receivedPackets.erase(
                    player.receivedPackets.begin(),
                    player.receivedPackets.begin() + (player.receivedPackets.size() - 60)
                );
            }

            auto sendPacketsRemaining = player.sendPackets.size();
            auto receivedPacketsRemaining = player.receivedPackets.size();
            m_logger->Log(LogLevel::DEBUG, "HandleGameState", { KV(player.remoteSequenceNumberLarge), KV(player.remoteSequenceNumberSmall), KV(sendPacketsRemaining), KV(receivedPacketsRemaining)  });

            ackBits = 0;
            NetworkUtilities::ComputeAckBits(player.receivedPackets, player.remoteSequenceNumberLarge, ackBits);

            player.localSequenceNumberLarge++;
            player.localSequenceNumberSmall = player.localSequenceNumberLarge % SEQUENCE_NUMBER_MAX;

            PlayerState playerState = gamePacket->DeserializePlayerState();
            player.keyboard = playerState.keyboard;

            GamePacket sendNetworkPacket;
            sendNetworkPacket.WriteInt8(static_cast<int8_t>(NetworkPacketType::GAME_STATE));
            sendNetworkPacket.WriteInt64(player.ConnectionSalt);
            sendNetworkPacket.WriteInt16(player.localSequenceNumberSmall);
            sendNetworkPacket.WriteInt16(player.remoteSequenceNumberSmall);
            sendNetworkPacket.WriteInt32(ackBits);

            // Serialize player states
            sendNetworkPacket.WriteInt8(static_cast<int8_t>(m_players.size()));
            for (const Player& p : m_players)
            {
                sendNetworkPacket.SerializePlayerState(p);
            }

            m_network->Send(sendNetworkPacket, player.Address);

            PacketInfo pi;
            pi.seqNum = player.localSequenceNumberLarge;
            pi.sendTicks = std::chrono::steady_clock::now();
            player.sendPackets.push_back(pi);

            m_logger->Log(LogLevel::DEBUG, "SendGameState", { KV(player.localSequenceNumberLarge), KV(player.localSequenceNumberSmall) });

            return 0;
        }
    }

    m_logger->Log(LogLevel::WARNING, "HandleGameState: Player not found");
    return 1;
}

int Server::HandleDisconnect(std::unique_ptr<NetworkPacket> networkPacket, sockaddr_in& clientAddr)
{
    int64_t connectionSalt = networkPacket->ReadUInt64();

    auto it = std::remove_if(m_players.begin(), m_players.end(),
        [connectionSalt, &clientAddr](const Player& p) {
            return
                p.ConnectionSalt == connectionSalt &&
                NetworkUtilities::IsSameAddress(p.Address, clientAddr);
        });

    if (it != m_players.end())
    {
        auto playerID = it->playerID;
        m_logger->Log(LogLevel::INFO, "HandleDisconnect: Player disconnected", { KV(playerID) });

        m_players.erase(it, m_players.end());

        // TODO: Notify other players
    }

    return 1;
}

int Server::QuitGame()
{
	m_logger->Log(LogLevel::INFO, "Server is stopping. Notifying clients.");
    for (Player& player : m_players)
    {
        // Send disconnect packets to server
        for (size_t i = 0; i < 10; i++)
        {
            NetworkPacket sendNetworkPacket;
            sendNetworkPacket.WriteInt8(static_cast<int8_t>(NetworkPacketType::DISCONNECT));
            sendNetworkPacket.WriteInt64(player.ConnectionSalt);
            m_network->Send(sendNetworkPacket, player.Address);
        }
    }

	return 0;
}