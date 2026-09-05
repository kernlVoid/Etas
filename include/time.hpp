#pragma once

#pragma comment(lib, "ws2_32.lib")

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <cstdint>
#include <stdexcept>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <chrono>

namespace Etas {


class time {
private:
    // Save real time
    std::int64_t rTime{0}; 

public:
    std::int64_t getTimestamp() const noexcept {
        return rTime;
    }

    /**
     * @brief Fetches the current system time from the OS.
     * High-speed and ultra-safe using C++ chrono.
     */
    void getTimeOS() noexcept {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        rTime = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    }

    /**
     * @brief Fetches the time from a highly reliable NTP pool server.
     * Ultra-fast UDP implementation with built-in error handling.
     */
    void getTimeWeb() {
        // Initialisiere Winsock (nur für Windows benötigt)
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw std::runtime_error("Winsock initialization failed.");
        }

        // take the fast ntp port/pool
        const char* server_name = "pool.ntp.org";
        const char* port = "123";

        struct addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_UNSPEC;     // IPv4 or IPv6
        hints.ai_socktype = SOCK_DGRAM;  // UDP (Ultra fast)
        hints.ai_protocol = IPPROTO_UDP;

        if (getaddrinfo(server_name, port, &hints, &res) != 0) {
            WSACleanup();
            throw std::runtime_error("DNS resolution failed for NTP server.");
        }

        SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sock == INVALID_SOCKET) {
            freeaddrinfo(res);
            WSACleanup();
            throw std::runtime_error("Socket creation failed.");
        }

        // Timeout for 2s
        DWORD timeout = 2000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

        // 48-Byte NTP Request-Paket (Modus 3 = Client)
        unsigned char packet[48] = {0};
        packet[0] = 0x1B; 

        if (sendto(sock, reinterpret_cast<const char*>(packet), sizeof(packet), 0, res->ai_addr, static_cast<int>(res->ai_addrlen)) == SOCKET_ERROR) {
            closesocket(sock);
            freeaddrinfo(res);
            WSACleanup();
            throw std::runtime_error("Failed to send NTP packet.");
        }

        // get result 
        int bytes_received = recv(sock, reinterpret_cast<char*>(packet), sizeof(packet), 0);
        
        // now free the result
        closesocket(sock);
        freeaddrinfo(res);
        WSACleanup();

        if (bytes_received < 48) {
            throw std::runtime_error("Invalid or timed out response from NTP server.");
        }

        // Zeitstempel extrahieren (Bytes 40-43 enthalten die Sekunden seit 1900)
        std::uint32_t secs_since_1900 = (static_cast<std::uint32_t>(packet[40]) << 24) |
                                        (static_cast<std::uint32_t>(packet[41]) << 16) |
                                        (static_cast<std::uint32_t>(packet[42]) << 8)  |
                                        static_cast<std::uint32_t>(packet[43]);

        // Differenz zwischen NTP-Epoche (1900) und Unix-Epoche (1970) abziehen
        const std::uint32_t ntp_to_unix_epoch = 2208988800U;
        rTime = static_cast<std::int64_t>(secs_since_1900 - ntp_to_unix_epoch);
    }
};

} // namespace EtasSaaS
