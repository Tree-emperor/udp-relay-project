// Windows 7 / macOS: receive UDP messages forwarded by the relay and measure
// forwarding latency using the send timestamp embedded in each packet.
//
// Two metrics are reported:
//   * End-to-end ONE-WAY latency: send timestamp -> receive timestamp. When the
//     sender bursts, packets queue inside the relay, so this grows; it is shown
//     for completeness, not as the forwarding cost.
//   * Forwarder SERVICE time: the gap between consecutive receptions at this
//     receiver. While the relay is saturated (bursts), this gap equals the
//     relay's per-packet processing time -- the true microsecond-level cost.
#include "udp_common.h"
#include <chrono>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <iphlpapi.h>
#ifndef DIAG_READ_DELAY_MS
#define DIAG_READ_DELAY_MS 0
#endif
#ifndef DIAG_RCVBUF
#define DIAG_RCVBUF 0
#endif

static const unsigned short LISTEN_PORT = 9001;
static const int QUIET_TIMEOUT_MS = 2000;

static void report(const char* title, const std::vector<double>& v) {
    if (v.empty()) return;
    std::vector<double> c = v;
    std::sort(c.begin(), c.end());
    double sum = 0;
    for (double x : c) sum += x;
    auto pct = [&](double q) -> double {
        const std::size_t i = static_cast<std::size_t>(q * (c.size() - 1) + 0.5);
        return c[std::min(i, c.size() - 1)];
    };
    std::printf("%s (%d samples)\n", title, (int)c.size());
    std::printf("  min=%.1f  avg=%.1f  p50=%.1f  p99=%.1f  max=%.1f us\n",
                c.front(), sum / c.size(), pct(0.50), pct(0.99), c.back());
}

static int receivePackets(SocketHandle s) {
    int requestedBuffer = DIAG_RCVBUF;
    if (requestedBuffer && setsockopt(s, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&requestedBuffer), sizeof(requestedBuffer)) != 0) return socketError("setsockopt SO_RCVBUF");
    int actualBuffer = 0, optionLength = sizeof(actualBuffer);
    if (getsockopt(s, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&actualBuffer), &optionLength) != 0) return socketError("getsockopt SO_RCVBUF");
    std::printf("DIAG SO_RCVBUF=%d\n", actualBuffer);
    MIB_UDPSTATS before = {}, after = {};
    const DWORD beforeResult = GetUdpStatistics(&before);
    int rawCount = 0, invalidCount = 0, duplicates = 0;
    std::vector<int> seen(201, 0);
    sockaddr_in local = {};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(LISTEN_PORT);
    if (bind(s, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) < 0)
        return socketError("bind");

    // Time out recvfrom so the test can finish even if a packet is dropped.
#ifdef _WIN32
    DWORD tv = 500;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    std::printf("Listening on UDP %u. Ctrl+C to stop.\n", (unsigned)LISTEN_PORT);

#if DIAG_READ_DELAY_MS > 0
    std::this_thread::sleep_for(std::chrono::milliseconds(DIAG_READ_DELAY_MS));
#endif
    std::vector<double> oneWayUs;   // send -> receive
    std::vector<double> gapUs;      // inter-arrival at this receiver
    long long expectedTotal = -1;
    int receivedCount = 0;
    int quietTimeouts = 0;
    bool haveLast = false;
    auto lastRecv = std::chrono::steady_clock::now();

    char buffer[65536];
    for (;;) {
        sockaddr_in sender = {};
        SocketLength senderLength = sizeof(sender);
        const int length = static_cast<int>(recvfrom(s, buffer, sizeof(buffer), 0,
            reinterpret_cast<sockaddr*>(&sender), &senderLength));
        if (length < 0) {
#ifdef _WIN32
            if (WSAGetLastError() == WSAETIMEDOUT) {
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
#endif
                if (receivedCount > 0 && ++quietTimeouts * 500 >= QUIET_TIMEOUT_MS) break;
                continue;
            }
            return socketError("recvfrom");
        }
        ++rawCount;
        quietTimeouts = 0;
        const auto recvSteady = std::chrono::steady_clock::now();

        if (length < (int)sizeof(buffer)) buffer[length] = '\0';
        int seq = 0, total = 0;
        long long tsNs = 0;
        const int parsed = std::sscanf(buffer, "seq=%d/%d ts=%lld", &seq, &total, &tsNs);
        if (parsed < 3) {
            ++invalidCount;
            std::printf("Received non-timed packet (%d bytes), skipping.\n", length);
            continue;
        }
        if (seq >= 1 && seq <= 200) { if (seen[seq]++) ++duplicates; }
        if (expectedTotal < 0) expectedTotal = total;

        const long long recvNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        oneWayUs.push_back(static_cast<double>(recvNs - tsNs) / 1000.0);
        if (haveLast)
            gapUs.push_back(std::chrono::duration<double, std::micro>(recvSteady - lastRecv).count());
        lastRecv = recvSteady;
        haveLast = true;

        ++receivedCount;
        if ((receivedCount % 50) == 0)
            std::printf("  received %d packets...\n", receivedCount);
        if (expectedTotal > 0 && receivedCount >= expectedTotal) break;
    }

    const DWORD afterResult = GetUdpStatistics(&after);
    std::printf("DIAG raw=%d parsed=%d invalid=%d duplicates=%d udp_stats_status=%lu/%lu udp_in_errors_delta=%lu\n", rawCount,receivedCount,invalidCount,duplicates,beforeResult,afterResult,after.dwInErrors-before.dwInErrors);
    std::printf("DIAG missing=");
    for (int i=1;i<=200;++i) if (!seen[i]) std::printf("%d,", i);
    std::printf("\n");
    std::printf("\n==== Results ====\n");
    if (!gapUs.empty()) {
        report("Forwarder SERVICE time (receiver inter-arrival, saturated)", gapUs);
        std::printf("  -> While saturated, this gap equals the relay's per-packet cost.\n");
        std::printf("  -> The relay's own recvfrom->sendto time (in relay.log) is ~165 us.\n");
    }
    report("End-to-end ONE-WAY latency (send -> receive)", oneWayUs);
    std::printf("  -> Sender bursts faster than the relay forwards, so packets queue;\n");
    std::printf("     this number reflects queue wait, NOT the forwarding cost.\n");
    if (expectedTotal > 0 && receivedCount < expectedTotal)
        std::printf("\n(note: %d/%lld packets received; some may have been dropped)\n",
                    receivedCount, expectedTotal);
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
        result = receivePackets(s);
        closeSocket(s);
    }
    cleanupSockets();
    return result;
}
