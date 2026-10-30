#ifndef UDP_COMMON_H
#define UDP_COMMON_H

#ifdef _WIN32
#ifndef WINVER
#define WINVER 0x0601
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif
using SocketHandle = SOCKET;
using SocketLength = int;
static const SocketHandle INVALID_UDP_SOCKET = INVALID_SOCKET;
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
using SocketHandle = int;
using SocketLength = socklen_t;
static const SocketHandle INVALID_UDP_SOCKET = -1;
#endif

#include <cstdio>
#include <chrono>
#include <thread>
#include <ctime>

inline bool initSockets() {
#ifdef _WIN32
    WSADATA wsa;
    int error = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (error != 0) {
        std::fprintf(stderr, "WSAStartup failed: %d\n", error);
        return false;
    }
#endif
    return true;
}

inline void cleanupSockets() {
#ifdef _WIN32
    WSACleanup();
#endif
}

inline void closeSocket(SocketHandle s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

inline int socketError(const char* operation) {
#ifdef _WIN32
    std::fprintf(stderr, "%s failed: WSA error %d\n", operation, WSAGetLastError());
#else
    int error = errno;
    std::fprintf(stderr, "%s failed: %s (%d)\n", operation, std::strerror(error), error);
#endif
    return 1;
}

inline bool parseIPv4(const char* ip, in_addr* address) {
#ifdef _WIN32
    return InetPtonA(AF_INET, ip, address) == 1;
#else
    return inet_pton(AF_INET, ip, address) == 1;
#endif
}

inline const char* formatIPv4(in_addr address, char* buffer) {
#ifdef _WIN32
    const char* result = InetNtopA(AF_INET, &address, buffer, INET_ADDRSTRLEN);
#else
    const char* result = inet_ntop(AF_INET, &address, buffer, INET_ADDRSTRLEN);
#endif
    return result ? result : "?";
}

// Wall-clock text is for logs, not for precise cross-computer timing.
inline void formatTimestamp(char* buffer, std::size_t capacity) {
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
    const int ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - seconds).count());
    const std::time_t value = std::chrono::system_clock::to_time_t(seconds);
    std::tm local = {};
#ifdef _WIN32
    localtime_s(&local, &value);
#else
    localtime_r(&value, &local);
#endif
    std::snprintf(buffer, capacity, "%02d:%02d:%02d.%03d",
                  local.tm_hour, local.tm_min, local.tm_sec, ms);
}

#endif
