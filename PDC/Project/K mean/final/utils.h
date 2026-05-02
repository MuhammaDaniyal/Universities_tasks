// =============================================================================
// UTILITIES HEADER
// Common data structures and utility functions for K-Means benchmarking
// =============================================================================

#pragma once

#include <vector>
#include <cmath>
#include <unordered_set>
#include <limits>
#include <algorithm>
#include <numeric>
#include <random>

// =============================================================================
// SHARED DATA STRUCTURE
// =============================================================================

/// Container for shared point dataset across both serial and parallel implementations.
/// Uses row-major layout: all coordinates for point i occupy contiguous memory.
/// This enables efficient access patterns for both AoS and SoA conversions.
struct SharedPoints {
    int n = 0;                              // number of points
    int d = 0;                              // number of dimensions
    std::vector<double> values;             // row-major: value[i*d + j] = point i, dim j

    SharedPoints() = default;
    
    /// Constructor initializes with specified dimensions.
    /// Allocates contiguous memory for n*d values.
    SharedPoints(int n, int d) 
        : n(n), d(d), values(static_cast<std::size_t>(n) * d) {}

    /// Access element at (point, dim).
    double& at(int point, int dim) { 
        return values[static_cast<std::size_t>(point) * d + dim]; 
    }
    
    /// Const access element at (point, dim).
    double at(int point, int dim) const { 
        return values[static_cast<std::size_t>(point) * d + dim]; 
    }
};

// =============================================================================
// SERIAL DATA STRUCTURE
// =============================================================================

/// Point structure for serial K-Means implementation.
/// Array of Structures (AoS) layout - good for serial access patterns.
struct Point {
    std::vector<double> values;     // coordinates for this point
    int cluster = -1;               // assigned cluster ID
};

// =============================================================================
// PARALLEL DATA STRUCTURE
// =============================================================================

/// Structure of Arrays (SoA) layout for parallel K-Means.
/// All x-coordinates contiguous, then all y-coordinates, etc.
/// Enables SIMD vectorization and better cache utilization.
struct PointSoA {
    int n;                              // number of points
    int d;                              // number of dimensions
    std::vector<float> data;            // data[dim*n + i] = point i, dimension dim

    /// Constructor initializes with specified dimensions.
    PointSoA(int n, int d) : n(n), d(d), data(static_cast<std::size_t>(n) * d) {}

    /// Access element at (dim, point) in SoA format.
    float& at(int dim, int i) { 
        return data[static_cast<std::size_t>(dim) * n + i]; 
    }
    
    /// Const access element at (dim, point) in SoA format.
    float at(int dim, int i) const { 
        return data[static_cast<std::size_t>(dim) * n + i]; 
    }
};

// =============================================================================
// CENTROID STRUCTURE (for parallel implementation)
// =============================================================================

/// Centroid with alignment for false-sharing prevention.
/// Each centroid occupies a full cache line to prevent thread contention.
struct alignas(64) Centroid {
    std::vector<float> coords;      // centroid coordinates
    std::vector<float> sum;         // accumulator for mean calculation
    long long count;                // number of assigned points

    /// Initialize centroid with specified dimension.
    void init(int d) {
        coords.assign(d, 0.0f);
        sum.assign(d, 0.0f);
        count = 0;
    }

    /// Reset accumulators for new iteration.
    void resetAccum() {
        std::fill(sum.begin(), sum.end(), 0.0f);
        count = 0;
    }
};

// =============================================================================
// INITIALIZATION FUNCTIONS
// =============================================================================

/// Generate random point dataset with specified dimensions.
/// Produces same dataset across multiple calls with same seed.
/// Parameters:
///   n: number of points
///   d: number of dimensions
///   seed: random seed for reproducibility
///   min_val: minimum coordinate value
///   max_val: maximum coordinate value
SharedPoints initializePoints(int n, int d, unsigned seed, double min_val, double max_val) {
    SharedPoints points(n, d);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> distribution(min_val, max_val);

    for (int point = 0; point < n; ++point) {
        for (int dim = 0; dim < d; ++dim) {
            points.at(point, dim) = distribution(rng);
        }
    }

    return points;
}

// =============================================================================
// CONVERTER FUNCTIONS
// =============================================================================

/// Convert shared dataset to parallel format (Structure of Arrays).
PointSoA makePointSoA(const SharedPoints& shared) {
    PointSoA pts(shared.n, shared.d);

    for (int point = 0; point < shared.n; ++point) {
        for (int dim = 0; dim < shared.d; ++dim) {
            pts.at(dim, point) = static_cast<float>(shared.at(point, dim));
        }
    }

    return pts;
}

/// Convert shared dataset to serial format (Array of Structures).
std::vector<Point> makePoints(const SharedPoints& shared) {
    std::vector<Point> data(shared.n);

    for (int point = 0; point < shared.n; ++point) {
        data[point].values.resize(shared.d);
        for (int dim = 0; dim < shared.d; ++dim) {
            data[point].values[dim] = shared.at(point, dim);
        }
        data[point].cluster = -1;
    }

    return data;
}
