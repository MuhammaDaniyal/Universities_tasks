/*
    Name: Muhammad Daniyal
    Roll Number: i230579
    Assignment: 2
*/

#ifndef OPENMP_H
#define OPENMP_H

#include <omp.h>
#include <vector>

#include "i230579_F_task1_V0.h"

using namespace std;

int triangle_count_openmp(vector<vector<int>>& graph, int graph_size, int num_threads)
{
    int totalTriangles = 0;

    omp_set_num_threads(num_threads);

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