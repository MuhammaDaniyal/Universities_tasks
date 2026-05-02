// =============================================================================
// K-Means Clustering — Optimized OpenMP Implementation
// Compile: g++ -O3 -march=native -ffast-math -fopenmp kmeans_openmp.cpp -o kmeans
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

#include "utils.h"

// =============================================================================
// CONFIGURATION
// =============================================================================

static constexpr int    CACHE_LINE      = 64;   // bytes — for centroid padding
static constexpr int    MAX_ITER        = 300;   // maximum K-Means iterations
static constexpr double CONVERGE_TOL    = 1e-6;  // convergence threshold (centroid shift)
static constexpr int    ELKAN_UPDATE    = 5;     // recompute inter-centroid distances every N iters

// =============================================================================
// PHASE 0 — DATA STRUCTURES
// =============================================================================

// Structure of Arrays (SoA) for points.
// All x-coordinates contiguous, then all y-coords, etc.
// Enables SIMD to load a full register of one dimension at a time.
struct PointSoA {
    int    n;   // number of points
    int    d;   // number of dimensions
    // data[dim * n + i] = coordinate of point i along dimension dim
    std::vector<float> data;

    PointSoA(int n, int d) : n(n), d(d), data(n * d) {}

    inline float& at(int dim, int i)       { return data[dim * n + i]; }
    inline float  at(int dim, int i) const { return data[dim * n + i]; }
};

// Padded centroid struct — each centroid occupies a full cache line (or multiple).
// Prevents false sharing when threads update different centroids in parallel.
struct alignas(CACHE_LINE) Centroid {
    std::vector<float> coords;   // length = D
    std::vector<float> sum;      // accumulator for new mean
    long long          count;    // number of points assigned

    void init(int d) {
        coords.assign(d, 0.0f);
        sum.assign(d, 0.0f);
        count = 0;
    }

    void resetAccum() {
        std::fill(sum.begin(), sum.end(), 0.0f);
        count = 0;
    }
};

static PointSoA makePointSoA(const SharedPoints& shared) {
    PointSoA pts(shared.n, shared.d);

    for (int point = 0; point < shared.n; ++point) {
        for (int dim = 0; dim < shared.d; ++dim) {
            pts.at(dim, point) = static_cast<float>(shared.at(point, dim));
        }
    }

    return pts;
}

// =============================================================================
// PHASE 1 — INITIALIZATION (Paper Algorithm: Max-Range Sort + Partition Means)
// =============================================================================

