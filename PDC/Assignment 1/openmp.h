#ifndef OPENMP_H
#define OPENMP_H

#include <omp.h>
#include <vector>
#include "naive.h" 

using namespace std;

int triangle_count_openmp(vector<vector<int>>& graph, int graph_size, int num_threads)
{
    int totalTriangles = 0;

    // Explicitly set thread count based on your hardware (you have 8)
    omp_set_num_threads(num_threads);

    // parallel for: splits loop iterations across threads
    // reduction: each thread has private copy of totalTriangles, summed at end
    // schedule dynamic: threads grab work chunks as they finish (better for uneven graphs)
    #pragma omp parallel for reduction(+:totalTriangles) schedule(dynamic, 64)
    for(int u = 0; u < graph_size; ++u)
    {
        for(int v : graph[u])
        {
            if(u < v)
            {
                totalTriangles += intersect_naive(graph[u], graph[v], v);
            }
        }
    }

    return totalTriangles;
}

#endif