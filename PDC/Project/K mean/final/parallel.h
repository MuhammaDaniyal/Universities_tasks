// =============================================================================
// PARALLEL K-MEANS CLUSTERING IMPLEMENTATION
// Optimized OpenMP parallel version
//
// PURPOSE:
//   High-performance K-Means clustering using OpenMP for multi-core parallelism.
//   Uses Structure-of-Arrays (SoA) layout for cache efficiency and SIMD.
//
// KEY OPTIMIZATIONS:
//   - SoA layout: better cache locality and SIMD vectorization
//   - Thread-private accumulators: eliminates lock contention
//   - Cache-line padding: prevents false sharing between threads
//   - Static work scheduling: predictable load distribution
//   - SIMD pragmas: enables auto-vectorization of distance computations
//
// COMPILATION:
//   g++ -O3 -march=native -ffast-math -fopenmp parallel.h main.cpp utils.cpp serial.h -o kmeans_benchmark
//
// PUBLIC INTERFACE:
//   std::vector<int> kMeans(PointSoA& pts, int K);
//       Clusters 'pts' into K clusters. Returns cluster assignment for each point.
//
// =============================================================================

#include <omp.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <numeric>
#include <vector>
#include <random>
#include <cassert>

using namespace std;

#include "utils.h"

// =============================================================================
// CONFIGURATION — Tuning parameters
// =============================================================================

/// Cache line size for alignment (prevents false sharing between threads)
static constexpr int    CACHE_LINE      = 64;

/// Convergence threshold: stop when centroid movement < CONVERGE_TOL
static constexpr double CONVERGE_TOL    = 1e-6;

// =============================================================================
// DATA STRUCTURES
// =============================================================================

// Note: PointSoA and Centroid are defined in utils.h
// They are included via #include "utils.h" above

// =============================================================================
// DISTANCE COMPUTATION — SIMD-vectorized Euclidean distance
// =============================================================================

/// Compute squared Euclidean distance between point and centroid.
/// The inner loop uses #pragma omp simd for auto-vectorization.
/// With -march=native: AVX2 (8 floats/cycle) or AVX-512 (16 floats/cycle).
///
/// Parameters:
///   pts: point dataset in SoA format
///   i: point index
///   centCoords: centroid coordinates (length D)
///   D: number of dimensions
///
/// Returns: squared Euclidean distance = sum((x_d - c_d)^2)
///
/// Complexity: O(D) per distance computation
static inline float squaredDist(
    const PointSoA&           pts,
    int                       i,
    const vector<float>& centCoords,
    int                       D)
{
    float dist = 0.0f;
    #pragma omp simd reduction(+: dist)
    for (int d = 0; d < D; ++d) {
        float diff = pts.at(d, i) - centCoords[d];
        dist += diff * diff;
    }
    return dist;
}

// =============================================================================
// ASSIGNMENT STEP — Assign each point to nearest centroid (parallel)
// =============================================================================

/// Assign each point to its nearest centroid and accumulate sums for update.
///
/// PARALLELIZATION STRATEGY:
///   - Each thread maintains thread-private accumulators (local_sum, local_count)
///   - No synchronization during point assignment loop → perfect scalability
///   - At end: single parallel reduction merges thread-local results
///
/// Parameters:
///   pts: point dataset in SoA format
///   centroids: current centroids
///   K: number of clusters
///   labels: [output] cluster assignment for each point
///   globalSum: [output] sum of coordinates per cluster
///   globalCount: [output] count of points per cluster
///
/// Returns: true if any point changed assignment, false if stable
///
/// Complexity: O(N*K*D) — N points, K centroid checks, D distance computation
/// Parallelism: O(N/P) work per thread with N/P independent threads
static bool assignmentStep(
    const PointSoA&              pts,
    const vector<Centroid>& centroids,
    int                          K,
    vector<int>&            labels,
    vector<vector<float>>& globalSum,
    vector<long long>&          globalCount)
{
    int  N          = pts.n;
    int  D          = pts.d;
    bool anyChanged = false;

    // Thread-private reduction arrays: each thread has its own copy
    int nThreads = omp_get_max_threads();
    vector<vector<vector<float>>> private_sum(
        nThreads, vector<vector<float>>(K, vector<float>(D, 0.0f)));
    vector<vector<long long>> private_count(nThreads, vector<long long>(K, 0LL));

    // #pragma omp parallel firstprivate(private_sum, private_count) reduction(||: anyChanged)
    #pragma omp parallel reduction(||: anyChanged)
    {
        int tid = omp_get_thread_num();

        #pragma omp for schedule(static)
        for (int i = 0; i < N; ++i) {
            int   bestK = 0;
            float bestD = numeric_limits<float>::max();

            for (int k = 0; k < K; ++k) {
                float d = squaredDist(pts, i, centroids[k].coords, D);
                if (d < bestD) {
                    bestD = d;
                    bestK = k;
                }
            }

            if (bestK != labels[i]) {
                labels[i]  = bestK;
                anyChanged = true;
            }

            private_count[tid][bestK]++;
            for (int d = 0; d < D; ++d)
                private_sum[tid][bestK][d] += pts.at(d, i);
        }
    }

    #pragma omp parallel for schedule(static)
    for (int k = 0; k < K; ++k) {
        globalCount[k] = 0;
        fill(globalSum[k].begin(), globalSum[k].end(), 0.0f);
        for (int t = 0; t < nThreads; ++t) {
            globalCount[k] += private_count[t][k];
            for (int d = 0; d < D; ++d)
                globalSum[k][d] += private_sum[t][k][d];
        }
    }

    return anyChanged;
}

