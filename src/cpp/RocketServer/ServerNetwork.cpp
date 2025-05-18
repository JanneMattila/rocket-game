#include <random>
#include <cstdint>
#include "ServerNetwork.h"
#include "NetworkPacketType.h"
#include "Utils.h"
#include "Platform.h"

#if PLATFORM == PLATFORM_WINDOWS
#define SOCKET_TIMEOUT ETIMEDOUT
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
#if PLATFORM == PLATFORM_WINDOWS
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
#if PLATFORM == PLATFORM_WINDOWS
	closesocket(m_socket);
	WSACleanup();
#else
	close(m_socket);
#endif
}

int ServerNetwork::Initialize(int port)
{
#if PLATFORM == PLATFORM_WINDOWS
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		m_logger->Log(LogLevel::EXCEPTION, "Initialize: WSAStartup failed");
		return 1;
	}
#endif

	m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_socket == INVALID_SOCKET)
	{
		auto errorCode = GetNetworkLastError();
		m_logger->Log(LogLevel::EXCEPTION, "Initialize: Socket creation failed with error: {}", errorCode);
		return 1;
	}

	// Set socket to non-blocking mode
#if PLATFORM == PLATFORM_WINDOWS
	u_long nonBlocking = 1;
	if (ioctlsocket(m_socket, FIONBIO, &nonBlocking) != 0){
		auto errorCode = GetNetworkLastError();
		std::string errorMsg = GetNetworkErrorMessage(errorCode);
		m_logger->Log(LogLevel::EXCEPTION, "Initialize: Failed to set socket to non-blocking: {}: {}", errorCode, errorMsg);
		return 1;
	}
#else
	int nonBlocking = 1;
	if (fcntl(m_socket, F_SETFL, O_NONBLOCK, &nonBlocking) == -1)
	{
		auto errorCode = GetNetworkLastError();
		std::string errorMsg = GetNetworkErrorMessage(errorCode);
		m_logger->Log(LogLevel::EXCEPTION, "Initialize: Failed to set socket to non-blocking: {}: {}", errorCode, errorMsg);
		return 1;
	}
#endif

	m_servaddr.sin_family = AF_INET;
	m_servaddr.sin_port = htons(port);
	m_servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	m_logger->Log(LogLevel::INFO, "Initialize: Binding on port: {}", port);
	if (bind(m_socket, (sockaddr*)&m_servaddr, sizeof(m_servaddr)) < 0)
	{
		m_logger->Log(LogLevel::EXCEPTION, "Initialize: Failed to bind socket on port: {}", port);
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
		auto errorCode = GetNetworkLastError();
		if (errorCode == ETIMEDOUT)
		{
			// Timeout, no data received
			result = -1;
			return nullptr;
		}

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

	if (sendto(m_socket, (char*)data.data(), (int)size, 0, (sockaddr*)&clientAddr, sizeof(clientAddr)) != size)
	{
		auto errorCode = GetNetworkLastError();
		std::string errorMsg = GetNetworkErrorMessage(errorCode);
		m_logger->Log(LogLevel::EXCEPTION, "Send: Failed with error: {}: {}", errorCode, errorMsg);
		return 1;
	}
	return 0;
}