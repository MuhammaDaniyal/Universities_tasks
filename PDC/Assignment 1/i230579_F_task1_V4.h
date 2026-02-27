/*
    Name: Muhammad Daniyal
    Roll Number: i230579
    Assignment: 2
*/

#ifndef V4
#define V4

#include <vector>
#include <omp.h>

#include "i230579_F_task1_V1.h"

using namespace std;

long long triangle_count_hybrid_intrinsic(vector<vector<int>>& graph, int graph_size, int num_threads)
{
    long long totalTriangles = 0;
    omp_set_num_threads(num_threads);

    #pragma omp parallel for reduction(+:totalTriangles) schedule(dynamic, 64)
    for(int u = 0; u < graph_size; ++u)
    {
        for(int v : graph[u])
        {
            if(u < v)
            {
                totalTriangles += intersect_SIMD_only(graph[u], graph[v], v);
            }
        }
    }

    return totalTriangles;
}

#endif