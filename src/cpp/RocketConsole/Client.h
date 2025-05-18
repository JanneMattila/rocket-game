#pragma once
#include <cstdint>
#include <unordered_map>
#include <csignal>
#include "Player.h"
#include "Logger.h"
#include "ClientNetwork.h"

class Client
{
private:
	std::shared_ptr<Logger> m_logger;
	std::unique_ptr<ClientNetwork> m_network;

	std::vector<Player> m_players;
	bool m_serverInitializedShutdown = false;

public:
	Client(std::shared_ptr<Logger> logger, std::unique_ptr<ClientNetwork> network);
	~Client();

	int Initialize(std::string server, int port);
	int EstablishConnection();

	int ExecuteGame(volatile std::sig_atomic_t& running);

	int QuitGame();
};
