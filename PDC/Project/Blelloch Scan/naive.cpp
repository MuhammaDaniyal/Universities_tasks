// naive.cpp — Sequential Blelloch (exclusive) prefix-sum
// Compile: g++ -O2 -o naive naive.cpp
// Usage  : ./naive [array_size]

#include <iostream>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Blelloch exclusive prefix-sum (in-place, sequential)
// After the call: out[i] = sum of in[0..i-1],  out[0] = 0
// ---------------------------------------------------------------------------
void blelloch_scan_seq(std::vector<long long>& a)
{
    const std::size_t n = a.size();
    if (n <= 1) { if (n == 1) a[0] = 0; return; }

    // --- Upsweep (reduce) phase -------------------------------------------
    // Build a partial-sum tree in-place.
    // After stride d, a[d-1], a[2d-1], a[3d-1] … hold partial sums.
    for (std::size_t stride = 1; stride < n; stride <<= 1) {
        for (std::size_t i = (stride << 1) - 1; i < n; i += stride << 1)
            a[i] += a[i - stride];
    }

    // --- Set identity element at root -------------------------------------
    a[n - 1] = 0;

    // --- Downsweep phase --------------------------------------------------
    for (std::size_t stride = n >> 1; stride >= 1; stride >>= 1) {
        for (std::size_t i = (stride << 1) - 1; i < n; i += stride << 1) {
            long long tmp = a[i - stride];
            a[i - stride] = a[i];
            a[i]         += tmp;
        }
    }
}

int main(int argc, char* argv[])
{
    // Default: 10 million elements
    std::size_t N = (argc > 1) ? static_cast<std::size_t>(std::atoll(argv[1]))
                               : 10'000'000ULL;

    // Edge-case guard
    if (N == 0) { std::cout << "Empty array — nothing to do.\n"; return 0; }

    std::cout << "=== Naive (Sequential) Blelloch Scan ===\n";
    std::cout << "Array size : " << N << "\n";

    // Round up to next power-of-two (Blelloch requires this)
    std::size_t padded = 1;
    while (padded < N) padded <<= 1;

    std::vector<long long> data(padded, 0LL);
    // Fill first N elements with 1; expected last element after scan = N-1
    for (std::size_t i = 0; i < N; ++i) data[i] = 1LL;

    // --- Timed section ----------------------------------------------------
    auto t0 = std::chrono::high_resolution_clock::now();
    blelloch_scan_seq(data);
    auto t1 = std::chrono::high_resolution_clock::now();

    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // --- Results ----------------------------------------------------------
    long long last = data[N - 1];          // should equal N-1 (exclusive scan)
    bool ok = (last == static_cast<long long>(N - 1));

    std::cout << "Device     : CPU (single thread)\n";
    std::cout << "Time       : " << ms << " ms\n";
    std::cout << "data[N-1]  : " << last
              << "  (expected " << N - 1 << ")\n";
    std::cout << "Correct    : " << (ok ? "YES" : "NO") << "\n";

    return ok ? 0 : 1;
}