#pragma once
#include <string>
#include <iostream>
#include "ServerNetworkBase.h"
#include "Logger.h"

class ServerNetwork : public ServerNetworkBase
{
private:
	SOCKET m_socket{};
	struct sockaddr_in m_servaddr{};
	std::shared_ptr<Logger> m_logger;

public:
	ServerNetwork(std::shared_ptr<Logger> logger);
	~ServerNetwork();
	int Initialize(std::string server, int port) override;
	int Send(NetworkPacket& networkPacket, sockaddr_in& clientAddr) override;
	std::unique_ptr<NetworkPacket> Receive(sockaddr_in& clientAddr, int& result) override;
};
