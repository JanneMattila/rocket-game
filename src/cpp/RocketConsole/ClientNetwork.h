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
	struct sockaddr_in m_servaddr {};
	uint64_t m_connectionSalt = 0;
	std::shared_ptr<Logger> m_logger;
	NetworkConnectionState m_connectionState = NetworkConnectionState::DISCONNECTED;

public:
	ClientNetwork(std::shared_ptr<Logger> logger);
	~ClientNetwork();
	int Initialize(std::string server, int port);
	int EstablishConnection();
	inline bool IsServerAddress(sockaddr_in& clientAddr) const
	{
		return (m_servaddr.sin_family == clientAddr.sin_family &&
			m_servaddr.sin_addr.s_addr == clientAddr.sin_addr.s_addr &&
			m_servaddr.sin_port == clientAddr.sin_port);
	}
	int Send(NetworkPacket& networkPacket);
	std::unique_ptr<NetworkPacket> Receive(sockaddr_in& clientAddr, int& result);
};
