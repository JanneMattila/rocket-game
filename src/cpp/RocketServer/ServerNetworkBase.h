#pragma once
#include <string>
#include <iostream>
#include "NetworkPacket.h"
#include "NetworkConnectionState.h"

class ServerNetworkBase
{
private:
public:
	virtual int Initialize(std::string server, int port) = 0;
	virtual int Send(NetworkPacket& networkPacket, sockaddr_in& clientAddr) = 0;
	virtual std::unique_ptr<NetworkPacket> Receive(sockaddr_in& clientAddr, int& result) = 0;
};
