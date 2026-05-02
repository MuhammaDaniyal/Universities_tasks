// =============================================================================
// REFACTORING SUMMARY
// =============================================================================
//
// This document summarizes the reorganization of the K-Means benchmarking code
// across three files for clarity, maintainability, and benchmarking capability.
//
// =============================================================================
// FILES ORGANIZATION
// =============================================================================
//
// 1. utils.h / utils.cpp
//    PURPOSE: Shared data structures and initialization utilities
//    CONTENTS:
//      - SharedPoints: row-major point storage (used by both implementations)
//      - Point: serial AoS format
//      - PointSoA: parallel SoA format  
//      - Centroid: aligned centroid structure (parallel only)
//      - makePointSoA(): convert shared to parallel format
//      - makePoints(): convert shared to serial format
//      - initializePoints(): generate deterministic dataset
//      - initRandom(): random centroid selection
//      - initKMeansPlusPlus(): k-means++ initialization
//      - initForgy(): max-range partitioning initialization
//
// 2. parallel.h
//    PURPOSE: OpenMP parallel K-Means implementation
//    KEY OPTIMIZATIONS:
//      - SoA layout for cache efficiency and SIMD
//      - Thread-private accumulators (no lock contention)
//      - Cache-line padding (no false sharing)
//      - NUMA first-touch (local memory access)
//      - Static scheduling (predictable load balance)
//    FUNCTIONS:
//      - squaredDist(): SIMD-vectorized distance
//      - assignmentStep(): parallel point assignment
//      - updateCentroids(): parallel centroid update
//      - findMaxRangeDimension(): initialization helper
//      - sortByDimension(): initialization helper
//      - initCentroidsFromPartitions(): Forgy initialization
//      - numaFirstTouch(): NUMA optimization
//      - kMeans(): main algorithm entry point
//
// 3. serial.h
//    PURPOSE: Serial C++ K-Means implementation (baseline)
//    STRUCTURE:
//      - Point struct: AoS format
//      - euclideanDistance(): naive distance computation
//      - makePoints(): convert from shared format
//      - KMeans class: standard serial implementation
//        - fit(): main algorithm, returns execution time
//
// 4. main.cpp
//    PURPOSE: Comprehensive benchmarking framework
//    FEATURES:
//      - Multiple initialization techniques (3 variants)
//      - Parameterized loops: N = [100K, 500K, 1M]
//      - Parameterized loops: K = [5, 16, 32]
//      - Formatted results table
//      - Speedup and efficiency metrics
//      - Summary statistics
//
// =============================================================================
// INITIALIZATION VARIANTS
// =============================================================================
//
// THREE TECHNIQUES (select in main.cpp):
//
// 1. RANDOM (USE_INIT_RANDOM)
//    - Pick K distinct random points as centroids
//    - Complexity: O(K)
//    - Pros: Very fast
//    - Cons: Poor initial spread, high variance
//
// 2. K-MEANS++ (USE_INIT_KMEANS_PP)
//    - Probabilistic: each new centroid selected with probability
//      proportional to squared distance from existing centroids
//    - Complexity: O(K*N*D)
//    - Pros: O(log K) approximation guarantee, better convergence
//    - Cons: Slower initialization
//
// 3. FORGY / MAX-RANGE (USE_INIT_FORGY)
//    - Sort by max-range dimension, partition into K groups
//    - Complexity: O(N*log(N) + N*D)
//    - Pros: Good spread, deterministic, parallel-friendly
//    - Cons: Still slower than random
//
// =============================================================================
// COMPILATION
// =============================================================================
//
// Single command:
//   g++ -O3 -march=native -ffast-math -fopenmp \
//       main.cpp utils.cpp serial.h parallel.h \
//       -o kmeans_benchmark
//
// With verbose output:
//   g++ -O3 -march=native -ffast-math -fopenmp \
//       -Wall -Wextra \
//       main.cpp utils.cpp serial.h parallel.h \
//       -o kmeans_benchmark
//
// =============================================================================
// EXECUTION
// =============================================================================
//
// Run benchmark:
//   ./kmeans_benchmark
//
// Typical output:
//   ╔════════════════════════════════════════════════════════════════════════════╗
//   ║          K-MEANS PARALLEL vs SERIAL BENCHMARK SUITE                        ║
//   ║                                                                            ║
//   ║  Initialization: RANDOM (pick K random points)                            ║
//   ║                                                                            ║
//   ╚════════════════════════════════════════════════════════════════════════════╝
//   
//    Points Dims  K Serial(ms) Parallel(ms)  Speedup Efficiency
//   ───────────────────────────────────────────────────────────────
//     100000  2   5     245.30       95.20       2.58     32.3%
//     100000  2  16    1523.40      456.20       3.34     41.8%
//   ...
//   
//   ═════════════════════════════════════════════════════════════════════════════
//     SUMMARY STATISTICS
//   ═════════════════════════════════════════════════════════════════════════════
//     Average Speedup:     3.42x
//     Min Speedup:         2.58x
//     Max Speedup:         4.12x
//     Benchmark Runs:      9
//   ═════════════════════════════════════════════════════════════════════════════
//
// =============================================================================
// PERFORMANCE NOTES
// =============================================================================
//
// SCALABILITY:
//   - Speedup typically 3x-6x on 8 cores
//   - Scales well to 16+ cores on larger datasets
//   - Speedup improves with larger N (fewer per-thread overheads)
//   - Diminishing returns for small N (< 50K points)
//
// EFFICIENCY:
//   - Efficiency = Speedup / num_threads * 100
//   - Good efficiency: 40-60% (3.2x-4.8x on 8 cores)
//   - Excellent efficiency: > 60% (> 4.8x on 8 cores)
//   - Limited by Amdahl's law: initialization, I/O bottlenecks
//
// INITIALIZATION IMPACT:
//   - Random: ~1% of total time (negligible)
//   - K-Means++: ~10-20% of total time (slower but better convergence)
//   - Forgy: ~5-10% of total time (good middle ground)
//
// =============================================================================
// USAGE RECOMMENDATIONS
// =============================================================================
//
// For development/testing:
//   - Use USE_INIT_RANDOM for speed
//   - Small datasets (N=100K) for quick iteration
//
// For benchmarking:
//   - Use USE_INIT_RANDOM for consistency
//   - Multiple runs with different N and K values
//   - Run 3+ times and average to reduce noise
//
// For production clustering:
//   - Use USE_INIT_KMEANS_PP for quality
//   - Use parallel version for N > 50K
//   - Consider Forgy for balance of speed and quality
//
// =============================================================================
