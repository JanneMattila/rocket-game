#pragma once
#include <string>
#include <iostream>
#include <memory>
#include "NetworkPacket.h"
#include "Logger.h"
#include "NetworkConnectionState.h"

#ifdef _WIN32
#else
typedef int SOCKET;
#endif

class ClientNetwork
{
private:
	SOCKET m_socket{};
	struct sockaddr_in m_serverAddr {};
	std::shared_ptr<Logger> m_logger;

public:
	ClientNetwork(std::shared_ptr<Logger> logger);
	~ClientNetwork();
	int Initialize(std::string server, int port);

    inline bool IsServerAddress(sockaddr_in& clientAddr) const
    {
        return (m_serverAddr.sin_family == clientAddr.sin_family &&
            m_serverAddr.sin_addr.s_addr == clientAddr.sin_addr.s_addr &&
            m_serverAddr.sin_port == clientAddr.sin_port);
    }

	int Send(NetworkPacket& networkPacket);
	std::unique_ptr<NetworkPacket> Receive(sockaddr_in& clientAddr, int& result);
};
