// Windows 7 / macOS: receive -> forward unchanged, with local interval timing.
#include "udp_common.h"

static const unsigned short LISTEN_PORT = 9000;
static const char* SOURCE_IP = "127.0.0.1";
static const char* DEST_IP = "127.0.0.1";
static const unsigned short DEST_PORT = 9001;

static int forwardLoop(SocketHandle s) {
    in_addr allowedSource = {};
    sockaddr_in destination = {};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(DEST_PORT);
    if (!parseIPv4(SOURCE_IP, &allowedSource) ||
        !parseIPv4(DEST_IP, &destination.sin_addr)) {
        std::fputs("Invalid SOURCE_IP or DEST_IP.\n", stderr);
        return 1;
    }

    sockaddr_in local = {};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(LISTEN_PORT);
    if (bind(s, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) < 0)
        return socketError("bind");

#ifdef _WIN32
    // This MinGW runtime's steady_clock falls back to gettimeofday, which is
    // too coarse on Windows 7. Use QPC explicitly for sub-millisecond intervals.
    LARGE_INTEGER frequency;
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0) {
        std::fputs("QueryPerformanceFrequency failed.\n", stderr);
        return 1;
    }
    std::printf("Timer=QueryPerformanceCounter; frequency=%lld Hz\n",
                static_cast<long long>(frequency.QuadPart));
#endif

    std::printf("Listening on UDP %u; accepting source IP %s\n",
                (unsigned)LISTEN_PORT, SOURCE_IP);
    std::printf("Forward to %s:%u. Ctrl+C to stop.\n",
                DEST_IP, (unsigned)DEST_PORT);

    char buffer[65536];
    for (;;) {
        sockaddr_in sender = {};
        SocketLength senderLength = sizeof(sender);
        const int received = static_cast<int>(recvfrom(s, buffer, sizeof(buffer), 0,
            reinterpret_cast<sockaddr*>(&sender), &senderLength));
#ifdef _WIN32
        LARGE_INTEGER start;
        QueryPerformanceCounter(&start);
#else
        const auto start = std::chrono::steady_clock::now();
#endif
        if (received < 0) return socketError("recvfrom");
        if (sender.sin_addr.s_addr != allowedSource.s_addr) continue;

        // Preserve the datagram's byte count, including binary/empty payloads.
        const int sent = static_cast<int>(sendto(s, buffer, received, 0,
            reinterpret_cast<const sockaddr*>(&destination), sizeof(destination)));
#ifdef _WIN32
        LARGE_INTEGER finish;
        QueryPerformanceCounter(&finish);
#else
        const auto finish = std::chrono::steady_clock::now();
#endif
        if (sent < 0) return socketError("sendto");
        if (sent != received) {
            std::fputs("Unexpected sent byte count.\n", stderr);
            return 1;
        }
#ifdef _WIN32
        const double localMs = static_cast<double>(finish.QuadPart - start.QuadPart)
            * 1000.0 / static_cast<double>(frequency.QuadPart);
#else
        const double localMs =
            std::chrono::duration<double, std::milli>(finish - start).count();
#endif
        std::printf("Forwarded %d bytes from %s:%u to %s:%u; local elapsed=%.6f ms\n",
                    received, SOURCE_IP, (unsigned)ntohs(sender.sin_port),
                    DEST_IP, (unsigned)DEST_PORT, localMs);
    }
}

int main() {
    std::setvbuf(stdout, NULL, _IONBF, 0);
    if (!initSockets()) return 1;
    SocketHandle s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int result;
    if (s == INVALID_UDP_SOCKET) {
        result = socketError("socket");
    } else {
        result = forwardLoop(s);
        closeSocket(s);
    }
    cleanupSockets();
    return result;
}
