#pragma once
#include <string>
#include <iostream>
#include "NetworkPacket.h"
#include "Logger.h"
#include "NetworkConnectionState.h"

class ServerNetwork
{
private:
	SOCKET m_socket{};
	struct sockaddr_in m_servaddr{};
	std::shared_ptr<Logger> m_logger;

public:
	ServerNetwork(std::shared_ptr<Logger> logger);
	~ServerNetwork();
	int Initialize(std::string server, int port);
	bool IsServerAddress(sockaddr_in& clientAddr);
	int Send(NetworkPacket& networkPacket, sockaddr_in& clientAddr);
	std::unique_ptr<NetworkPacket> Receive(sockaddr_in& clientAddr, int& result);
};
