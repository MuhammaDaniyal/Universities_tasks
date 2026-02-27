#include <iostream>
#include <vector>
#include <fstream>
#include <chrono>
#include <sstream>
#include <algorithm>

using namespace std;

int intersect(vector<int>& Adj_A, vector<int>& Adj_B, int v)
{
    int i = 0, j = 0, count = 0;

    while(i < Adj_A.size() && j < Adj_B.size())
    {
        if (Adj_A[i] <= v) { i++; continue; }
        if (Adj_B[j] <= v) { j++; continue; }
        
        if(Adj_A[i] < Adj_B[j])
        {
            i++;
        }
        else if(Adj_A[i] > Adj_B[j])
        {
            j++;
        }
        else
        {
            i++;
            j++;
            count++;
        }
    }

    return count;
}

int main() {


    ifstream inputFile("/home/daniyal/Documents/data_set/email-Eu-core.txt");
    int graph_size = 1005;

    // ifstream inputFile("/home/daniyal/Documents/data_set/com-lj.ungraph.txt");
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