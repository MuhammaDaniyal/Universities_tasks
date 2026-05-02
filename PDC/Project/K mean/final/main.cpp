// =============================================================================
// K-MEANS BENCHMARKING FRAMEWORK
// Compares serial vs parallel implementations across different parameters
// =============================================================================

#include <chrono>
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>

#include "utils.h"
#include "parallel.h"
#include "serial.h"

// =============================================================================
// CONFIGURATION - Select initialization technique
// =============================================================================

// Uncomment ONE of the following to select which initialization to use
#define USE_INIT_RANDOM        1
// #define USE_INIT_KMEANS_PP     2
// #define USE_INIT_FORGY         3

// =============================================================================
// BENCHMARK RESULTS STRUCTURE
// =============================================================================

/// Holds timing and performance metrics for one benchmark run
struct BenchmarkResult {
    int N;                          // number of points
    int D;                          // number of dimensions
    int K;                          // number of clusters
    double serial_ms;               // serial execution time (ms)
    double parallel_ms;             // parallel execution time (ms)
    double speedup;                 // parallel_ms / serial_ms
    double efficiency;              // speedup / num_threads (%)

    void print() const {
        std::cout << std::setw(8) << N
                  << std::setw(5) << D
                  << std::setw(5) << K
                  << std::setw(12) << std::fixed << std::setprecision(2) << serial_ms
                  << std::setw(12) << std::fixed << std::setprecision(2) << parallel_ms
                  << std::setw(10) << std::fixed << std::setprecision(2) << speedup
                  << std::setw(12) << std::fixed << std::setprecision(1) << efficiency * 100
                  << "%\n";
    }
};

// =============================================================================
// BENCHMARK FUNCTION
// =============================================================================

/// Run a single benchmark comparing serial vs parallel K-Means
/// Parameters:
///   N: number of points
///   D: number of dimensions
///   K: number of clusters
///   init_type: which initialization technique to use (1=Random, 2=KMeans++, 3=Forgy)
/// Returns: BenchmarkResult with timing and speedup metrics
BenchmarkResult runBenchmark(int N, int D, int K, int init_type) {
    BenchmarkResult result{N, D, K, 0, 0, 0, 0};

    // Generate shared dataset
    SharedPoints shared = initializePoints(N, D, 42, 0.0, 100.0);

    // Convert to parallel format
    PointSoA parallelPoints = makePointSoA(shared);

    // Convert to serial format
    std::vector<Point> serialPoints = makePoints(shared);

    // =========================================================================
    // PARALLEL VERSION
    // =========================================================================
    auto parallelStart = std::chrono::steady_clock::now();
    std::vector<int> parallelLabels = kMeans(parallelPoints, K, 300);
    auto parallelEnd = std::chrono::steady_clock::now();
    result.parallel_ms = std::chrono::duration<double, std::milli>(
        parallelEnd - parallelStart).count();

    // =========================================================================
    // SERIAL VERSION
    // =========================================================================
    KMeans serialKMeans(K, 300);  // max 300 iterations
    auto serialStart = std::chrono::steady_clock::now();
    double serialReportedMs = serialKMeans.fit(serialPoints);
    auto serialEnd = std::chrono::steady_clock::now();
    result.serial_ms = std::chrono::duration<double, std::milli>(
        serialEnd - serialStart).count();

    // =========================================================================
    // COMPUTE METRICS
    // =========================================================================
    result.speedup = result.serial_ms / result.parallel_ms;
    
    // Get number of threads (assuming OpenMP is used)
    int num_threads = 1;
    #ifdef _OPENMP
    #pragma omp parallel
    {
        #pragma omp single
        num_threads = omp_get_num_threads();
    }
    #endif
    
    result.efficiency = (result.speedup / num_threads) * 100.0;

    return result;
}

// =============================================================================
// PRINT HEADER
// =============================================================================

/// Print formatted header for benchmark results table
void printHeader() {
    std::cout << "\n";
    std::cout << "=============================================================================\n";
    std::cout << "  K-MEANS BENCHMARK RESULTS\n";
    std::cout << "=============================================================================\n";
    std::cout << std::setw(8) << "Points"
              << std::setw(5) << "Dims"
              << std::setw(5) << "K"
              << std::setw(12) << "Serial(ms)"
              << std::setw(12) << "Parallel(ms)"
              << std::setw(10) << "Speedup"
              << std::setw(12) << "Efficiency\n";
    std::cout << std::setfill('-');
    std::cout << std::setw(73) << "-" << "\n";
    std::cout << std::setfill(' ');
}

// =============================================================================
// MAIN BENCHMARK SUITE
// =============================================================================

