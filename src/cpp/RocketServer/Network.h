#pragma once
#include <string>
#include <iostream>
#include <memory>
#include "NetworkBase.h"
#include "Logger.h"

#ifdef _WIN32
#else
typedef int SOCKET;
#endif

class Network : public NetworkBase
{
private:
	SOCKET m_socket{};
	std::shared_ptr<Logger> m_logger;

public:
    Network(std::shared_ptr<Logger> logger);
	~Network();
    int Initialize(std::string server, int port, sockaddr_in& addr) override;
	int Send(NetworkPacket& networkPacket, sockaddr_in& clientAddr) override;
	std::unique_ptr<NetworkPacket> Receive(sockaddr_in& clientAddr, int& result) override;
};
