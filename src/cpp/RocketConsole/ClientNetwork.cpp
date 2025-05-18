#include "ClientNetwork.h"
#include "NetworkPacketType.h"
#include <random>
#include <cstdint>
#include "Utils.h"

static std::string GetWSAErrorMessage(int errorCode)
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
	return message;
}

ClientNetwork::ClientNetwork(std::shared_ptr<Logger> logger) : m_logger(logger), m_socket(0), m_connectionSalt(0), m_servaddr{}
{
}

ClientNetwork::~ClientNetwork()
{
	// Close the socket and cleanup
	closesocket(m_socket);

#ifdef _WIN32
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
		auto errorCode = GetLastError();
		m_logger->Log(LogLevel::EXCEPTION, "Initialize: Socket creation failed with error: {}", errorCode);
		return 1;
	}

	// Set receive timeout
#ifdef _WIN32
	int timeoutMs = 5000; // 1 seconds
	if (setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs)) < 0) {
		auto errorCode = GetLastError();
		std::string errorMsg = GetWSAErrorMessage(errorCode);
		m_logger->Log(LogLevel::EXCEPTION, "Initialize: Failed to set socket timeout: {}: {}", errorCode, errorMsg);
		return 1;
	}
#else
	struct timeval timeout;
	timeout.tv_sec = 5; // 5 seconds
	timeout.tv_usec = 0; // 0 microseconds
	setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif

	m_servaddr.sin_family = AF_INET;
	m_servaddr.sin_port = htons(port);

	if (inet_pton(AF_INET, server.c_str(), &m_servaddr.sin_addr) != 1)
	{
		m_logger->Log(LogLevel::EXCEPTION, "Initialize: Failed to convert address: {}", server);
		return 1;
	}

	return 0;
}

bool ClientNetwork::IsServerAddress(sockaddr_in& clientAddr) const
{
	return (m_servaddr.sin_family == clientAddr.sin_family &&
		m_servaddr.sin_addr.s_addr == clientAddr.sin_addr.s_addr &&
		m_servaddr.sin_port == clientAddr.sin_port);
}

int ClientNetwork::EstablishConnection()
{
	uint64_t clientSalt = Utils::GetRandomNumber64();
	std::unique_ptr<NetworkPacket> networkPacket = std::make_unique<NetworkPacket>();

	m_connectionState = NetworkConnectionState::CONNECTING;
	networkPacket->WriteInt8(static_cast<int8_t>(NetworkPacketType::CONNECTION_REQUEST));
	networkPacket->WriteInt64(clientSalt);

	// Pad the packet to 1000 bytes
	for (size_t i = 0; i < 1000 - sizeof(uint32_t) - sizeof(uint64_t); i++)
	{
		networkPacket->WriteInt8(0);
	}

	int result = 0;

	m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Send connection request");

	if (Send(*networkPacket) != 0)
	{
		m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Failed to send connection request");
		return 1;
	}

	m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Waiting for response");

	sockaddr_in clientAddr{};
	networkPacket = Receive(clientAddr, result);

	if (result != 0)
	{
		m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Timeout waiting for response to connection request");
		return 1;
	}

	if (networkPacket->Validate())
	{
		m_connectionState = NetworkConnectionState::DISCONNECTED;
		m_logger->Log(LogLevel::WARNING, "EstablishConnection: Packet validation failed");
		return 1;
	}

	if (networkPacket->GetNetworkPacketType() != NetworkPacketType::CHALLENGE)
	{
		m_connectionState = NetworkConnectionState::DISCONNECTED;
		m_logger->Log(LogLevel::WARNING, "EstablishConnection: Invalid packet type");
		return 1;
	}

	uint64_t receivedClientSalt = networkPacket->ReadInt64();
	if (receivedClientSalt != clientSalt)
	{
		m_connectionState = NetworkConnectionState::DISCONNECTED;
		m_logger->Log(LogLevel::WARNING, "EstablishConnection: Client salt mismatch: {} <> {}", clientSalt, receivedClientSalt);
		return 1;
	}

	// Extract server salt from the response
	uint64_t serverSalt = networkPacket->ReadInt64();
	m_connectionSalt = clientSalt ^ serverSalt;

	networkPacket->Clear();

	// Create a new packet with the connection salt
	networkPacket->WriteInt8(static_cast<int8_t>(NetworkPacketType::CHALLENGE_RESPONSE));
	networkPacket->WriteInt64(m_connectionSalt);
	// Pad the packet to 1000 bytes
	for (size_t i = 0; i < 1000 - sizeof(uint32_t) - sizeof(uint64_t); i++)
	{
		networkPacket->WriteInt8(0);
	}
	if (Send(*networkPacket) != 0)
	{
		m_connectionState = NetworkConnectionState::DISCONNECTED;
		m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Failed to send challenge request");
		return 1;
	}

	networkPacket = Receive(clientAddr, result);
	if (result != 0)
	{
		m_connectionState = NetworkConnectionState::DISCONNECTED;
		m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Timeout waiting for response to challenge request");
		return 1;
	}
	if (networkPacket->Validate())
	{
		m_connectionState = NetworkConnectionState::DISCONNECTED;
		m_logger->Log(LogLevel::WARNING, "EstablishConnection: Challenge packet validation failed");
		return 1;
	}

	if (networkPacket->GetNetworkPacketType() != NetworkPacketType::CONNECTION_ACCEPTED)
	{
		m_connectionState = NetworkConnectionState::DISCONNECTED;
		m_logger->Log(LogLevel::WARNING, "EstablishConnection: Connection was not accepted");
		return 1;
	}

	// TODO: Read connection accepted payload
	//offset = NetworkPacket::PAYLOAD_START_INDEX;
	//uint64_t serverSalt2 = networkPacket->ReadInt64(offset);

	m_connectionState = NetworkConnectionState::CONNECTED;
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
		auto errorCode = GetLastError();
		if (errorCode == WSAETIMEDOUT)
		{
			// Timeout, no data received
			result = -1;
			return nullptr;
		}

		std::string errorMsg = GetWSAErrorMessage(errorCode);
		m_logger->Log(LogLevel::EXCEPTION, "Receive: Failed with error: {}: {}", errorCode, errorMsg);
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

	// Log the received buffer as comma seperated values as string
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
	m_logger->Log(LogLevel::DEBUG, "Receive: Received {} bytes: {}", n, bufferString);
#endif

	return std::make_unique<NetworkPacket>(data);
}

int ClientNetwork::Send(NetworkPacket& networkPacket)
{
	networkPacket.CalculateCRC();

	auto size = networkPacket.Size();
	auto data = networkPacket.ToBytes();

#if _DEBUG
	// Log the send buffer as comma seperated values as string
	std::string bufferString;
	for (size_t i = 0; i < data.size(); i++)
	{
		bufferString += std::to_string(data[i]);
		if (i != data.size() - 1)
		{
			bufferString += ",";
		}
	}
	m_logger->Log(LogLevel::DEBUG, "Send: Sending {} bytes: {}", size, bufferString);
#endif

	if (sendto(m_socket, (char*)data.data(), (int)size, 0, (sockaddr*)&m_servaddr, sizeof(m_servaddr)) == SOCKET_ERROR)
	{
		auto errorCode = GetLastError();
		std::string errorMsg = GetWSAErrorMessage(errorCode);
		m_logger->Log(LogLevel::EXCEPTION, "Send: Failed with error: {}: {}", errorCode, errorMsg);
		return 1;
	}
	return 0;
}
