#pragma once
#include <cstdint>
#include <unordered_map>
#include <csignal>
#include "Player.h"
#include "Logger.h"
#include "NetworkBase.h"

class Server
{
private:
	static constexpr int8_t MAX_PLAYERS = 8;

	std::shared_ptr<Logger> m_logger;
	std::shared_ptr<NetworkBase> m_network;

	std::vector<Player> m_players;

public:
	Server(std::shared_ptr<Logger> logger, std::shared_ptr<NetworkBase> network);
	~Server();

	int Initialize(int port);

	int ExecuteGame(volatile std::sig_atomic_t& running);

	int QuitGame();

	int HandleConnectionRequest(std::unique_ptr<NetworkPacket> networkPacket, sockaddr_in& clientAddr);
	int HandleChallengeResponse(std::unique_ptr<NetworkPacket> networkPacket, sockaddr_in& clientAddr);
    int HandleClockSync(std::unique_ptr<NetworkPacket> networkPacket, sockaddr_in& clientAddr);
    int HandleGameState(std::unique_ptr<NetworkPacket> networkPacket, sockaddr_in& clientAddr);
    int HandleDisconnect(std::unique_ptr<NetworkPacket> networkPacket, sockaddr_in& clientAddr);
};

