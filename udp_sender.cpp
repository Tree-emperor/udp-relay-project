// Windows 7 / macOS: send test messages to the relay, embedding a high-resolution
// send timestamp so the receiver can measure true one-way latency.
#include "udp_common.h"

// Default: all three programs run on one computer.
static const char* LOCAL_IP = "127.0.0.1";
static const char* RELAY_IP = "127.0.0.1";
static const unsigned short RELAY_PORT = 9000;
static const int PACKET_COUNT = 200;
static const int INTERVAL_MS = 0;

static int sendPackets(SocketHandle s) {
    sockaddr_in local = {}, relay = {};
    local.sin_family = AF_INET;
    local.sin_port = 0;
    relay.sin_family = AF_INET;
    relay.sin_port = htons(RELAY_PORT);
    if (!parseIPv4(LOCAL_IP, &local.sin_addr) ||
        !parseIPv4(RELAY_IP, &relay.sin_addr)) {
        std::fputs("Invalid LOCAL_IP or RELAY_IP.\n", stderr);
        return 1;
    }
    if (bind(s, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) < 0) {
        std::fputs("LOCAL_IP must belong to this computer.\n", stderr);
        return socketError("bind");
    }

    for (int i = 1; i <= PACKET_COUNT; ++i) {
        // Embed a high-resolution timestamp (nanoseconds since the Unix epoch).
        // system_clock's epoch is identical across processes on the same machine,
        // so the receiver can subtract it from its own clock for one-way latency.
        const auto now = std::chrono::system_clock::now();
        const long long sendNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();

        char message[256];
        const int length = std::snprintf(message, sizeof(message),
            "seq=%d/%d ts=%lld hello UDP", i, PACKET_COUNT, sendNs);
        if (length < 0 || length >= static_cast<int>(sizeof(message))) return 1;
        const int sent = static_cast<int>(sendto(s, message, length, 0,
            reinterpret_cast<const sockaddr*>(&relay), sizeof(relay)));
        if (sent < 0) return socketError("sendto");
        if (sent != length) return 1;
        std::printf("Sent %d/%d (ts=%lld)\n", i, PACKET_COUNT, sendNs);
        if (i < PACKET_COUNT)
            std::this_thread::sleep_for(std::chrono::milliseconds(INTERVAL_MS));
    }
    std::printf("Finished sending %d packets.\n", PACKET_COUNT);
    return 0;
}

int main() {
    std::setvbuf(stdout, NULL, _IONBF, 0);
    if (!initSockets()) return 1;
    SocketHandle s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int result;
    if (s == INVALID_UDP_SOCKET) {
        result = socketError("socket");
    } else {
        result = sendPackets(s);
        closeSocket(s);
    }
    cleanupSockets();
    return result;
}
