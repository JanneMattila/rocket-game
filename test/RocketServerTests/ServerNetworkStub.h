#pragma once
#include "pch.h"
#include <string>
#include <iostream>
#include "ServerNetworkBase.h"
#include "Logger.h"

class ServerNetworkStub : public ServerNetworkBase
{
private:
	struct sockaddr_in m_servaddr {};
	std::shared_ptr<Logger> m_logger;

public:
	std::vector<int> InitializeReturnValues;
	std::vector<int> SendReturnValues;
	std::vector<int> ReceiveReturnValues;
	std::vector<std::vector<uint8_t>> ReceiveDataReturnValues;

	int Initialize(std::string server, int port) override
	{
		if (InitializeReturnValues.empty())
		{
			return 0;
		}
		int returnValue = InitializeReturnValues.front();
		InitializeReturnValues.erase(InitializeReturnValues.begin());
		return returnValue;
	}
	int Send(NetworkPacket& networkPacket, sockaddr_in& clientAddr) override
	{
		if (SendReturnValues.empty())
		{
			return 0;
		}
		int returnValue = SendReturnValues.front();
		SendReturnValues.erase(SendReturnValues.begin());
		return returnValue;
	}

	std::unique_ptr<NetworkPacket> Receive(sockaddr_in& clientAddr, int& result) override
	{
		if (ReceiveReturnValues.empty())
		{
			result = 0;
			return nullptr;
		}
		int returnValue = ReceiveReturnValues.front();
		ReceiveReturnValues.erase(ReceiveReturnValues.begin());
		result = returnValue;

		if (ReceiveDataReturnValues.empty())
		{
			return nullptr;
		}
		std::vector<uint8_t> data = ReceiveDataReturnValues.front();
		ReceiveDataReturnValues.erase(ReceiveDataReturnValues.begin());
		return std::make_unique<NetworkPacket>(data);
	}
};
