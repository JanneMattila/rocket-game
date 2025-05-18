#include <random>
#include <cstdint>
#ifdef _WIN32
#define SOCKET_TIMEOUT ETIMEDOUT
#else
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <cerrno>
// https://en.wikipedia.org/wiki/Errno.h
#define SOCKET_ERROR -1
#define SOCKET_TIMEOUT EAGAIN // 11
#define INVALID_SOCKET  (SOCKET)(~0)
#endif
#include "ServerNetwork.h"
#include "NetworkPacketType.h"
#include "Utils.h"

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
	return message;
}
#else
static inline int GetNetworkLastError()
{
	return errno;
}
static std::string GetNetworkErrorMessage(int errorCode)
{
	return std::strerror(errorCode);
}
#endif

ServerNetwork::ServerNetwork(std::shared_ptr<Logger> logger) : m_logger(logger), m_socket(0), m_servaddr{}
{
}

ServerNetwork::~ServerNetwork()
{
	// Close the socket and cleanup
#ifdef _WIN32
	closesocket(m_socket);
	WSACleanup();
#else
	close(m_socket);
#endif
}

int ServerNetwork::Initialize(int port)
{
#ifdef _WIN32
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		m_logger->Log(LogLevel::EXCEPTION, "Initialize: WSAStartup failed");
		return 1;
	}
#endif

	m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_socket == INVALID_SOCKET) {
		auto errorCode = GetNetworkLastError();
		m_logger->Log(LogLevel::EXCEPTION, "Initialize: Socket creation failed with error: {}", errorCode);
		return 1;
	}

	// Set receive timeout
#ifdef _WIN32
	int timeoutMs = 5000; // 1 seconds
	if (setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs)) < 0) {
		auto errorCode = GetNetworkLastError();
		std::string errorMsg = GetNetworkErrorMessage(errorCode);
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

	m_logger->Log(LogLevel::INFO, "Initialize: Binding on port: {}", port);
	if (bind(m_socket, (sockaddr*)&m_servaddr, sizeof(m_servaddr)) < 0)
	{
		//m_logger->Log(LogLevel::EXCEPTION, "Initialize: Failed to bind socket on port: {}", port);
		return 1;
	}

	m_logger->Log(LogLevel::INFO, "Initialize: Successfully bound on port: {}", port);

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
		reinterpret_cast<sockaddr*>(&clientAddr),
		&addrLen);

	if (n == SOCKET_ERROR)
	{
		m_logger->Log(LogLevel::DEBUG, "DEBUG 4: After SOCKET_ERROR");

		auto errorCode = GetNetworkLastError();

		m_logger->Log(LogLevel::DEBUG, "DEBUG 4: After SOCKET_ERROR {}", errorCode);

		if (errorCode == ETIMEDOUT)
		{
			m_logger->Log(LogLevel::DEBUG, "DEBUG 5: After ETIMEDOUT");

			// Timeout, no data received
			result = -1;
			return nullptr;
		}

		m_logger->Log(LogLevel::DEBUG, "DEBUG 6: Before GetNetworkErrorMessage");

		std::string errorMsg = GetNetworkErrorMessage(errorCode);
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

	m_logger->Log(LogLevel::DEBUG, "DEBUG 6: Before resize {}", n);

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

	if (sendto(m_socket, (char*)data.data(), (int)size, 0, (sockaddr*)&clientAddr, sizeof(clientAddr)) == SOCKET_ERROR)
	{
		auto errorCode = GetNetworkLastError();
		std::string errorMsg = GetNetworkErrorMessage(errorCode);
		m_logger->Log(LogLevel::EXCEPTION, "Send: Failed with error: {}: {}", errorCode, errorMsg);
		return 1;
	}
	return 0;
}