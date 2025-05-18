#include "Server.h"
#include "NetworkPacketType.h"
#include "Utils.h"

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


Server::Server(std::shared_ptr<Logger> logger, std::shared_ptr<ServerNetworkBase> network)
	: m_logger(logger), m_network(network) {
}

Server::~Server() {
}

int Server::Initialize(int port)
{
	return m_network->Initialize(port);
}

int Server::ExecuteGame(volatile std::sig_atomic_t& running)
{
	while (running)
	{
		sockaddr_in clientAddr{};
		int result = 0;

		std::unique_ptr<NetworkPacket> networkPacket = m_network->Receive(clientAddr, result);

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

		std::string address = addrToString(clientAddr);
		size_t size = networkPacket->Size();

		m_logger->Log(LogLevel::DEBUG, "Received {} bytes from {}", size, address);

		if (size < CRC32::CRC_SIZE)
		{
			m_logger->Log(LogLevel::WARNING, "Received too small packet: {}", size);
			continue;
		}

		if (networkPacket->Validate())
		{
			m_logger->Log(LogLevel::WARNING, "Packet validation failed");
			continue;
		}

		NetworkPacketType packetType = networkPacket->GetNetworkPacketType();
		auto packetTypeInt = static_cast<int>(packetType);
		m_logger->Log(LogLevel::DEBUG, "Received packet type: {}", packetTypeInt);

		switch (packetType)
		{
		case NetworkPacketType::CONNECTION_REQUEST:
		{
			if (size != 1000)
			{
				m_logger->Log(LogLevel::WARNING, "Received invalid packet size for connection request: {}", size);
				continue;
			}

			if (HandleConnectionRequest(std::move(networkPacket), clientAddr) != 0)
			{
				continue;
			}
		}
		break;
		case NetworkPacketType::CHALLENGE_RESPONSE:
			if (size != 1000)
			{
				m_logger->Log(LogLevel::WARNING, "Received invalid packet size for challenge: {}", size);
				continue;
			}


			if (HandleChallengeResponse(std::move(networkPacket), clientAddr) != 0)
			{
				continue;
			}
			break;
		case NetworkPacketType::GAME_STATE:
			break;
		case NetworkPacketType::DISCONNECT:
		{
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

	for (const Player& player : m_players) {
		if (player.Address == clientAddr) {
			m_logger->Log(LogLevel::DEBUG, "Client {} has already started connection request", player.PlayerID);
			playerID = player.PlayerID;
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
				if (player.PlayerID == i) {
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

	Player player;
	player.ConnectionState = NetworkConnectionState::CONNECTING;
	player.ClientSalt = networkPacket->ReadInt64();
	player.ServerSalt = Utils::GetRandomNumber64();
	player.Salt = player.ClientSalt ^ player.ServerSalt;
	player.PlayerID = playerID;
	player.Address = clientAddr;
	player.Created = std::chrono::steady_clock::now();
	m_players.push_back(player);

	m_logger->Log(LogLevel::DEBUG, "Player {} challenge", player.PlayerID);

	networkPacket->Clear();
	networkPacket->WriteInt8(static_cast<int8_t>(NetworkPacketType::CHALLENGE));
	networkPacket->WriteInt64(player.ClientSalt);
	networkPacket->WriteInt64(player.ServerSalt);

	if (m_network->Send(*networkPacket, clientAddr) != 0)
	{
		m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Failed to send challenge");
		return 1;
	}
	return 0;
}

int Server::HandleChallengeResponse(std::unique_ptr<NetworkPacket> networkPacket, sockaddr_in& clientAddr)
{
	int64_t salt = networkPacket->ReadInt64();

	for (Player& player : m_players)
	{
		if (player.Address == clientAddr)
		{
			networkPacket->Clear();

			if (player.Salt == salt)
			{
				m_logger->Log(LogLevel::DEBUG, "HandleChallengeRequest: Player {} connection accepted", player.PlayerID);
				
				player.ConnectionState = NetworkConnectionState::CONNECTED;

				networkPacket->WriteInt8(static_cast<int8_t>(NetworkPacketType::CONNECTION_ACCEPTED));
				networkPacket->WriteInt64(player.PlayerID);

				if (m_network->Send(*networkPacket, clientAddr) != 0)
				{
					m_logger->Log(LogLevel::WARNING, "HandleChallengeRequest: Failed to send connection accepted");
					return 1;
				}
			}
			else
			{
				m_logger->Log(LogLevel::WARNING, "HandleChallengeRequest: Player {} connection not accepted", player.PlayerID);

				// Remove player from m_players
				auto it = std::find_if(m_players.begin(), m_players.end(),
					[&clientAddr](const Player& p) {
						return p.Address == clientAddr;
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

int Server::QuitGame()
{
	m_logger->Log(LogLevel::DEBUG, "Server is stopping");
	return 0;
}
