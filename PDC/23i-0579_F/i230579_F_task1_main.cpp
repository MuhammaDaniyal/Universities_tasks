/*
    Name: Muhammad Daniyal
    Roll Number: i230579
    Assignment: 2
*/

#include <iostream>
#include <vector>
#include <fstream>
#include <chrono>
#include <sstream>
#include <algorithm>

#include "i230579_F_task1_V0.h"
#include "i230579_F_task1_V1.h"
#include "i230579_F_task1_V2.h"
#include "i230579_F_task1_V3.h"
#include "i230579_F_task1_V4.h"
#include "i230579_F_task1_V5.h"

using namespace std;

int main()
{

    ifstream inputFile("/home/daniyal/Documents/data_set/email-Eu-core.txt");
    int graph_size = 1005;

    // ifstream inputFile("/home/daniyal/Documents/data_set/com-lj.ungraph.txt");
    // int graph_size = 34681189;

    if (!inputFile.is_open())
    {
        cerr << "Error opening file!" << endl;
        return 1;
    }

    vector<vector<int>> graph(graph_size);
    long long totalTriangles = 0;

    // Measure the time taken to read the file and build the graph

    auto start_time = chrono::high_resolution_clock::now();

    int u, v;
    int size_ = 0;
    string line;

    vector<pair<int, int>> edges;
    while (getline(inputFile, line))
    {
        if (line.empty() || line[0] == '#')
            continue;
        stringstream ss(line);
        if (ss >> u >> v)
        {
            if (u == v)
                continue;

            int a = min(u, v);
            int b = max(u, v);
            edges.push_back({a, b});
        }
    }

    // Sort edges and remove duplicates
    sort(edges.begin(), edges.end());
    edges.erase(unique(edges.begin(), edges.end()), edges.end());

    // Now build adjacency list
    for (auto &e : edges)
    {
        u = e.first;
        v = e.second;
        graph[u].push_back(v);
        graph[v].push_back(u);
    }

    cout << edges.size() << endl;

    auto end_time = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);
    cout << "File reading time: " << duration.count() << " milliseconds" << endl;

    // Sort the adjacency lists to enable efficient intersection counting

    start_time = chrono::high_resolution_clock::now();

    for (int i = 0; i < graph.size(); i++)
        sort(graph[i].begin(), graph[i].end());

    end_time = chrono::high_resolution_clock::now();
    duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);
    cout << "Sorting time: " << duration.count() << " milliseconds" << endl;

    /////////////////////////////////////////////////////////////////
    /////////////////////////// NAIVE ///////////////////////////////
    /////////////////////////////////////////////////////////////////

    start_time = chrono::high_resolution_clock::now();

    for (int u = 0; u < graph.size(); ++u)
    {
        for (int v : graph[u])
        {
            if (u < v)
            {
                totalTriangles += intersect_naive(graph[u], graph[v], v);
            }
        }
    }

    end_time = chrono::high_resolution_clock::now();
    duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);
    cout << "\n[Naive] time: " << duration.count() << " ms\t\t";
    cout << "[Naive] triangles: " << totalTriangles << endl;

    /////////////////////////////////////////////////////////////////
    /////////////////////////// SIMD ////////////////////////////////
    /////////////////////////////////////////////////////////////////

    totalTriangles = 0;
    start_time = chrono::high_resolution_clock::now();

    for (int u = 0; u < graph.size(); ++u)
    {
        for (int v : graph[u])
        {
            if (u < v)
            {
                totalTriangles += intersect_SIMD_only(graph[u], graph[v], v);
            }
        }
    }

    end_time = chrono::high_resolution_clock::now();
    duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);
    cout << "\n[SIMD] time: " << duration.count() << " ms\t\t";
    cout << "[SIMD] triangles: " << totalTriangles << endl;

    /////////////////////////////////////////////////////////////////
    //////////////////////// Openmp SIMD ////////////////////////////
    /////////////////////////////////////////////////////////////////

    totalTriangles = 0;
    start_time = chrono::high_resolution_clock::now();

    for (int u = 0; u < graph.size(); ++u)
    {
        for (int v : graph[u])
        {
            if (u < v)
            {
                totalTriangles += intersect_omp_simd(graph[u], graph[v], v);
            }
        }
    }

    end_time = chrono::high_resolution_clock::now();
    duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);
    cout << "\n[OpenMP SIMD] time: " << duration.count() << " ms\t";
    cout << "[OpenMP SIMD] triangles: " << totalTriangles << endl;

    /////////////////////////////////////////////////////////////////
    ////////////////////////// Openmp ///////////////////////////////
    /////////////////////////////////////////////////////////////////

    cout << "\n--- [OpenMP] ---" << endl;

    int thread_counts[] = {1, 2, 4, 8};

    for (int t : thread_counts)
    {
        int triangles = 0;

        auto start = chrono::high_resolution_clock::now();

        triangles = triangle_count_openmp(graph, graph_size, t);

        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);

        cout << "Threads: " << t
             << " | Time: " << duration.count() << "ms"
             << " | Triangles: " << triangles << endl;
    }

    /////////////////////////////////////////////////////////////////
    ////////////////// Hybrid 1: Intrinsic + OpenMP /////////////////
    /////////////////////////////////////////////////////////////////

    cout << "\n--- [Hybrid 1] AVX2 Intrinsics + OpenMP Threading ---" << endl;

    for (int t : thread_counts)
    {
        long long triangles = 0;
        auto start = chrono::high_resolution_clock::now();
        triangles = triangle_count_hybrid_intrinsic(graph, graph_size, t);
        auto end = chrono::high_resolution_clock::now();
        auto dur = chrono::duration_cast<chrono::milliseconds>(end - start);
        cout << "Threads: " << t
             << " | Time: " << dur.count() << "ms"
             << " | Triangles: " << triangles << endl;
    }

    /////////////////////////////////////////////////////////////////
    ////////////////// Hybrid 2: OpenMP SIMD + OpenMP ///////////////
    /////////////////////////////////////////////////////////////////

    cout << "\n--- [Hybrid 2] OpenMP SIMD + OpenMP Threading ---" << endl;

    for (int t : thread_counts)
    {
        long long triangles = 0;
        auto start = chrono::high_resolution_clock::now();
        triangles = triangle_count_hybrid_omp_simd(graph, graph_size, t);
        auto end = chrono::high_resolution_clock::now();
        auto dur = chrono::duration_cast<chrono::milliseconds>(end - start);
        cout << "Threads: " << t
             << " | Time: " << dur.count() << "ms"
             << " | Triangles: " << triangles << endl;
    }

    return 0;
}