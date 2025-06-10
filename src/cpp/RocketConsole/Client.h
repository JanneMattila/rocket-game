#pragma once
#include <cstdint>
#include <unordered_map>
#include <csignal>
#include "Player.h"
#include "Logger.h"
#include "Network.h"

class Client
{
private:
	std::shared_ptr<Logger> m_logger;
	std::unique_ptr<Network> m_network;

    uint64_t m_clientSalt = 0;
    uint64_t m_serverSalt = 0;
    uint64_t m_connectionSalt = 0;
    NetworkConnectionState m_connectionState = NetworkConnectionState::DISCONNECTED;
    struct sockaddr_in m_serverAddr {};

    std::vector<PacketInfo> m_sendPackets;
    std::vector<uint64_t> m_receivedPackets;

	std::vector<Player> m_players;
	bool m_serverInitializedShutdown = false;
    uint64_t m_localSequenceNumberLarge = 0;
    uint16_t m_localSequenceNumberSmall = 0;

    uint64_t m_remoteSequenceNumberLarge = 0;
    uint16_t m_remoteSequenceNumberSmall = 0;

public:
	Client(std::shared_ptr<Logger> logger, std::unique_ptr<Network> network);
	~Client();

	int Initialize(std::string server, int port);
	int EstablishConnection();

	int ExecuteGame(volatile std::sig_atomic_t& running);

	void SendGameState();
    int HandleGameState(std::unique_ptr<NetworkPacket> networkPacket);

	int QuitGame();
};
