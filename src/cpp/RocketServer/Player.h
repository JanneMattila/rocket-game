#pragma once
#include <cstdint>
#include <chrono>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include "NetworkConnectionState.h"
#include "GamePacket.h"
#include "PacketInfo.h"

struct Player
{
	uint64_t ClientSalt = 0;
	uint64_t ServerSalt = 0;
	uint64_t ConnectionSalt = 0;
	uint8_t PlayerID = 0;
    // TODO: Add signing key to encrypt payload but not headers
	sockaddr_in Address{};
	NetworkConnectionState ConnectionState = NetworkConnectionState::DISCONNECTED;
	std::chrono::steady_clock::time_point Created;
	std::chrono::steady_clock::time_point LastUpdated;
	int Messages = 0;
	int64_t Ticks = 0;
	float PositionX = 0;
	float PositionY = 0;
	float VelocityX = 0;
	float VelocityY = 0;
	float Rotation = 0;
	float Speed = 0;
	uint8_t IsFiring = 0;

    std::vector<PacketInfo> sendPackets;
    std::vector<uint64_t> receivedPackets;

    uint64_t localSequenceNumberLarge = 0;
    uint16_t localSequenceNumberSmall = 0;

    uint64_t remoteSequenceNumberLarge = 0;
    uint16_t remoteSequenceNumberSmall = 0;
};