// Step 1a: Find the dimension with the maximum value range across all points.
// Parallelized with per-thread min/max reduction.
static int findMaxRangeDimension(const PointSoA& pts) {
    int    best_dim   = 0;
    float  best_range = -1.0f;

    for (int d = 0; d < pts.d; ++d) {
        float gmin =  std::numeric_limits<float>::max();
        float gmax = -std::numeric_limits<float>::max();

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

// Step 1b: Sort point indices in descending order by the chosen dimension.
// Uses std::sort — swap to __gnu_parallel::sort for parallel sort if available.
static std::vector<int> sortByDimension(const PointSoA& pts, int dim) {
    std::vector<int> idx(pts.n);
    std::iota(idx.begin(), idx.end(), 0);

    std::sort(idx.begin(), idx.end(), [&](int a, int b) {
        return pts.at(dim, a) > pts.at(dim, b);   // anti-decreasing (descending)
    });
    return idx;
}

// Step 1c: Divide sorted indices into K equal partitions.
// Compute arithmetic mean of each partition as the initial centroid.
// Each partition mean is independent — fully parallel over K.
static void initCentroidsFromPartitions(
    const PointSoA&        pts,
    const std::vector<int>& sortedIdx,
    int                     K,
    std::vector<Centroid>&  centroids)
{
    int partSize = pts.n / K;

    #pragma omp parallel for schedule(static)
    for (int k = 0; k < K; ++k) {
        int start = k * partSize;
        int end   = (k == K - 1) ? pts.n : start + partSize;  // last partition absorbs remainder

        centroids[k].init(pts.d);

        // Sum all points in this partition
        for (int idx = start; idx < end; ++idx) {
            int i = sortedIdx[idx];
            for (int d = 0; d < pts.d; ++d) {
                centroids[k].coords[d] += pts.at(d, i);
            }
        }

        // Divide to get mean
        float inv = 1.0f / (float)(end - start);
        for (int d = 0; d < pts.d; ++d) {
            centroids[k].coords[d] *= inv;
        }
    }
}

// =============================================================================
// PHASE 2 — DISTANCE COMPUTATION
// =============================================================================

// Squared Euclidean distance between point i and a centroid.
// #pragma omp simd enables auto-vectorization of the inner dimension loop.
// With -march=native this emits AVX2 (8 floats/cycle) or AVX-512 (16 floats/cycle).
static inline float squaredDist(
    const PointSoA&       pts,
    int                   i,
    const std::vector<float>& centCoords,
    int                   D)
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
// PHASE 3 — ELKAN'S TRIANGLE INEQUALITY PRUNING
// =============================================================================
// Maintains per-point upper bound u[i] and per-point-per-centroid lower bounds l[i][k].
// Uses: dist(x, c_j) >= max(0, l[i][j])  and  dist(x, assigned) <= u[i]
// If u[i] <= 0.5 * min_{j != assigned} d(c_assigned, c_j), skip point entirely.
// Reduces distance computations by 50-90% on well-separated clusters.

// Compute full K x K inter-centroid distance matrix (upper triangle only).
static void computeInterCentroidDists(
    const std::vector<Centroid>& centroids,
    int                          K,
    std::vector<float>&          dCC)       // dCC[k1 * K + k2] = dist(c_k1, c_k2)
{
    dCC.assign(K * K, 0.0f);

    #pragma omp parallel for schedule(static) collapse(2)
    for (int k1 = 0; k1 < K; ++k1) {
        for (int k2 = 0; k2 < K; ++k2) {
            if (k1 >= k2) continue;
            float dist = 0.0f;
            int   D    = (int)centroids[0].coords.size();
            #pragma omp simd reduction(+: dist)
            for (int d = 0; d < D; ++d) {
                float diff = centroids[k1].coords[d] - centroids[k2].coords[d];
                dist += diff * diff;
            }
            dist = std::sqrt(dist);
            dCC[k1 * K + k2] = dist;
            dCC[k2 * K + k1] = dist;
        }
    }
}

// For each centroid k, compute s[k] = 0.5 * min distance to any other centroid.
// A point assigned to k can be skipped if u[i] <= s[k].
static void computeHalfMinDist(
    const std::vector<float>& dCC,
    int                       K,
    std::vector<float>&       s)
{
    s.resize(K);
    #pragma omp parallel for schedule(static)
    for (int k = 0; k < K; ++k) {
        float minD = std::numeric_limits<float>::max();
        for (int j = 0; j < K; ++j) {
            if (j != k && dCC[k * K + j] < minD)
                minD = dCC[k * K + j];
        }
        s[k] = 0.5f * minD;
    }
}

// =============================================================================
// PHASE 4 — ASSIGNMENT STEP (with Elkan pruning)
// =============================================================================
// Each thread handles its own chunk of points.
// Thread-private accumulators (local_sum, local_count) eliminate all critical sections.
// At the end, a single parallel reduction merges thread-local results.

static bool assignmentStep(
    const PointSoA&              pts,
    const std::vector<Centroid>& centroids,
    const std::vector<float>&    dCC,
    const std::vector<float>&    s,
    int                          K,
    std::vector<int>&            labels,
    std::vector<float>&          upperBound,   // per-point upper bound (Elkan)
    std::vector<std::vector<float>>& lowerBound, // per-point per-centroid lower bound
    // Output accumulators (indexed by centroid)
    std::vector<std::vector<float>>& globalSum,
    std::vector<long long>&          globalCount,
    bool                             useElkan)
{
    int  N          = pts.n;
    int  D          = pts.d;
    bool anyChanged = false;

    // -------------------------------------------------------------------------
    // Thread-private reduction arrays
    // Each thread has its own copy of sum[K][D] and count[K].
    // No synchronization needed during the loop — merge at the end.
    // -------------------------------------------------------------------------
    int nThreads = omp_get_max_threads();
    // private_sum[t][k][d], private_count[t][k]
    std::vector<std::vector<std::vector<float>>> private_sum(
        nThreads, std::vector<std::vector<float>>(K, std::vector<float>(D, 0.0f)));
    std::vector<std::vector<long long>> private_count(
        nThreads, std::vector<long long>(K, 0LL));

    #pragma omp parallel reduction(||: anyChanged)
    {
        int tid = omp_get_thread_num();

        #pragma omp for schedule(static)
        for (int i = 0; i < N; ++i) {

            int   assigned = labels[i];
            float uBound   = upperBound[i];

            // ---- Elkan filter 1: skip if upper bound <= half-min-centroid-dist ----
            if (useElkan && uBound <= s[assigned]) {
                // Point cannot change assignment — accumulate and continue
                private_count[tid][assigned]++;
                for (int d = 0; d < D; ++d)
                    private_sum[tid][assigned][d] += pts.at(d, i);
                continue;
            }

            // ---- Full distance to current centroid ----
            float dCurr = std::sqrt(squaredDist(pts, i, centroids[assigned].coords, D));
            upperBound[i] = dCurr;   // tighten upper bound

            int   bestK  = assigned;
            float bestD  = dCurr;

            // ---- Check other centroids with Elkan lower-bound filter ----
            for (int k = 0; k < K; ++k) {
                if (k == assigned) continue;

                // Elkan filter 2: lower bound >= upper bound — skip
                if (useElkan && lowerBound[i][k] >= bestD) continue;

                // Elkan filter 3: inter-centroid distance pruning
                if (useElkan && dCC[assigned * K + k] >= 2.0f * bestD) continue;

                float dk = std::sqrt(squaredDist(pts, i, centroids[k].coords, D));
                if (useElkan) lowerBound[i][k] = dk;

                if (dk < bestD) {
                    bestD = dk;
                    bestK = k;
                }
            }

            if (bestK != assigned) {
                labels[i]     = bestK;
                upperBound[i] = bestD;
                anyChanged    = true;
            }

            private_count[tid][bestK]++;
            for (int d = 0; d < D; ++d)
                private_sum[tid][bestK][d] += pts.at(d, i);
        }
    } // end parallel

    // -------------------------------------------------------------------------
    // Reduction: merge all thread-private accumulators into global arrays.
    // One small parallel loop over K — trivial cost.
    // -------------------------------------------------------------------------
    #pragma omp parallel for schedule(static)
    for (int k = 0; k < K; ++k) {
        globalCount[k] = 0;
        std::fill(globalSum[k].begin(), globalSum[k].end(), 0.0f);
        for (int t = 0; t < nThreads; ++t) {
            globalCount[k] += private_count[t][k];
            for (int d = 0; d < D; ++d)
                globalSum[k][d] += private_sum[t][k][d];
        }
    }

    return anyChanged;
}

// =============================================================================
// PHASE 5 — UPDATE CENTROIDS
// =============================================================================
// Compute new centroid positions from accumulated sums.
// Returns max centroid shift (used for convergence check).
// Parallelized over K — scales with large K.

static float updateCentroids(
    std::vector<Centroid>&          centroids,
    const std::vector<std::vector<float>>& globalSum,
    const std::vector<long long>&   globalCount,
    int                             K,
    int                             D,
    std::vector<float>&             centroidShift)   // per-centroid shift for Elkan update
{
    float maxShift = 0.0f;

    #pragma omp parallel for schedule(static) reduction(max: maxShift)
    for (int k = 0; k < K; ++k) {
        if (globalCount[k] == 0) continue;   // empty cluster — keep old centroid

        float inv   = 1.0f / (float)globalCount[k];
        float shift = 0.0f;

        for (int d = 0; d < D; ++d) {
            float newVal = globalSum[k][d] * inv;
            float diff   = newVal - centroids[k].coords[d];
            shift += diff * diff;
            centroids[k].coords[d] = newVal;
        }

        centroidShift[k] = std::sqrt(shift);
        if (centroidShift[k] > maxShift)
            maxShift = centroidShift[k];
    }
    return maxShift;
}

// =============================================================================
// PHASE 6 — ELKAN BOUND UPDATE
// =============================================================================
// After centroids move, update upper and lower bounds for all points.
// upper[i] += shift[assigned[i]]
// lower[i][k] = max(0, lower[i][k] - shift[k])

static void updateElkanBounds(
    int                          N,
    int                          K,
    const std::vector<int>&      labels,
    const std::vector<float>&    centroidShift,
    std::vector<float>&          upperBound,
    std::vector<std::vector<float>>& lowerBound)
{
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < N; ++i) {
        upperBound[i] += centroidShift[labels[i]];
        for (int k = 0; k < K; ++k) {
            lowerBound[i][k] = std::max(0.0f, lowerBound[i][k] - centroidShift[k]);
        }
    }
}

// =============================================================================
// PHASE 7 — NUMA-AWARE DATA INITIALIZATION
// =============================================================================
// Each thread touches its own chunk of the point array first.
// Linux first-touch policy places those pages on the NUMA node local to each thread.
// Later parallel reads are local — no cross-socket bus traffic.

static void numaFirstTouch(PointSoA& pts) {
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < pts.n; ++i) {
        for (int d = 0; d < pts.d; ++d) {
            pts.at(d, i) = pts.at(d, i);   // read-write touch to force page placement
        }
    }
}

