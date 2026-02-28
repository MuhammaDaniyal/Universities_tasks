/*
    Name: Muhammad Daniyal
    Roll Number: i230579
    Assignment: 2
*/

#ifndef V5
#define V5

#include <vector>
#include <omp.h>

#include "i230579_F_task1_V2.h"

using namespace std;

long long triangle_count_hybrid_omp_simd(vector<vector<int>>& graph, int graph_size, int num_threads)
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
                totalTriangles += intersect_omp_simd(graph[u], graph[v], v);
            }
        }
    }

    return totalTriangles;
}

#endif