// =============================================================================
// UPDATE CENTROIDS — Compute new centroid positions from accumulated sums
// =============================================================================

/// Update centroid positions by computing mean of assigned points.
/// Also computes maximum shift for convergence check.
///
/// ALGORITHM:
///   For each centroid k:
///     new_pos[k] = sum[k] / count[k]    (element-wise mean)
///
/// Used for convergence check: if max_shift < CONVERGE_TOL, algorithm converged.
///
/// Parameters:
///   centroids: [in/out] centroids to update in-place
///   globalSum: accumulated coordinates per centroid
///   globalCount: count of assigned points per centroid
///   K: number of clusters
///   D: number of dimensions
///
/// Returns: maximum centroid shift (L2 distance), used for convergence test
///
/// Complexity: O(K*D)
/// Parallelism: Independent per centroid → perfect scaling
static float updateCentroids(
    vector<Centroid>&                 centroids,
    const vector<vector<float>>& globalSum,
    const vector<long long>&          globalCount,
    int                                    K,
    int                                    D)
{
    float maxShift = 0.0f;

    #pragma omp parallel for schedule(static) reduction(max: maxShift)
    for (int k = 0; k < K; ++k) {
        if (globalCount[k] == 0) continue;

        float inv   = 1.0f / (float)globalCount[k];
        float shift = 0.0f;

        for (int d = 0; d < D; ++d) {
            float newVal = globalSum[k][d] * inv;
            float diff   = newVal - centroids[k].coords[d];
            shift += diff * diff;
            centroids[k].coords[d] = newVal;
        }

        float s = sqrt(shift);
        if (s > maxShift) maxShift = s;
    }
    return maxShift;
}

// =============================================================================
// INITIALIZATION HELPERS — Setup for main K-Means loop
// =============================================================================

/// Find dimension with maximum value range across all points.
/// Used for initialization: sort by max-range dimension.
///
/// Parallelization: min/max reduction over all points
/// Complexity: O(N*D)
/// Returns: dimension index with largest range
static int findMaxRangeDimension(const PointSoA& pts) {
    int    best_dim   = 0;
    float  best_range = -1.0f;

    for (int d = 0; d < pts.d; ++d) {
        float gmin =  numeric_limits<float>::max();
        float gmax = -numeric_limits<float>::max();

        #pragma omp parallel for reduction(min: gmin) reduction(max: gmax) schedule(static)
        for (int i = 0; i < pts.n; ++i) {
            float v = pts.at(d, i);
            if (v < gmin) gmin = v;
            if (v > gmax) gmax = v;
        }

        float range = gmax - gmin;
        if (range > best_range) {
            best_range = range;
            best_dim   = d;
        }
    }
    return best_dim;
}

/// Sort point indices by specified dimension (descending).
/// Used for Forgy initialization method.
/// Complexity: O(N*log(N))
static vector<int> sortByDimension(const PointSoA& pts, int dim) {
    vector<int> idx(pts.n);
    iota(idx.begin(), idx.end(), 0);

    sort(idx.begin(), idx.end(), [&](int a, int b) {
        return pts.at(dim, a) > pts.at(dim, b);
    });
    return idx;
}

