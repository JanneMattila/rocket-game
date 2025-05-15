#pragma once
#include "pch.h"
#include <string>
#include <iostream>
#include <functional>
#include "ServerNetworkBase.h"
#include "Logger.h"
#include "NetworkPacketStub.h"

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
	std::vector<sockaddr_in> ReceiveAddressReturnValues;

	// Function to override validation for testing failures
	int ValidateReturnValue = 0;

	// Callback function for capturing sent packets
	std::function<int(NetworkPacket&, sockaddr_in&)> SendCaptureCallback;

	ServerNetworkStub() : m_logger(nullptr), ValidateReturnValue(0), SendCaptureCallback(nullptr) {}

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
		// If a capture callback is set, use it first
		if (SendCaptureCallback)
		{
			return SendCaptureCallback(networkPacket, clientAddr);
		}

		// Otherwise, use the standard behavior
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
		// Set up client address if provided
		if (!ReceiveAddressReturnValues.empty())
		{
			clientAddr = ReceiveAddressReturnValues.front();
			ReceiveAddressReturnValues.erase(ReceiveAddressReturnValues.begin());
		}

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

		auto packet = std::make_unique<NetworkPacket>(data);

		// Override validation if needed for testing
		if (ValidateReturnValue != 0)
		{
			// Create a subclass of NetworkPacket that overrides Validate
			return std::make_unique<NetworkPacketStub>(data, ValidateReturnValue);
		}

		return packet;
	}
};
