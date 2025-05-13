#pragma once
#include <string>
#include <iostream>
#include "NetworkPacket.h"
#include "Logger.h"
#include "NetworkConnectionState.h"

class ClientNetwork
{
private:
	SOCKET m_socket{};
	struct sockaddr_in m_servaddr {};
	uint64_t m_connectionSalt = 0;
	std::shared_ptr<Logger> m_logger;
	NetworkConnectionState m_connectionState = NetworkConnectionState::DISCONNECTED;

public:
	ClientNetwork(std::shared_ptr<Logger> logger);
	~ClientNetwork();
	int Initialize(std::string server, int port);
	int EstablishConnection();
	bool IsServerAddress(sockaddr_in& clientAddr);
	int Send(NetworkPacket& networkPacket);
	std::unique_ptr<NetworkPacket> Receive(sockaddr_in& clientAddr, int& result);
};
