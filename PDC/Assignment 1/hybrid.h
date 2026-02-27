#ifndef HYBRID_H
#define HYBRID_H

#include <vector>
#include <omp.h>
#include "SIMD_only.h"      // intersect_SIMD_only()
#include "openmp_simd.h"    // intersect_omp_simd()

using namespace std;

// ─── Hybrid 1: OpenMP threads (outer) + AVX2 Intrinsics (inner) ───────────────
long long triangle_count_hybrid_intrinsic(vector<vector<int>>& graph, int graph_size, int num_threads)
{
    long long totalTriangles = 0;
    omp_set_num_threads(num_threads);

    // Outer loop parallelized across threads
    // Inner intersect uses hand-written AVX2 intrinsics
    #pragma omp parallel for reduction(+:totalTriangles) schedule(dynamic, 64)
    for(int u = 0; u < graph_size; ++u)
    {
        for(int v : graph[u])
        {
            if(u < v)
            {
                // intersect_SIMD_only is thread-safe: only reads graph data
                totalTriangles += intersect_SIMD_only(graph[u], graph[v], v);
            }
        }
    }

    return totalTriangles;
}

// ─── Hybrid 2: OpenMP threads (outer) + OpenMP SIMD (inner) ───────────────────
long long triangle_count_hybrid_omp_simd(vector<vector<int>>& graph, int graph_size, int num_threads)
{
    long long totalTriangles = 0;
    omp_set_num_threads(num_threads);

    // Outer loop parallelized across threads
    // Inner intersect uses #pragma omp simd for vectorization
    #pragma omp parallel for reduction(+:totalTriangles) schedule(dynamic, 64)
    for(int u = 0; u < graph_size; ++u)
    {
        for(int v : graph[u])
        {
            if(u < v)
            {
                // intersect_omp_simd is thread-safe: only reads graph data
                totalTriangles += intersect_omp_simd(graph[u], graph[v], v);
            }
        }
    }

    return totalTriangles;
}

#endif