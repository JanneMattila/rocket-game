#pragma once
#include <cstdint>
#include <unordered_map>
#include "Player.h"
#include "Logger.h"
#include "ClientNetwork.h"

class Client
{
private:
	bool m_running = true;

	std::shared_ptr<Logger> m_logger;
	std::unique_ptr<ClientNetwork> m_network;

	std::vector<Player> m_players;

public:
	Client(std::shared_ptr<Logger> logger, std::unique_ptr<ClientNetwork> network);
	~Client();

	int Initialize(std::string server, int port);
	int EstablishConnection();

	int ExecuteGame();

	int PrepareToQuitGame();
	int QuitGame();
};
