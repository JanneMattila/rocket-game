#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <thread>
#include <cstring>
#include <string>
#include <csignal>
#include <cstdlib>
#include <algorithm>
#include "NetworkPacket.h"
#include "Logger.h"
#include "ServerNetwork.h"
#include "NetworkPacketType.h"
#include "Player.h"
#include "Server.h"

// Global variables for cleanup
std::shared_ptr<Logger> g_logger;
std::unique_ptr<Server> g_server;

// Signal handler for Ctrl+C
static void SignalHandler(int signal)
{
	if (signal == SIGINT || signal == SIGTERM)
	{
		g_logger->Log(LogLevel::INFO, "Signal detected ({}). Starting to close the game.", signal);
		if (g_server)
		{
			g_server->PrepareToQuitGame();
		}
	}
}

int main()
{
	// Register the signal handler for SIGINT and SIGTERM
	std::signal(SIGINT, SignalHandler);
	std::signal(SIGTERM, SignalHandler);

	g_logger = std::make_shared<Logger>();
	g_logger->SetLogLevel(LogLevel::DEBUG);

	g_logger->Log(LogLevel::INFO, "Rocket server starting");

	int udpPort = 3501;
	const char* envPort = std::getenv("UDP_PORT");
	if (envPort) 
	{
		udpPort = std::atoi(envPort);
	}

	g_logger->Log(LogLevel::INFO, "Listening in UDP Port: {}", udpPort);

	std::unique_ptr<ServerNetwork> network = std::make_unique<ServerNetwork>(g_logger);
	g_server = std::make_unique<Server>(g_logger, std::move(network));

	if (g_server->Initialize(udpPort) != 0)
	{
		g_logger->Log(LogLevel::WARNING, "Failed to initialize network");
		return 1;
	}

	if (g_server->ExecuteGame() != 0)
	{
		g_logger->Log(LogLevel::EXCEPTION, "Server stopped unexpectedly");
		return 1;
	}

	g_server->QuitGame();

#if _DEBUG
	g_logger->Log(LogLevel::DEBUG, "Press any key to exit...");
	std::cin.get();
#endif

	return 0;
}
