#pragma once
#include <string>
#include <iostream>
#include "NetworkPacket.h"
#include "Logger.h"
#include "NetworkConnectionState.h"

class Network
{
private:
	bool m_isServer = false;
	SOCKET m_socket{};
	struct sockaddr_in m_servaddr{};
	uint64_t m_connectionSalt = 0;
	std::shared_ptr<Logger> m_logger;
	NetworkConnectionState m_connectionState = NetworkConnectionState::DISCONNECTED;

public:
	Network(std::shared_ptr<Logger> logger);
	~Network();
	int Initialize(std::string server, int port);
	int EstablishConnection();
	bool IsServerAddress(sockaddr_in& clientAddr);
	int SendToServer(NetworkPacket& networkPacket);
	int SendToPlayer(NetworkPacket& networkPacket, sockaddr_in& clientAddr);
	std::unique_ptr<NetworkPacket> Receive(sockaddr_in& clientAddr, int& result);
};
