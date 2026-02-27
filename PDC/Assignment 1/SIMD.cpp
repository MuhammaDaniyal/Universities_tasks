#include <iostream>
#include <vector>
#include <fstream>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <immintrin.h> 


using namespace std;

int intersect(vector<int>& Adj_A, vector<int>& Adj_B, int v)
{
    const int* a  = Adj_A.data();
    const int* b  = Adj_B.data();
    int na = (int)Adj_A.size();
    int nb = (int)Adj_B.size();
    int count = 0;

    int i = 0, j = 0;

    // Skip elements <= v
    while (i < na && a[i] <= v) i++;
    while (j < nb && b[j] <= v) j++;

    // SIMD zone: both have >= 8 elements remaining
    while (i + 8 <= na && j + 8 <= nb)
    {
        int a_min = a[i],     a_max = a[i+7];
        int b_min = b[j],     b_max = b[j+7];

        // Load 8 ints from each
        __m256i va = _mm256_loadu_si256((__m256i*)(a + i));
        __m256i vb = _mm256_loadu_si256((__m256i*)(b + j));

        // For each of the 8 b values, compare against all 8 a values
        // This is an 8x8 comparison — counts matches
        
        // Unroll: compare each lane of vb against all of va
        __m256i b0 = _mm256_set1_epi32(b[j+0]);
        __m256i b1 = _mm256_set1_epi32(b[j+1]);
        __m256i b2 = _mm256_set1_epi32(b[j+2]);
        __m256i b3 = _mm256_set1_epi32(b[j+3]);
        __m256i b4 = _mm256_set1_epi32(b[j+4]);
        __m256i b5 = _mm256_set1_epi32(b[j+5]);
        __m256i b6 = _mm256_set1_epi32(b[j+6]);
        __m256i b7 = _mm256_set1_epi32(b[j+7]);

        // Compare each broadcast against all 8 of va
        __m256i c0 = _mm256_cmpeq_epi32(va, b0);
        __m256i c1 = _mm256_cmpeq_epi32(va, b1);
        __m256i c2 = _mm256_cmpeq_epi32(va, b2);
        __m256i c3 = _mm256_cmpeq_epi32(va, b3);
        __m256i c4 = _mm256_cmpeq_epi32(va, b4);
        __m256i c5 = _mm256_cmpeq_epi32(va, b5);
        __m256i c6 = _mm256_cmpeq_epi32(va, b6);
        __m256i c7 = _mm256_cmpeq_epi32(va, b7);

        // OR all comparison results, then popcount
        __m256i any = _mm256_or_si256(
                        _mm256_or_si256(_mm256_or_si256(c0,c1), _mm256_or_si256(c2,c3)),
                        _mm256_or_si256(_mm256_or_si256(c4,c5), _mm256_or_si256(c6,c7))
                      );
        // Count how many lanes matched (each match = 4 bytes set = 0xFFFFFFFF)
        // _mm256_movemask_epi8 gives 1 bit per byte, so divide by 4 for int matches
        count += __builtin_popcount(_mm256_movemask_epi8(any)) / 4;

        // Advance the pointer whose max is smaller
        if (a_max < b_max)      i += 8;
        else if (b_max < a_max) j += 8;
        else { i += 8; j += 8; }
    }

    // Scalar tail for remaining elements
    while (i < na && j < nb)
    {
        if (a[i] <= v) { i++; continue; }
        if (b[j] <= v) { j++; continue; }
        if      (a[i] < b[j]) i++;
        else if (a[i] > b[j]) j++;
        else { count++; i++; j++; }
    }

    return count;
}

int main() {


    ifstream inputFile("/home/daniyal/Documents/data_set/email-Eu-core.txt");
    int graph_size = 1005;

    // ifstream inputFile("data_set/com-lj.ungraph.txt");
    // int graph_size = 34681189;

    if (!inputFile.is_open()) {
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

    vector<pair<int,int>> edges;
    while (getline(inputFile, line)) {
        if (line.empty() || line[0] == '#') continue;
        stringstream ss(line);
        if (ss >> u >> v) {
            if (u == v) continue; 

            int a = min(u,v);
            int b = max(u,v);
            edges.push_back({a,b});
        }
    }

    // Sort edges and remove duplicates
    sort(edges.begin(), edges.end());
    edges.erase(unique(edges.begin(), edges.end()), edges.end());

    // Now build adjacency list
    for (auto &e : edges) {
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


    // Count triangles using the intersection method

    start_time = chrono::high_resolution_clock::now();

    for(int u = 0; u < graph.size(); ++u)
    {
        for(int v : graph[u])
        {
            if(u < v)
            {
                totalTriangles += intersect(graph[u], graph[v], v);
            }
        }
    }

    end_time = chrono::high_resolution_clock::now();
    duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);
    cout << "Triangle counting time: " << duration.count() << " milliseconds" << endl;

    cout << "Total number of triangles: " << totalTriangles << endl;


    return 0;
}