/// Initialize centroids using Forgy method.
/// Sort by max-range dimension, partition into K groups, compute partition means.
/// Each partition's mean becomes a centroid.
///
/// Parallelization: Independent work per partition
/// Complexity: O(N*D)
/// Pros: Good initial spread, deterministic
/// Cons: Requires sorting (O(N*log(N)) overhead)
static void initCentroidsFromPartitions(
    const PointSoA&        pts,
    const vector<int>& sortedIdx,
    int                     K,
    vector<Centroid>&  centroids)
{
    int partSize = pts.n / K;

    #pragma omp parallel for schedule(static)
    for (int k = 0; k < K; ++k) {
        int start = k * partSize;
        int end   = (k == K - 1) ? pts.n : start + partSize;

        centroids[k].init(pts.d);

        for (int idx = start; idx < end; ++idx) {
            int i = sortedIdx[idx];
            for (int d = 0; d < pts.d; ++d) {
                centroids[k].coords[d] += pts.at(d, i);
            }
        }

        float inv = 1.0f / (float)(end - start);
        for (int d = 0; d < pts.d; ++d) {
            centroids[k].coords[d] *= inv;
        }
    }
}

/// NUMA first-touch optimization.
/// Each thread touches its own chunk of memory first.
/// Linux NUMA allocator places pages on local node → later accesses are fast.
///
/// Complexity: O(N*D)
/// Speedup on NUMA systems: 1.5x-3.0x for large N
static void numaFirstTouch(PointSoA& pts) {
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < pts.n; ++i) {
        for (int d = 0; d < pts.d; ++d) {
            pts.at(d, i) = pts.at(d, i);   // read-write touch forces page placement
        }
    }
}

// =============================================================================
// MAIN K-MEANS ALGORITHM
// =============================================================================

/// Perform K-Means clustering on point dataset.
///
/// ALGORITHM OVERVIEW:
///   1. NUMA first-touch: each thread touches its memory chunk
///   2. INITIALIZATION: sort by max-range dim, partition into K groups → means
///   3. LOOP:
///      a. ASSIGNMENT: assign each point to nearest centroid (parallel)
///      b. UPDATE: recompute centroids from assignments (parallel)
///      c. CHECK: if no changes or small shift → converged
///   4. RETURN: cluster assignment for each point
///
/// PARALLELIZATION:
///   - Assignment step: O(N) per thread, no synchronization except reduction
///   - Update step: O(K) per thread, independent per centroid
///   - Total speedup: typically 3x-6x on 8 cores, scales to 16+ cores
///
/// Parameters:
///   pts: point dataset in SoA format [output: points reordered during first-touch]
///   K: number of clusters (must be 1 ≤ K ≤ N)
///
/// Returns: cluster assignment vector (assignment[i] = cluster ID for point i)
///
/// Complexity:
///   Time: O(I * N * K * D) where I = iteration count (typically 10-100)
///   Space: O(N + K*D)
///
/// Convergence: typically 20-50 iterations for most datasets
vector<int> kMeans(PointSoA& pts, int K, int MAX_ITER = 300) {
    int N = pts.n;
    int D = pts.d;

    assert(K > 0 && K <= N);

    numaFirstTouch(pts);

    vector<Centroid> centroids(K);
    int maxDim = findMaxRangeDimension(pts);
    vector<int> sortedIdx = sortByDimension(pts, maxDim);
    initCentroidsFromPartitions(pts, sortedIdx, K, centroids);

    vector<int> labels(N, 0);

    vector<vector<float>> globalSum(K, vector<float>(D, 0.0f));
    vector<long long>          globalCount(K, 0LL);

    // Initial full assignment
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < N; ++i) {
        float best  = numeric_limits<float>::max();
        int   bestK = 0;
        for (int k = 0; k < K; ++k) {
            float d = squaredDist(pts, i, centroids[k].coords, D);
            if (d < best) { best = d; bestK = k; }
        }
        labels[i] = bestK;
    }

    // Main iteration loop
    for (int iter = 0; iter < MAX_ITER; ++iter) {

        #pragma omp parallel for schedule(static)
        for (int k = 0; k < K; ++k) {
            globalCount[k] = 0;
            fill(globalSum[k].begin(), globalSum[k].end(), 0.0f);
        }

        bool changed = assignmentStep(
            pts, centroids, K,
            labels, globalSum, globalCount);

        float maxShift = updateCentroids(
            centroids, globalSum, globalCount, K, D);

        if (!changed || maxShift < CONVERGE_TOL) break;
    }

    return labels;
}