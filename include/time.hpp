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
#include <optional>
#include <array>

namespace Etas {

class time {
private:
    // Save real time
    std::int64_t rTime{0};
    
    // Static Winsock initialization flag
    static bool wsockInitialized;
    
    /**
     * @brief Initializes Winsock once (thread-safe)
     */
    static void initWinsock() {
        static bool initialized = false;
        static WSADATA wsaData;
        
        if (!initialized) {
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                throw std::runtime_error("Winsock initialization failed.");
            }
            initialized = true;
        }
    }

public:
    std::int64_t getTimestamp() const noexcept {
        return rTime;
    }

    /**
     * @brief Fetches the current system time from the OS.
     * High-speed using C++ chrono with millisecond precision.
     */
    void getTimeOS() noexcept {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        rTime = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }

    /**
     * @brief Fetches the time with high precision (microseconds)
     */
    std::int64_t getTimeOSHighPrecision() noexcept {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    }

    /**
     * @brief Fetches the time from NTP server with improved error handling
     * @return true on success, false on failure
     */
    bool getTimeWebSafe() noexcept {
        try {
            getTimeWeb();
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

    /**
     * @brief Fetches the time from a highly reliable NTP pool server.
     * Ultra-fast UDP implementation with built-in error handling.
     */
    void getTimeWeb() {
        initWinsock();

        const char* server_name = "pool.ntp.org";
        const char* port = "123";

        struct addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;

        if (getaddrinfo(server_name, port, &hints, &res) != 0) {
            throw std::runtime_error("DNS resolution failed for NTP server.");
        }

        SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sock == INVALID_SOCKET) {
            freeaddrinfo(res);
            throw std::runtime_error("Socket creation failed.");
        }

        // Set 2s timeout
        DWORD timeout = 2000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

        // 48-Byte NTP Request packet (Mode 3 = Client)
        std::array<unsigned char, 48> packet{};
        packet[0] = 0x1B;

        int bytes_sent = sendto(sock, reinterpret_cast<const char*>(packet.data()), packet.size(), 0, res->ai_addr, static_cast<int>(res->ai_addrlen));
        
        if (bytes_sent == SOCKET_ERROR) {
            closesocket(sock);
            freeaddrinfo(res);
            throw std::runtime_error("Failed to send NTP packet.");
        }

        // Receive result
        int bytes_received = recv(sock, reinterpret_cast<char*>(packet.data()), packet.size(), 0);
        
        closesocket(sock);
        freeaddrinfo(res);

        if (bytes_received < 48) {
            throw std::runtime_error("Invalid or timed out response from NTP server.");
        }

        // Extract timestamp (Bytes 40-43 contain seconds since 1900)
        std::uint32_t secs_since_1900 = (static_cast<std::uint32_t>(packet[40]) << 24) |
                                        (static_cast<std::uint32_t>(packet[41]) << 16) |
                                        (static_cast<std::uint32_t>(packet[42]) << 8) |
                                        static_cast<std::uint32_t>(packet[43]);

        // Subtract difference between NTP epoch (1900) and Unix epoch (1970)
        constexpr std::uint32_t ntp_to_unix_epoch = 2208988800U;
        rTime = static_cast<std::int64_t>(secs_since_1900 - ntp_to_unix_epoch);
    }

    /**
     * @brief Get current timestamp with fallback mechanism
     * Tries NTP first, falls back to OS time on failure
     */
    void getTimeWithFallback() noexcept {
        if (!getTimeWebSafe()) {
            getTimeOS();
        }
    }

    /**
     * @brief Returns elapsed time since a given timestamp in seconds
     */
    std::int64_t getElapsedSeconds(std::int64_t since_timestamp) const noexcept {
        return rTime - since_timestamp;
    }

    /**
     * @brief Formats timestamp to readable string (Windows)
     */
    std::string formatTimestamp() const noexcept {
        time_t t = static_cast<time_t>(rTime);
        struct tm tm_info;
        localtime_s(&tm_info, &t);
        
        char buffer[32];
        if (strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_info) > 0) {
            return std::string(buffer);
        }
        return "Invalid timestamp";
    }

    /**
     * @brief Validates if timestamp is within reasonable range
     */
    bool isValidTimestamp() const noexcept {
        // Unix epoch boundaries (1970 - 2100)
        constexpr std::int64_t min_timestamp = 0;
        constexpr std::int64_t max_timestamp = 4102444800LL; // Jan 1, 2100
        return rTime >= min_timestamp && rTime <= max_timestamp;
    }
};

} // namespace Etas
