#include <random>
#include <cstdint>
#include <algorithm> 
#include "ClientNetwork.h"
#include "NetworkPacketType.h"
#include "Utils.h"

#ifdef _WIN32
#define SOCKET_TIMEOUT WSAEWOULDBLOCK
#else
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <cerrno>
// https://en.wikipedia.org/wiki/Errno.h
#define SOCKET_ERROR -1
#define SOCKET_TIMEOUT EAGAIN // 11
#define INVALID_SOCKET  (SOCKET)(~0)
#endif

// Platform specific methods
#ifdef _WIN32
static inline int GetNetworkLastError()
{
	return WSAGetLastError();
}
static std::string GetNetworkErrorMessage(int errorCode)
{
	char* msgBuf = nullptr;
	DWORD size = FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		errorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&msgBuf,
		0, nullptr);

	std::string message;
	if (size && msgBuf)
	{
		message.assign(msgBuf, size);
		LocalFree(msgBuf);
	}
	else
	{
		message = "Unknown error";
	}
	std::ranges::replace(message, '\r', ' ');
	std::ranges::replace(message, '\n', ' ');
	std::ranges::replace(message, '"', '\'');

	return message;
}
#else
static inline int GetNetworkLastError()
{
	return errno;
}
static std::string GetNetworkErrorMessage(int errorCode)
{
	auto message = std::strerror(errorCode);

	std::ranges::replace(message, '\r', ' ');
	std::ranges::replace(message, '\n', ' ');
	std::ranges::replace(message, '"', '\'');

	return message;
}
#endif

ClientNetwork::ClientNetwork(std::shared_ptr<Logger> logger) : m_logger(logger), m_socket(0), m_connectionSalt(0), m_servaddr{}
{
}

ClientNetwork::~ClientNetwork()
{
	// Close the socket and cleanup
#ifdef _WIN32
	closesocket(m_socket);
	WSACleanup();
#else
	close(m_socket);
#endif
}

int ClientNetwork::Initialize(std::string server, int port)
{
#ifdef _WIN32
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		m_logger->Log(LogLevel::EXCEPTION, "WSAStartup failed");
		return 1;
	}
#endif

	m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_socket == INVALID_SOCKET) {
		auto errorCode = GetNetworkLastError();
		m_logger->Log(LogLevel::EXCEPTION, "Initialize: Socket creation failed", { KV(errorCode) });
		return 1;
	}

	// Set socket to non-blocking mode
#ifdef _WIN32
	u_long nonBlocking = 1;
	if (ioctlsocket(m_socket, FIONBIO, &nonBlocking) != 0) {
		auto errorCode = GetNetworkLastError();
		std::string errorMsg = GetNetworkErrorMessage(errorCode);
		m_logger->Log(LogLevel::EXCEPTION, "Initialize: Failed to set socket to non-blocking", { KV(errorCode), KVS(errorMsg) });
		return 1;
	}
#else
	int nonBlocking = 1;
	if (fcntl(m_socket, F_SETFL, O_NONBLOCK, &nonBlocking) == -1)
	{
		auto errorCode = GetNetworkLastError();
		std::string errorMsg = GetNetworkErrorMessage(errorCode);
		m_logger->Log2(LogLevel::EXCEPTION, "Initialize: Failed to set socket to non-blocking", { KV(errorCode), KVS(errorMsg) });
		return 1;
	}
#endif

	m_servaddr.sin_family = AF_INET;
	m_servaddr.sin_port = htons(port);

	if (inet_pton(AF_INET, server.c_str(), &m_servaddr.sin_addr) != 1)
	{
		m_logger->Log(LogLevel::EXCEPTION, "Initialize: Failed to convert address", { KVS(server) });
		return 1;
	}

	return 0;
}

