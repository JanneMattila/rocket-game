#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <thread>
#include <cstring>
#include <string>
#include <cstdlib>
#include <algorithm>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include "NetworkPacket.h"

struct RocketPlayer {
    uint8_t PlayerID = 0;
    sockaddr_in Address{};
    std::chrono::steady_clock::time_point Created;
    std::chrono::steady_clock::time_point LastUpdated;
    int Messages = 0;
    int64_t Ticks = 0;
    float PositionX = 0;
    float PositionY = 0;
    float VelocityX = 0;
    float VelocityY = 0;
    float Rotation = 0;
    float Speed = 0;
    uint8_t IsFiring = 0;
};

std::string addrToString(const sockaddr_in& addr) {
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf));
    return std::string(buf) + ":" + std::to_string(ntohs(addr.sin_port));
}

bool operator==(const sockaddr_in& a, const sockaddr_in& b) {
    return a.sin_family == b.sin_family &&
           a.sin_addr.s_addr == b.sin_addr.s_addr &&
           a.sin_port == b.sin_port;
}

namespace std {
    template<>
    struct hash<sockaddr_in> {
        size_t operator()(const sockaddr_in& addr) const {
            return hash<uint32_t>()(addr.sin_addr.s_addr) ^ hash<uint16_t>()(addr.sin_port);
        }
    };
}

int main() {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);
#endif
    int udpPort = 3501;
    const char* envPort = std::getenv("UDP_PORT");
    if (envPort) {
        udpPort = std::atoi(envPort);
    }
    std::cout << "UDP Port: " << udpPort << std::endl;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(udpPort);
    if (bind(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Failed to bind socket" << std::endl;
#ifdef _WIN32
        closesocket(sock);
        WSACleanup();
#else
        close(sock);
#endif
        return 1;
    }

    std::unordered_map<sockaddr_in, RocketPlayer> players;
    std::vector<uint8_t> buffer(NetworkPacket::ExpectedMessageSize);
    auto lastCleanup = std::chrono::steady_clock::now();

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t addrLen = sizeof(clientAddr);
        int received = recvfrom(sock, reinterpret_cast<char*>(buffer.data()), buffer.size(), 0, (sockaddr*)&clientAddr, &addrLen);
        if (received != NetworkPacket::ExpectedMessageSize) {
            continue;
        }
        try {
            NetworkPacket packet = NetworkPacket::FromBytes(buffer);
            auto now = std::chrono::steady_clock::now();
            auto it = players.find(clientAddr);
            if (it == players.end()) {
                RocketPlayer player;
                player.PlayerID = static_cast<uint8_t>(players.size() + 1);
                player.Address = clientAddr;
                player.Created = now;
                player.LastUpdated = now;
                players[clientAddr] = player;
                it = players.find(clientAddr);
            }
            RocketPlayer& player = it->second;
            player.Ticks = packet.Ticks;
            player.PositionX = packet.PositionX;
            player.PositionY = packet.PositionY;
            player.VelocityX = packet.VelocityX;
            player.VelocityY = packet.VelocityY;
            player.Rotation = packet.Rotation;
            player.Speed = packet.Speed;
            player.IsFiring = packet.IsFiring;
            player.LastUpdated = now;
            player.Messages++;
            // Forward to all other players
            for (const auto& kv : players) {
                if (!(kv.first == clientAddr)) {
                    sendto(sock, reinterpret_cast<const char*>(buffer.data()), buffer.size(), 0, (sockaddr*)&kv.first, sizeof(sockaddr_in));
                }
            }
            // Cleanup every second
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCleanup).count() >= 1000) {
                auto threshold = now - std::chrono::seconds(5);
                std::vector<sockaddr_in> toRemove;
                for (const auto& kv : players) {
                    if (kv.second.LastUpdated < threshold) {
                        toRemove.push_back(kv.first);
                    }
                }
                for (const auto& addr : toRemove) {
                    players.erase(addr);
                }
                lastCleanup = now;
            }
        } catch (const std::exception& ex) {
            // Ignore invalid packets
            continue;
        }
    }
#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif
    return 0;
}
