#include <iostream>
#include <cstring>
#include <chrono>
#include <thread>
#include "ClientNetwork.h"
#include "Client.h"
#include <csignal>

static std::string GetEnvVariable(const char* varName) {
	std::string result;

#ifdef _WIN32
	char* buffer = nullptr;
	size_t size = 0;
	if (_dupenv_s(&buffer, &size, varName) == 0 && buffer != nullptr) {
		result = buffer;
		free(buffer); // Free the allocated memory
	}
#else
	const char* value = std::getenv(varName);
	if (value != nullptr) {
		result = value;
	}
#endif

	return result;
}

// Global variables for cleanup
std::shared_ptr<Logger> g_logger;
std::unique_ptr<Client> g_client;

volatile std::sig_atomic_t g_running = 1;

// Signal handler for Ctrl+C
static void SignalHandler(int signal)
{
	if (signal == SIGINT || signal == SIGTERM)
	{
		g_running = 0;
	}
}

int main(int argc, char** argv)
{
	// Register the signal handler for SIGINT and SIGTERM
	std::signal(SIGINT, SignalHandler);
	std::signal(SIGTERM, SignalHandler);

	g_logger = std::make_shared<Logger>();
	g_logger->SetLogLevel(LogLevel::DEBUG);

	std::cout << "Rocket Console" << std::endl;

	std::string server = "127.0.0.1";
	int udpPort = 3501;

	// Check for environment variables
	std::string envServer = GetEnvVariable("UDP_SERVER");
	std::string envPort = GetEnvVariable("UDP_PORT");
	if (!envPort.empty())
	{
		udpPort = std::atoi(envPort.data());
	}
	if (!envServer.empty())
	{
		server = envServer;
	}

	// Check for command line arguments
	if (argc > 1)
	{
		server = argv[1];
	}
	if (argc > 2)
	{
		udpPort = std::atoi(argv[2]);
	}

	g_logger->Log(LogLevel::INFO, "UDP Server: {}:{}", server, udpPort);

	std::unique_ptr<ClientNetwork> network = std::make_unique<ClientNetwork>(g_logger);
	g_client = std::make_unique<Client>(g_logger, std::move(network));

	if (g_client->Initialize(server, udpPort) != 0)
	{
		g_logger->Log(LogLevel::WARNING, "Failed to initialize network");
		return 1;
	}

	while (g_client->EstablishConnection() != 0)
	{
		g_logger->Log(LogLevel::WARNING, "Failed to establish connection");
		std::this_thread::sleep_for(std::chrono::seconds(3));
	}

	g_logger->Log(LogLevel::INFO, "Connection established");

	if (g_client->ExecuteGame(g_running) != 0)
	{
		g_logger->Log(LogLevel::EXCEPTION, "Game stopped unexpectedly");
		return 1;
	}

	g_client->QuitGame();

#if _DEBUG
	g_logger->Log(LogLevel::DEBUG, "Press any key to exit...");
	std::cin.get();
#endif

	return 0;
}
