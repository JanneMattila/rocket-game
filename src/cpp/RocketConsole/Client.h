#pragma once
#include <csignal>
#include <deque>
#include "Player.h"
#include "Logger.h"
#include "Network.h"
#include "PacketInfo.h"
#include "NetworkQueue.h"
#include "PhysicsEngine.h"
#include "GameStateSnapshot.h"

class Client
{
private:
	std::shared_ptr<Logger> m_logger;
	std::unique_ptr<Network> m_network;

    uint64_t m_clientSalt = 0;
    uint64_t m_serverSalt = 0;
    uint64_t m_connectionSalt = 0;
    uint8_t m_playerID = 0;
    NetworkConnectionState m_connectionState = NetworkConnectionState::DISCONNECTED;
    struct sockaddr_in m_serverAddr {};

    std::vector<PacketInfo> m_sendPackets;
    std::vector<uint64_t> m_receivedPackets;

    std::deque<GameStateSnapshot> m_gameStateSnapshot;

	std::vector<Player> m_players;
	bool m_serverInitializedShutdown = false;
    uint64_t m_localSequenceNumberLarge = 0;
    uint16_t m_localSequenceNumberSmall = 0;

    uint64_t m_remoteSequenceNumberLarge = 0;
    uint16_t m_remoteSequenceNumberSmall = 0;

    uint64_t m_roundTripTimeMs = 0;

    int64_t m_serverClockOffset = 0; // Offset to synchronize the server clock

    const int MAX_SEND_PACKETS_STORED = 33; // Maximum number of packets to store for sending
    const int MAX_RECEIVED_PACKETS_STORED = 33; // Maximum number of packets to store for received packets

public:
	Client(std::shared_ptr<Logger> logger, std::unique_ptr<Network> network);
	~Client();

    NetworkQueue<PlayerState, 256> OutgoingState;
    NetworkQueue<std::vector<PlayerState>, 256> IncomingStates;

	int Initialize(std::string server, int port);
	int EstablishConnection();
    int SyncClock();

	int ExecuteGame(volatile std::sig_atomic_t& running);

    void SendGameState();
    int HandleGameState(std::unique_ptr<NetworkPacket> networkPacket);
    NetworkConnectionState GetConnectionState() const { return m_connectionState; }
    uint64_t GetRoundTripTimeMs() const { return m_roundTripTimeMs; }

    void ClientSidePrediction(const PlayerState& playerState, const uint64_t seqNum);
    void ApplyAuthoritativeState(const GameStateSnapshot& serverState, const uint64_t seqNum);
    void RollbackAndReplay(const GameStateSnapshot& serverState);

	int QuitGame();
};
