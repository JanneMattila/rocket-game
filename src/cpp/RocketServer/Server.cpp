#include "Server.h"
#include "NetworkPacketType.h"

std::string addrToString(const sockaddr_in& addr) {
	char buf[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
	return std::string(buf) + ":" + std::to_string(ntohs(addr.sin_port));
}

bool operator==(const sockaddr_in& a, const sockaddr_in& b) {
	return a.sin_family == b.sin_family &&
		a.sin_addr.s_addr == b.sin_addr.s_addr &&
		a.sin_port == b.sin_port;
}


Server::Server(std::shared_ptr<Logger> logger, std::unique_ptr<Network> network)
	: m_logger(logger), m_network(std::move(network)) {
}

Server::~Server() {
}

int Server::Initialize(int port)
{
	return m_network->Initialize("" /* This is server */, port);
}

int Server::ExecuteGame()
{
	while (m_running)
	{
		sockaddr_in clientAddr{};
		std::vector<uint8_t> data;

		int result = m_network->Receive(data, clientAddr);
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

		auto address = addrToString(clientAddr);
		auto size = data.size();

		m_logger->Log(LogLevel::DEBUG, "Received {} bytes from {}", size, address);

		if (size < NetworkPacket::MINIMUM_PACKET_SIZE)
		{
			m_logger->Log(LogLevel::WARNING, "Received too small packet: {}", size);
			continue;
		}

		auto packetType = static_cast<NetworkPacketType>(data[CRC32::CRC_SIZE + 1]);
		auto packetTypeInt = static_cast<int>(packetType);
		m_logger->Log(LogLevel::DEBUG, "Received packet type: {}", packetTypeInt);

		switch (packetType)
		{
		case NetworkPacketType::CONNECTION_REQUEST:
		{
			auto it = m_playersByAddress.find(clientAddr);
			if (it == m_playersByAddress.end()) {
				Player player;
				player.ClientSalt = 0; // TODO: Generate a random salt

				player.PlayerID = static_cast<uint8_t>(m_playersByAddress.size() + 1);
				player.Address = clientAddr;
				player.Created = std::chrono::steady_clock::now();
				m_playersByAddress[clientAddr] = player;
				it = m_playersByAddress.find(clientAddr);
			}
			Player& player = it->second;
			m_logger->Log(LogLevel::INFO, "Player {} connected", player.PlayerID);
		}
		break;
		case NetworkPacketType::CHALLENGE:
			break;
		case NetworkPacketType::GAME_STATE:
			break;
		case NetworkPacketType::DISCONNECT:
		{
			auto it = m_playersByAddress.find(clientAddr);
			if (it != m_playersByAddress.end()) {
				m_logger->Log(LogLevel::INFO, "Player {} disconnected", it->second.PlayerID);
				m_playersByAddress.erase(it);
			}
		}
		break;
		default:
			break;
		}
	}

	/*
	std::vector<uint8_t> buffer(NetworkPacket::ExpectedMessageSize);
	auto lastCleanup = std::chrono::steady_clock::now();

	while (m_running) {
		sockaddr_in clientAddr{};
		socklen_t addrLen = sizeof(clientAddr);
		int received = recvfrom(sock, reinterpret_cast<char*>(buffer.data()), buffer.size(), 0, (sockaddr*)&clientAddr, &addrLen);
		if (received != NetworkPacket::ExpectedMessageSize) {
			continue;
		}
		try {
			NetworkPacket packet = NetworkPacket::FromBytes(buffer);
			auto now = std::chrono::steady_clock::now();
			auto it = players.find(clientAddr);
			if (it == players.end()) {
				RocketPlayer player;
				player.PlayerID = static_cast<uint8_t>(players.size() + 1);
				player.Address = clientAddr;
				player.Created = now;
				player.LastUpdated = now;
				players[clientAddr] = player;
				it = players.find(clientAddr);
			}
			RocketPlayer& player = it->second;
			player.Ticks = packet.Ticks;
			player.PositionX = packet.PositionX;
			player.PositionY = packet.PositionY;
			player.VelocityX = packet.VelocityX;
			player.VelocityY = packet.VelocityY;
			player.Rotation = packet.Rotation;
			player.Speed = packet.Speed;
			player.IsFiring = packet.IsFiring;
			player.LastUpdated = now;
			player.Messages++;
			// Forward to all other players
			for (const auto& kv : players) {
				if (!(kv.first == clientAddr)) {
					sendto(sock, reinterpret_cast<const char*>(buffer.data()), buffer.size(), 0, (sockaddr*)&kv.first, sizeof(sockaddr_in));
				}
			}
			// Cleanup every second
			if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCleanup).count() >= 1000) {
				auto threshold = now - std::chrono::seconds(5);
				std::vector<sockaddr_in> toRemove;
				for (const auto& kv : players) {
					if (kv.second.LastUpdated < threshold) {
						toRemove.push_back(kv.first);
					}
				}
				for (const auto& addr : toRemove) {
					players.erase(addr);
				}
				lastCleanup = now;
			}
		}
		catch (const std::exception& ex) {
			// Ignore invalid packets
			continue;
		}
	}
	*/
	return 0;
}

int Server::PrepareToQuitGame()
{
	m_running = false;
	return 0;
}

int Server::QuitGame()
{
	return 0;
}