// =============================================================================
// MAIN K-MEANS FUNCTION
// =============================================================================

std::vector<int> kMeans(PointSoA& pts, int K) {
    int N = pts.n;
    int D = pts.d;

    assert(K > 0 && K <= N);

    // -------------------------------------------------------------------------
    // Phase 7: NUMA first-touch
    // -------------------------------------------------------------------------
    numaFirstTouch(pts);

    // -------------------------------------------------------------------------
    // Phase 1: Initialization — max-range sort + partition means
    // -------------------------------------------------------------------------
    std::vector<Centroid> centroids(K);

    int maxDim = findMaxRangeDimension(pts);
    std::vector<int> sortedIdx = sortByDimension(pts, maxDim);
    initCentroidsFromPartitions(pts, sortedIdx, K, centroids);

    // -------------------------------------------------------------------------
    // Allocate labels, bounds, accumulators
    // -------------------------------------------------------------------------
    std::vector<int>   labels(N, 0);
    std::vector<float> upperBound(N, std::numeric_limits<float>::max());
    std::vector<std::vector<float>> lowerBound(N, std::vector<float>(K, 0.0f));

    std::vector<std::vector<float>> globalSum(K, std::vector<float>(D, 0.0f));
    std::vector<long long>          globalCount(K, 0LL);
    std::vector<float>              centroidShift(K, 0.0f);

    // Inter-centroid distances for Elkan
    std::vector<float> dCC;
    std::vector<float> s(K);

    // Initial full assignment (no pruning on first pass)
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < N; ++i) {
        float best = std::numeric_limits<float>::max();
        int   bestK = 0;
        for (int k = 0; k < K; ++k) {
            float d = squaredDist(pts, i, centroids[k].coords, D);
            if (d < best) { best = d; bestK = k; }
        }
        labels[i]     = bestK;
        upperBound[i] = std::sqrt(best);
        for (int k = 0; k < K; ++k)
            lowerBound[i][k] = std::sqrt(squaredDist(pts, i, centroids[k].coords, D));
    }

    // -------------------------------------------------------------------------
    // Main iteration loop
    // -------------------------------------------------------------------------
    for (int iter = 0; iter < MAX_ITER; ++iter) {

        // Phase 3: Recompute inter-centroid distances periodically
        if (iter % ELKAN_UPDATE == 0) {
            computeInterCentroidDists(centroids, K, dCC);
            computeHalfMinDist(dCC, K, s);
        }

        // Reset accumulators
        #pragma omp parallel for schedule(static)
        for (int k = 0; k < K; ++k) {
            globalCount[k] = 0;
            std::fill(globalSum[k].begin(), globalSum[k].end(), 0.0f);
        }

        // Phase 4: Assignment with Elkan pruning + thread-private reduction
        bool changed = assignmentStep(
            pts, centroids, dCC, s, K,
            labels, upperBound, lowerBound,
            globalSum, globalCount, /*useElkan=*/true);

        // Phase 5: Update centroids
        float maxShift = updateCentroids(
            centroids, globalSum, globalCount, K, D, centroidShift);

        // Phase 6: Update Elkan bounds
        updateElkanBounds(N, K, labels, centroidShift, upperBound, lowerBound);

        // std::cout << "Iter " << iter + 1
        //           << "  max centroid shift: " << maxShift << "\n";

        // // Convergence check
        // if (!changed || maxShift < CONVERGE_TOL) {
        //     std::cout << "Converged at iteration " << iter + 1 << "\n";
        //     break;
        // }
    }

    return labels;
}

// =============================================================================
// MAIN — Example driver
// =============================================================================

// #ifndef KMEANS_BENCHMARK
// int main() {
//     const int N = 100000;
//     const int D = 8;
//     const int K = 16;

//     SharedPoints shared = initializePoints(N, D, 42);
//     PointSoA pts = makePointSoA(shared);

//     std::cout << "Running K-Means: N=" << N
//               << " D=" << D << " K=" << K
//               << " Threads=" << omp_get_max_threads() << "\n\n";

//     double t0 = omp_get_wtime();
//     auto labels = kMeans(pts, K);
//     double t1 = omp_get_wtime();

//     std::cout << "\nTotal time: " << (t1 - t0) << " seconds\n";

//     std::cout << "First 10 labels: ";
//     for (int i = 0; i < 10; ++i) std::cout << labels[i] << " ";
//     std::cout << "\n";

//     return 0;
// }
// #endif