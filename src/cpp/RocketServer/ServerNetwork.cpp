#include "ServerNetwork.h"
#include "NetworkPacketType.h"
#include <random>
#include <cstdint>
#include "Utils.h"

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

ServerNetwork::ServerNetwork(std::shared_ptr<Logger> logger) : m_logger(logger), m_socket(0), m_servaddr{}
{
}

ServerNetwork::~ServerNetwork()
{
	// Close the socket and cleanup
	closesocket(m_socket);

#ifdef _WIN32
	WSACleanup();
#else
	close(m_socket);
#endif
}

int ServerNetwork::Initialize(std::string server, int port)
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
	int timeoutMs = 5000; // 1 seconds
	if (setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs)) < 0) {
		auto errorCode = WSAGetLastError();
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
	m_servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	m_logger->Log(LogLevel::INFO, "Initialize: Bind on port: {}", port);
	if (bind(m_socket, (sockaddr*)&m_servaddr, sizeof(m_servaddr)) < 0)
	{
		m_logger->Log(LogLevel::EXCEPTION, "Initialize: Failed to bind socket on port: {}", port);
		return 1;
	}

	return 0;
}

std::unique_ptr<NetworkPacket> ServerNetwork::Receive(sockaddr_in& clientAddr, int &result)
{
	// Resize the vector to the maximum buffer size
	std::vector<uint8_t> data(1024);

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

int ServerNetwork::Send(NetworkPacket& networkPacket, sockaddr_in& clientAddr)
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

	if (sendto(m_socket, (char*)data.data(), (int)size, 0, (SOCKADDR*)&clientAddr, sizeof(clientAddr)) == SOCKET_ERROR)
	{
		auto errorCode = WSAGetLastError();
		std::string errorMsg = GetWSAErrorMessage(errorCode);
		m_logger->Log(LogLevel::EXCEPTION, "Send: Failed with error: {}: {}", errorCode, errorMsg);
		return 1;
	}
	return 0;
}