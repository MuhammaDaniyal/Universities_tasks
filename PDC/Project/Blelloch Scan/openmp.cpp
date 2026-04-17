// openmp.cpp — Parallel Blelloch (exclusive) prefix-sum using OpenMP
// Compile: g++ -O2 -fopenmp -o openmp openmp.cpp
// Usage  : OMP_NUM_THREADS=8 ./openmp [array_size]

#include <iostream>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <omp.h>

// ---------------------------------------------------------------------------
// Blelloch exclusive prefix-sum — OpenMP parallel version
// ---------------------------------------------------------------------------
void blelloch_scan_omp(std::vector<long long>& a)
{
    const std::size_t n = a.size();
    if (n <= 1) { if (n == 1) a[0] = 0; return; }

    // --- Upsweep (reduce) phase -------------------------------------------
    for (std::size_t stride = 1; stride < n; stride <<= 1) {
        std::size_t step = stride << 1;
        #pragma omp parallel for schedule(static)
        for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(step - 1);
             i < static_cast<std::ptrdiff_t>(n);
             i += static_cast<std::ptrdiff_t>(step))
        {
            a[i] += a[i - static_cast<std::ptrdiff_t>(stride)];
        }
    }                                                                                                       

    // --- Set root to identity ---------------------------------------------
    a[n - 1] = 0;

    // --- Downsweep phase --------------------------------------------------
    for (std::size_t stride = n >> 1; stride >= 1; stride >>= 1) {
        std::size_t step = stride << 1;
        #pragma omp parallel for schedule(static)
        for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(step - 1);
             i < static_cast<std::ptrdiff_t>(n);
             i += static_cast<std::ptrdiff_t>(step))
        {
            long long tmp                                    = a[i - static_cast<std::ptrdiff_t>(stride)];
            a[i - static_cast<std::ptrdiff_t>(stride)]      = a[i];
            a[i]                                            += tmp;
        }
    }
}

int main(int argc, char* argv[])
{
    std::size_t N = (argc > 1) ? static_cast<std::size_t>(std::atoll(argv[1]))
                               : 10'000'000ULL;

    if (N == 0) { std::cout << "Empty array — nothing to do.\n"; return 0; }

    int nthreads = omp_get_max_threads();

    std::cout << "=== OpenMP Blelloch Scan ===\n";
    std::cout << "Array size : " << N << "\n";
    std::cout << "Threads    : " << nthreads << "\n";

    // Round up to next power-of-two
    std::size_t padded = 1;
    while (padded < N) padded <<= 1;

    std::vector<long long> data(padded, 0LL);
    for (std::size_t i = 0; i < N; ++i) data[i] = 1LL;

    // --- Timed section ----------------------------------------------------
    auto t0 = std::chrono::high_resolution_clock::now();
    blelloch_scan_omp(data);
    auto t1 = std::chrono::high_resolution_clock::now();

    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    long long last = data[N - 1];
    bool ok = (last == static_cast<long long>(N - 1));

    std::cout << "Device     : CPU (OpenMP)\n";
    std::cout << "Time       : " << ms << " ms\n";
    std::cout << "data[N-1]  : " << last
              << "  (expected " << N - 1 << ")\n";
    std::cout << "Correct    : " << (ok ? "YES" : "NO") << "\n";

    return ok ? 0 : 1;
}