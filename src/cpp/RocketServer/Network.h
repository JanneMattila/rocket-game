#pragma once
#include <string>
#include <iostream>
#include "NetworkPacket.h"
#include "Logger.h"

class Network
{
private:
	SOCKET m_socket;
	struct sockaddr_in m_servaddr;
	uint64_t m_connectionSalt;
	std::shared_ptr<Logger> m_logger;

public:
	Network(std::shared_ptr<Logger> logger);
	~Network();
	int Initialize(std::string server, int port);
	int EstablishConnection();
	int Send(std::vector<uint8_t>& data);
	int Receive(std::vector<uint8_t>& data, sockaddr_in& clientAddr);
};
