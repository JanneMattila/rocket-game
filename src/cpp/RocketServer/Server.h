#pragma once
#include <cstdint>
#include <unordered_map>
#include "Player.h"
#include "Logger.h"
#include "Network.h"

namespace std {
	template<>
	struct hash<sockaddr_in> {
		size_t operator()(const sockaddr_in& addr) const {
			return hash<uint32_t>()(addr.sin_addr.s_addr) ^ hash<uint16_t>()(addr.sin_port);
		}
	};
}

class Server
{
private:
	static constexpr int8_t MAX_PLAYERS = 8;

	bool m_running = true;

	std::shared_ptr<Logger> m_logger;
	std::unique_ptr<Network> m_network;

	std::vector<Player> m_players;

public:
	Server(std::shared_ptr<Logger> logger, std::unique_ptr<Network> network);
	~Server();

	int Initialize(int port);

	int ExecuteGame();

	int PrepareToQuitGame();
	int QuitGame();

	int HandleConnectionRequest(std::unique_ptr<NetworkPacket> networkPacket, sockaddr_in& clientAddr);
	int HandleChallengeResponse(std::unique_ptr<NetworkPacket> networkPacket, sockaddr_in& clientAddr);
};

