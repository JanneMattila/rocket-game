#include <iostream>
#include <cstring>
#include <chrono>
#include <thread>
#include "Network.h"

std::string GetEnvVariable(const char* varName) {
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

int main(int argc, char** argv)
{
	std::shared_ptr<Logger> logger = std::make_shared<Logger>();
	logger->SetLogLevel(LogLevel::DEBUG);

	std::cout << "Rocket Console" << std::endl;

	std::string server = "127.0.0.1";
	int udpPort = 3501;

	// Check for environment variables
	std::string envServer = GetEnvVariable("UDP_SERVER");
	std::string envPort = GetEnvVariable("UDP_PORT");
	if (!envPort.empty()) {
		udpPort = std::atoi(envPort.data());
	}
	if (!envServer.empty()) {
		server = envServer;
	}

	// Check for command line arguments
	if (argc > 1) {
		server = argv[1];
	}
	if (argc > 2) {
		udpPort = std::atoi(argv[2]);
	}

	logger->Log(LogLevel::INFO, "UDP Server: {}:{}", server, udpPort);

	std::unique_ptr<Network> network = std::make_unique<Network>(logger);

	if (network->Initialize(server, udpPort) != 0) {
		logger->Log(LogLevel::WARNING, "Failed to initialize network");
		return 1;
	}

	while (network->EstablishConnection() != 0) {
		logger->Log(LogLevel::WARNING, "Failed to establish connection");
		std::this_thread::sleep_for(std::chrono::seconds(3));
	}

	std::cout << "Rocket Console finished." << std::endl;
	return 0;
}
