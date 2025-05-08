#include "Network.h"
#include "NetworkPacketType.h"
#include <random>
#include <cstdint>

std::string GetWSAErrorMessage(int errorCode)
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

Network::Network(std::shared_ptr<Logger> logger) : m_logger(logger)
{
}

Network::~Network()
{
	// Close the socket and cleanup
	closesocket(m_socket);

#ifdef _WIN32
	WSACleanup();
#else
	close(m_socket);
#endif
}

int Network::Initialize(std::string server, int port)
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
		auto errorCode = WSAGetLastError();
		m_logger->Log(LogLevel::EXCEPTION, "Initialize: Socket creation failed with error: {}", errorCode);
		return 1;
	}

	// Set receive timeout
#ifdef _WIN32
	int timeoutMs = 1000; // 1 seconds
	if (setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs)) < 0) {
		auto errorCode = WSAGetLastError();
		std::string errorMsg = GetWSAErrorMessage(errorCode);
		m_logger->Log(LogLevel::EXCEPTION, "Initialize: Failed to set socket timeout: {}: {}", errorCode, errorMsg);
		return 1;
	}
#else
	struct timeval timeout;
	timeout.tv_sec = 1; // 5 seconds
	timeout.tv_usec = 0; // 0 microseconds
	setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif

	m_servaddr.sin_family = AF_INET;
	m_servaddr.sin_port = htons(port);

	if (server.empty())
	{
		m_servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

		m_logger->Log(LogLevel::INFO, "Initialize: Bind on port: {}", port);
		if (bind(m_socket, (sockaddr*)&m_servaddr, sizeof(m_servaddr)) < 0)
		{
			m_logger->Log(LogLevel::EXCEPTION, "Initialize: Failed to bind socket on port: {}", port);
			return 1;
		}
	}
	else
	{
		if (inet_pton(AF_INET, server.c_str(), &m_servaddr.sin_addr) != 1)
		{
			m_logger->Log(LogLevel::EXCEPTION, "Initialize: Failed to convert address: {}", server);
			return 1;
		}
	}

	return 0;
}

int Network::EstablishConnection()
{
	// Create random salt value
	std::random_device rd; // Non-deterministic random number generator
	std::mt19937_64 gen(rd()); // Seed the Mersenne Twister with random_device
	std::uniform_int_distribution<uint64_t> dis(0, UINT64_MAX); // Uniform distribution

	uint64_t clientSalt = dis(gen); // Generate a random 64-bit number

	std::unique_ptr<NetworkPacket> connectionPacket = std::make_unique<NetworkPacket>();
	connectionPacket->WriteInt8(static_cast<int8_t>(NetworkPacketType::CONNECTION_REQUEST));
	connectionPacket->WriteInt64(clientSalt);
	// Pad the packet to 1000 bytes
	for (size_t i = 0; i < 250; i++)
	{
		connectionPacket->WriteInt32(0);
	}

	auto data = connectionPacket->ToBytes();

	m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Send connection request");

	if (Send(data) != 0) return 1;

	std::vector<uint8_t> responseData;
	sockaddr_in clientAddr{};

	m_logger->Log(LogLevel::DEBUG, "EstablishConnection: Waiting for response");

	if (Receive(responseData, clientAddr)) return 1;

	if (connectionPacket->Validate(responseData)) return 1;

	size_t offset = CRC32::CRC_SIZE;

	// Check if the packet type is correct
	if (connectionPacket->ReadInt8(offset) != static_cast<int8_t>(NetworkPacketType::CHALLENGE))
	{
		m_logger->Log(LogLevel::WARNING, "EstablishConnection: Invalid packet type");
		return 1;
	}

	uint64_t receivedClientSalt = connectionPacket->ReadInt64(offset);
	if (receivedClientSalt != clientSalt)
	{
		m_logger->Log(LogLevel::WARNING, "EstablishConnection: Client salt mismatch: {} <> {}", clientSalt, receivedClientSalt);
		return 1;
	}

	// Extract server salt from the response
	uint64_t serverSalt = connectionPacket->ReadInt64(offset);
	m_connectionSalt = clientSalt ^ serverSalt;

	connectionPacket->Clear();

	// Create a new packet with the connection salt
	connectionPacket->WriteInt8(static_cast<int8_t>(NetworkPacketType::CHALLENGE_RESPONSE));
	connectionPacket->WriteInt64(m_connectionSalt);
	// Pad the packet to 1000 bytes
	for (size_t i = 0; i < 250; i++)
	{
		connectionPacket->WriteInt32(0);
	}
	data = connectionPacket->ToBytes();
	if (Send(data) != 0) return 1;
	responseData.clear();
	if (Receive(responseData, clientAddr)) return 1;
	if (connectionPacket->Validate(responseData)) return 1;
	// Extract server salt from the response
	offset = CRC32::CRC_SIZE;
	uint64_t serverSalt2 = connectionPacket->ReadInt64(offset);

	return 0;
}

int Network::Receive(std::vector<uint8_t>& data, sockaddr_in& clientAddr)
{
	// Resize the vector to the maximum buffer size
	data.resize(1024);

	socklen_t addrLen = sizeof(clientAddr);
	int n = recvfrom(
		m_socket, 
		reinterpret_cast<char*>(data.data()), 
		(int)data.size(), 
		0, 
		reinterpret_cast<SOCKADDR*>(&clientAddr), 
		&addrLen);

	if (n == SOCKET_ERROR)
	{
		auto errorCode = WSAGetLastError();
		if (errorCode == WSAETIMEDOUT)
		{
			// Timeout, no data received
			return -1;
		}

		std::string errorMsg = GetWSAErrorMessage(errorCode);
		m_logger->Log(LogLevel::EXCEPTION, "Receive: Failed with error: {}: {}", errorCode, errorMsg);
		return 1;
	}
	if (n == 0)
	{
		m_logger->Log(LogLevel::WARNING, "Receive: No data received");
		return 1;
	}

	// Resize the vector to the actual number of bytes received
	data.resize(n);

	// Log the received buffer as comma seperated values as string
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

	return 0;
}

int Network::Send(std::vector<uint8_t>& data)
{
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
	auto size = data.size();
	m_logger->Log(LogLevel::DEBUG, "Send: Sending {} bytes: {}", size, bufferString);

	if (sendto(m_socket, (char*)data.data(), (int)size, 0, (SOCKADDR*)&m_servaddr, sizeof(m_servaddr)) == SOCKET_ERROR)
	{
		auto errorCode = WSAGetLastError();
		std::string errorMsg = GetWSAErrorMessage(errorCode);
		m_logger->Log(LogLevel::EXCEPTION, "Send: Failed with error: {}: {}", errorCode, errorMsg);
		return 1;
	}
	return 0;
}
