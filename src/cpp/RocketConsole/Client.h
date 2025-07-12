#pragma once
#include <csignal>
#include "Player.h"
#include "Logger.h"
#include "Network.h"
#include "PacketInfo.h"
#include "NetworkQueue.h"

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

    uint64_t m_roundTripTimeMs = 0;

    int64_t m_serverClockOffset = 0; // Offset to synchronize the server clock

    const int MAX_SEND_PACKETS_STORED = 32; // Maximum number of packets to store for sending
    const int MAX_RECEIVED_PACKETS_STORED = 32; // Maximum number of packets to store for received packets

public:
	Client(std::shared_ptr<Logger> logger, std::unique_ptr<Network> network);
	~Client();

    NetworkQueue<PlayerState, 1024> SendQueue;
    NetworkQueue<std::vector<PlayerState>, 1024> ReceiveQueue;

	int Initialize(std::string server, int port);
	int EstablishConnection();
    void SyncClock();

	int ExecuteGame(volatile std::sig_atomic_t& running);

	void SendGameState();
    int HandleGameState(std::unique_ptr<NetworkPacket> networkPacket);
    NetworkConnectionState GetConnectionState() const { return m_connectionState; }
    uint64_t GetRoundTripTimeMs() const { return m_roundTripTimeMs; }

	int QuitGame();
};
