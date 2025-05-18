#include "Client.h"
#include "NetworkPacketType.h"
#include "Utils.h"

static std::string addrToString(const sockaddr_in& addr) {
	char buf[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
	return std::string(buf) + ":" + std::to_string(ntohs(addr.sin_port));
}

static bool operator==(const sockaddr_in& a, const sockaddr_in& b) {
	return a.sin_family == b.sin_family &&
		a.sin_addr.s_addr == b.sin_addr.s_addr &&
		a.sin_port == b.sin_port;
}


Client::Client(std::shared_ptr<Logger> logger, std::unique_ptr<ClientNetwork> network)
	: m_logger(logger), m_network(std::move(network)) {
}

Client::~Client() {
}

int Client::Initialize(std::string server, int port)
{
	return m_network->Initialize(server, port);
}

int Client::EstablishConnection()
{
	return m_network->EstablishConnection();
}

int Client::ExecuteGame(volatile std::sig_atomic_t& running)
{
	// Main loop
	int8_t index = 0;
	while (running)
	{
		sockaddr_in serverAddr{};
		int result = 0;

		NetworkPacket sendNetworkPacket;
		sendNetworkPacket.WriteInt8(static_cast<int8_t>(NetworkPacketType::GAME_STATE));
		sendNetworkPacket.WriteInt8(1);
		sendNetworkPacket.WriteInt8(index++);
		m_network->Send(sendNetworkPacket);

		std::unique_ptr<NetworkPacket> networkPacket = m_network->Receive(serverAddr, result);
		if (result == -1)
		{
			m_logger->Log(LogLevel::DEBUG, "Waiting for data");
			continue;
		}
		if (result != 0)
		{
			m_logger->Log(LogLevel::DEBUG, "Failed to receive data");
			continue;
		}

		if (!m_network->IsServerAddress(serverAddr))
		{
			m_logger->Log(LogLevel::DEBUG, "Received data from unknown address");
			continue;
		}

		size_t size = networkPacket->Size();
		if (size < CRC32::CRC_SIZE)
		{
			m_logger->Log(LogLevel::WARNING, "Received too small packet: {}", size);
			continue;
		}

		if (networkPacket->Validate())
		{
			m_logger->Log(LogLevel::WARNING, "Invalid packet");
			continue;
		}

		switch (networkPacket->GetNetworkPacketType())
		{
			case NetworkPacketType::GAME_STATE:
				m_logger->Log(LogLevel::DEBUG, "Game state packet received");
				break;
			case NetworkPacketType::DISCONNECT:
				m_logger->Log(LogLevel::DEBUG, "Disconnect packet received");
				running = false;
				break;
			default:
				m_logger->Log(LogLevel::WARNING, "Unknown packet type");
				break;
		}
	}

	return 0;
}

int Client::QuitGame()
{
	m_logger->Log(LogLevel::DEBUG, "Game is stopping");

	if (!m_serverInitializedShutdown)
	{
		// Send disconnect packet to server
		NetworkPacket sendNetworkPacket;
		sendNetworkPacket.WriteInt8(static_cast<int8_t>(NetworkPacketType::DISCONNECT));
		sendNetworkPacket.WriteInt8(0);
		m_network->Send(sendNetworkPacket);
	}
	return 0;
}
