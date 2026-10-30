#include <windows.h>
#include <chrono>
#include <cstdio>
#include <vector>
#include <algorithm>

int main() {
    LARGE_INTEGER frequency;
    if (!QueryPerformanceFrequency(&frequency)) return 1;
    std::vector<double> pairs;
    int steadyZero = 0;
    long long smallestSteadyNs = 0;
    for (int i = 0; i < 100000; ++i) {
        LARGE_INTEGER a, b;
        QueryPerformanceCounter(&a);
        QueryPerformanceCounter(&b);
        pairs.push_back((b.QuadPart - a.QuadPart) * 1e6 / frequency.QuadPart);
        const auto s = std::chrono::steady_clock::now();
        const auto t = std::chrono::steady_clock::now();
        const auto delta = std::chrono::duration_cast<std::chrono::nanoseconds>(t-s).count();
        if (delta == 0) ++steadyZero;
        else if (delta > 0 && (!smallestSteadyNs || delta < smallestSteadyNs)) smallestSteadyNs = delta;
    }
    std::sort(pairs.begin(), pairs.end());
    std::printf("QPC_frequency_Hz=%lld\nQPC_tick_us=%.3f\n", frequency.QuadPart, 1e6/frequency.QuadPart);
    std::printf("QPC_adjacent_pair_samples=100000 min_us=%.3f p50_us=%.3f p99_us=%.3f max_us=%.3f\n",
                pairs.front(),pairs[49999],pairs[98999],pairs.back());
    std::printf("steady_clock_adjacent_pair_samples=100000 zero_deltas=%d smallest_positive_delta_ns=%lld\n", steadyZero, smallestSteadyNs);
    return 0;
}