int ClientNetwork::EstablishConnection()
{
    m_clientSalt = 0;
    m_serverSalt = 0;
    m_connectionSalt = 0;

	uint64_t clientSalt = Utils::GetRandomNumberUInt64();
	std::unique_ptr<NetworkPacket> networkPacket = std::make_unique<NetworkPacket>();

	m_connectionState = NetworkConnectionState::CONNECTING;
	networkPacket->WriteInt8(static_cast<int8_t>(NetworkPacketType::CONNECTION_REQUEST));
	networkPacket->WriteUInt64(clientSalt);

	// Pad the packet to 1000 bytes
	for (size_t i = 0; i < 1000 
        - sizeof(uint32_t) /* crc32 */
        - sizeof(uint8_t) /* packet type */
        - sizeof(uint64_t) /* client salt */; i++)
	{
		networkPacket->WriteInt8(0);
	}

	int result = 0;

    m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Send connection request with client salt", { KV(clientSalt) });

	if (Send(*networkPacket) != 0)
	{
		m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Failed to send connection request");
		return 1;
	}

	m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Waiting for challenge");

    // Sleep for a short period to allow the server to respond
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	sockaddr_in clientAddr{};
    std::unique_ptr<NetworkPacket> challengePacket = Receive(clientAddr, result);

	if (result != 0)
	{
		m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Timeout waiting for response to connection request");
		return 1;
	}

	if (challengePacket->ReadAndValidateCRC())
	{
		m_connectionState = NetworkConnectionState::DISCONNECTED;
		m_logger->Log(LogLevel::WARNING, "EstablishConnection: Packet validation failed");
		return 1;
	}

	if (challengePacket->ReadNetworkPacketType() != NetworkPacketType::CHALLENGE)
	{
		m_connectionState = NetworkConnectionState::DISCONNECTED;
		m_logger->Log(LogLevel::WARNING, "EstablishConnection: Invalid packet type");
		return 1;
	}

	uint64_t receivedClientSalt = challengePacket->ReadUInt64();
    uint64_t serverSalt = challengePacket->ReadUInt64();
    m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Received challenge with client and server salt", { KV(receivedClientSalt), KV(serverSalt) });
    
    if (receivedClientSalt != clientSalt)
	{
		m_connectionState = NetworkConnectionState::DISCONNECTED;
		m_logger->Log(LogLevel::WARNING, "EstablishConnection: Client salt mismatch", { KV(clientSalt), KV(receivedClientSalt) });
		return 1;
	}

    uint64_t connectionSalt = clientSalt ^ serverSalt;
	networkPacket->Clear();

	// Create a new packet with the connection salt
	networkPacket->WriteInt8(static_cast<int8_t>(NetworkPacketType::CHALLENGE_RESPONSE));
	networkPacket->WriteUInt64(connectionSalt);
	// Pad the packet to 1000 bytes
	for (size_t i = 0; i < 1000
        - sizeof(uint32_t) /* crc32 */
        - sizeof(uint8_t) /* packet type */
        - sizeof(uint64_t) /* connection salt */; i++)
	{
		networkPacket->WriteInt8(0);
	}

    m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Sending connection salt", { KV(connectionSalt) });

	if (Send(*networkPacket) != 0)
	{
		m_connectionState = NetworkConnectionState::DISCONNECTED;
		m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Failed to send challenge request");
		return 1;
	}

    m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Waiting for challenge response");

    // Sleep for a short period to allow the server to respond
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::unique_ptr<NetworkPacket> challengeResponsePacket = Receive(clientAddr, result);
	if (result != 0)
	{
		m_connectionState = NetworkConnectionState::DISCONNECTED;
		m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Timeout waiting for response to challenge request");
		return 1;
	}
	if (challengeResponsePacket->ReadAndValidateCRC())
	{
		m_connectionState = NetworkConnectionState::DISCONNECTED;
		m_logger->Log(LogLevel::WARNING, "EstablishConnection: Challenge packet validation failed");
		return 1;
	}

	if (challengeResponsePacket->ReadNetworkPacketType() != NetworkPacketType::CONNECTION_ACCEPTED)
	{
		m_connectionState = NetworkConnectionState::DISCONNECTED;
		m_logger->Log(LogLevel::WARNING, "EstablishConnection: Connection was not accepted");
		return 1;
	}

	m_connectionState = NetworkConnectionState::CONNECTED;
    m_clientSalt = clientSalt;
    m_serverSalt = serverSalt;
    m_connectionSalt = clientSalt ^ serverSalt;
	m_logger->Log(LogLevel::INFO, "EstablishConnection: Connected");

	return 0;
}

std::unique_ptr<NetworkPacket> ClientNetwork::Receive(sockaddr_in& clientAddr, int& result)
{
	// Resize the vector to the maximum buffer size
    std::vector<uint8_t> data(1024);

	socklen_t addrLen = sizeof(clientAddr);
	int n = recvfrom(
		m_socket,
		reinterpret_cast<char*>(data.data()),
		(int)data.size(),
		0,
		reinterpret_cast<sockaddr*>(&clientAddr),
		&addrLen);

	if (n == SOCKET_ERROR)
	{
		auto errorCode = GetNetworkLastError();
		if (errorCode == SOCKET_TIMEOUT)
		{
			// Timeout, no data received
			result = -1;
			return nullptr;
		}

		std::string errorMsg = GetNetworkErrorMessage(errorCode);
		m_logger->Log(LogLevel::EXCEPTION, "Receive: Failed", { KV(errorCode), KVS(errorMsg) });
		result = 1;
		return nullptr;
	}
	if (n == 0)
	{
		m_logger->Log(LogLevel::WARNING, "Receive: No data received");
		result = 1;
		return nullptr;
	}

	// Resize the vector to the actual number of bytes received
	data.resize(n);

	// Log the received buffer as comma separated values as string
#if _DEBUG
	std::string bufferString;
	for (size_t i = 0; i < data.size(); i++)
	{
		bufferString += std::to_string(data[i]);
		if (i != data.size() - 1)
		{
			bufferString += ",";
		}
	}
	m_logger->Log(LogLevel::DEBUG, "Receive: Received bytes", { KV(n), KVS(bufferString) });
#endif

	return std::make_unique<NetworkPacket>(data);
}

int ClientNetwork::Send(NetworkPacket& networkPacket)
{
	networkPacket.CalculateCRC();

	auto size = networkPacket.Size();
	auto data = networkPacket.ToBytes();

#if _DEBUG
	// Log the send buffer as comma separated values as string
	std::string bufferString;
	for (size_t i = 0; i < data.size(); i++)
	{
		bufferString += std::to_string(data[i]);
		if (i != data.size() - 1)
		{
			bufferString += ",";
		}
	}
	m_logger->Log(LogLevel::DEBUG, "Send: Sending bytes", { KV(size), KVS(bufferString) });
#endif

	if (sendto(m_socket, (char*)data.data(), (int)size, 0, (sockaddr*)&m_servaddr, sizeof(m_servaddr)) != size)
	{
		auto errorCode = GetNetworkLastError();
		std::string errorMsg = GetNetworkErrorMessage(errorCode);
		m_logger->Log(LogLevel::EXCEPTION, "Send: Failed", { KV(errorCode), KVS(errorMsg) });
		return 1;
	}
	return 0;
}