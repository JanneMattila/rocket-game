#pragma once
#include "pch.h"
#include "NetworkPacket.h"

class NetworkPacketStub : public NetworkPacket
{
private:
	int m_validateReturnValue;
public:
	NetworkPacketStub(std::vector<uint8_t>& data, int validateReturnValue)
		: NetworkPacket(data), m_validateReturnValue(validateReturnValue) {
	}

	int Validate() override {
		return m_validateReturnValue;
	}
};