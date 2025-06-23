#include <random>
#include <cstdint>
#include <algorithm> 
#include "Network.h"
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
	auto messageCStr = std::strerror(errorCode);
    std::string message(messageCStr);

	std::ranges::replace(message, '\r', ' ');
	std::ranges::replace(message, '\n', ' ');
	std::ranges::replace(message, '"', '\'');

	return message;
}
#endif

Network::Network(std::shared_ptr<Logger> logger) : m_logger(logger), m_socket(0)
{
}

Network::~Network()
{
	// Close the socket and cleanup
#ifdef _WIN32
	closesocket(m_socket);
	WSACleanup();
#else
	close(m_socket);
#endif
}

int Network::Initialize(std::string server, int port, sockaddr_in& addr)
{
#ifdef _WIN32
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
		m_logger->Log(
			LogLevel::EXCEPTION,
			"Initialize: Socket creation failed",
			{ KV(errorCode) }
		);
		return 1;
	}

	// Set socket to non-blocking mode
#ifdef _WIN32
	u_long nonBlocking = 1;
	if (ioctlsocket(m_socket, FIONBIO, &nonBlocking) != 0) {
		auto errorCode = GetNetworkLastError();
		std::string errorMsg = GetNetworkErrorMessage(errorCode);
		m_logger->Log(
			LogLevel::EXCEPTION,
			"Initialize: Failed to set socket to non-blocking",
			{ KV(errorCode), KVS(errorMsg) }
		);
		return 1;
	}
#else
	int nonBlocking = 1;
	if (fcntl(m_socket, F_SETFL, O_NONBLOCK, &nonBlocking) == -1)
	{
		auto errorCode = GetNetworkLastError();
		std::string errorMsg = GetNetworkErrorMessage(errorCode);
		m_logger->Log(
			LogLevel::EXCEPTION,
			"Initialize: Failed to set socket to non-blocking",
			{ KV(errorCode), KVS(errorMsg) }
		);
		return 1;
	}
#endif

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (server.empty())
    {
        // Server
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        m_logger->Log(LogLevel::INFO, "Initialize: Binding on port", { KV(port) });
        if (bind(m_socket, (sockaddr*)&addr, sizeof(addr)) < 0)
        {
            m_logger->Log(LogLevel::EXCEPTION, "Initialize: Failed to bind socket on port", { KV(port) });
            return 1;
        }

        m_logger->Log(LogLevel::INFO, "Initialize: Successfully bound on port", { KV(port) });
    }
    else
    {
        // Client

        if (inet_pton(AF_INET, server.c_str(), &addr.sin_addr) != 1)
        {
            m_logger->Log(LogLevel::EXCEPTION, "Initialize: Failed to convert address", { KVS(server) });
            return 1;
        }
    }
	return 0;
}

std::unique_ptr<NetworkPacket> Network::Receive(sockaddr_in& clientAddr, int& result)
{
    const int len = 1024;
    std::vector<uint8_t> data(len);

	socklen_t addrLen = sizeof(clientAddr);
	int n = recvfrom(
		m_socket,
		reinterpret_cast<char*>(data.data()),
        len,
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
		m_logger->Log(
			LogLevel::EXCEPTION,
			"Receive: Failed",
			{ KV(errorCode), KVS(errorMsg) }
		);
		result = 1;
		return nullptr;
	}
	if (n == 0)
	{
		m_logger->Log(LogLevel::WARNING, "Receive: No data received");
		result = 1;
		return nullptr;
	}

    // Resize to actual received size
    data.resize(n);

	// Log the received buffer as comma separated values as string
#if _DEBUG
	std::string bufferString;
	for (size_t i = 0; i < n; i++)
	{
		bufferString += std::to_string(data[i]);
		if (i != n - 1)
		{
			bufferString += ",";
		}
	}
	m_logger->Log(
		LogLevel::DEBUG,
		"Receive: Received bytes",
		{ KV(n), KVS(bufferString) }
	);
#endif

	return std::make_unique<NetworkPacket>(data);
}

int Network::Send(NetworkPacket& networkPacket, sockaddr_in& clientAddr)
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
	m_logger->Log(
		LogLevel::DEBUG,
		"Send: Sending bytes",
		{ KV(size), KVS(bufferString) }
	);
#endif

	if (sendto(m_socket, (char*)data.data(), (int)size, 0, (sockaddr*)&clientAddr, sizeof(clientAddr)) != size)
	{
		auto errorCode = GetNetworkLastError();
		std::string errorMsg = GetNetworkErrorMessage(errorCode);
		m_logger->Log(
			LogLevel::EXCEPTION,
			"Send: Failed",
			{ KV(errorCode), KVS(errorMsg) }
		);
		return 1;
	}
	return 0;
}