int main() {
    // Select dimensions and clusters to test
    int D = 2;  // dimensions (fixed for clarity)

    // Test different problem sizes
    std::vector<int> N_values = {100'000, 500'000, 1'000'000};
    std::vector<int> K_values = {5, 16, 32};

    // Store all results
    std::vector<BenchmarkResult> results;

    std::cout << "\n╔════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║          K-MEANS PARALLEL vs SERIAL BENCHMARK SUITE                        ║\n";
    std::cout << "║                                                                            ║\n";
    
    #ifdef USE_INIT_RANDOM
    std::cout << "║  Initialization: RANDOM (pick K random points)                            ║\n";
    #elif defined(USE_INIT_KMEANS_PP)
    std::cout << "║  Initialization: K-MEANS++ (probabilistic selection)                      ║\n";
    #elif defined(USE_INIT_FORGY)
    std::cout << "║  Initialization: FORGY (max-range partitioning)                           ║\n";
    #endif
    
    std::cout << "║                                                                            ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════════════════╝\n";

    printHeader();

    // =========================================================================
    // RUN BENCHMARK SUITE
    // =========================================================================
    
    for (int N : N_values) {
        for (int K : K_values) {
            BenchmarkResult result = runBenchmark(N, D, K, 1);
            result.print();
            results.push_back(result);
        }
        std::cout << std::setfill('-');
        std::cout << std::setw(73) << "-" << "\n";
        std::cout << std::setfill(' ');
    }

    // =========================================================================
    // SUMMARY STATISTICS
    // =========================================================================

    if (!results.empty()) {
        double avg_speedup = 0;
        double min_speedup = results[0].speedup;
        double max_speedup = results[0].speedup;

        for (const auto& r : results) {
            avg_speedup += r.speedup;
            min_speedup = std::min(min_speedup, r.speedup);
            max_speedup = std::max(max_speedup, r.speedup);
        }
        avg_speedup /= results.size();

        std::cout << "\n";
        std::cout << "═════════════════════════════════════════════════════════════════════════════\n";
        std::cout << "  SUMMARY STATISTICS\n";
        std::cout << "═════════════════════════════════════════════════════════════════════════════\n";
        std::cout << "  Average Speedup:     " << std::fixed << std::setprecision(2) << avg_speedup << "x\n";
        std::cout << "  Min Speedup:         " << std::fixed << std::setprecision(2) << min_speedup << "x\n";
        std::cout << "  Max Speedup:         " << std::fixed << std::setprecision(2) << max_speedup << "x\n";
        std::cout << "  Benchmark Runs:      " << results.size() << "\n";
        std::cout << "═════════════════════════════════════════════════════════════════════════════\n\n";

        // Interpretation
        std::cout << "  INTERPRETATION:\n";
        if (avg_speedup >= 3.5) {
            std::cout << "  ✓ Excellent parallel scaling for this hardware\n";
        } else if (avg_speedup >= 2.0) {
            std::cout << "  ✓ Good parallel scaling; parallelization is beneficial\n";
        } else if (avg_speedup >= 1.5) {
            std::cout << "  ~ Moderate speedup; overhead reduces efficiency\n";
        } else {
            std::cout << "  ✗ Limited speedup; consider serial version or optimization\n";
        }
        std::cout << "\n";
    }

    return 0;
}

// =============================================================================
// NOTES ON INITIALIZATION VARIANTS
// =============================================================================

/*
 * THREE INITIALIZATION STRATEGIES:
 *
 * 1. RANDOM (USE_INIT_RANDOM):
 *    - Pick K distinct random points from dataset
 *    - Complexity: O(K)
 *    - Pros: Very fast initialization
 *    - Cons: Poor initial spread, high variance in convergence
 *    - Best for: Quick prototyping, already near-optimal clusters
 *
 * 2. K-MEANS++ (USE_INIT_KMEANS_PP):
 *    - Probabilistic: pick first centroid randomly, then each subsequent
 *      centroid with probability proportional to squared distance
 *    - Complexity: O(K*N*D)
 *    - Pros: O(log K) approximation of optimal clustering guaranteed
 *    - Cons: Slower initialization, sequential (hard to parallelize)
 *    - Best for: Production systems where convergence speed matters
 *
 * 3. FORGY / MAX-RANGE (USE_INIT_FORGY):
 *    - Sort by max-range dimension, partition into K groups
 *    - Complexity: O(N*log(N) + N*D)
 *    - Pros: Good spread, deterministic, parallel-friendly
 *    - Cons: Still slower than random
 *    - Best for: Balanced initialization with minimal randomness
 *
 * RECOMMENDATION:
 * - For initial testing and benchmarking: use RANDOM for speed
 * - For production clustering: use K-MEANS++ for quality
 * - For large-scale systems: use FORGY for parallelism
 */
 

