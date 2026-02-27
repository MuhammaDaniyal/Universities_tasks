#include <unordered_set>
#include <set>
#include <vector>
#include <fstream>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <string>

using namespace std;

int main()
{
    ifstream inputFile("data_set/email-Eu-core.txt");
    int graph_size = 1005;

    // ifstream inputFile("data_set/com-lj.ungraph.txt");
    // int graph_size = 34681189;

    if (!inputFile.is_open())
    {
        cerr << "Error opening file!" << endl;
        return 1;
    }

    vector<vector<int>> graph(graph_size);
    long long totalTriangles = 0;

    // place before reading loop
    set<pair<int, int>> uniqueEdges;
    long long duplicateEdgeLines = 0;

    string line;
    int u, v;
    while (getline(inputFile, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        stringstream ss(line);
        if (ss >> u >> v)
        {

            // normalize (u,v) so (3,7) and (7,3) are same
            int a = min(u, v);
            int b = max(u, v);

            if (uniqueEdges.count({a, b}))
            {
                duplicateEdgeLines++;
            }
            else
            {
                uniqueEdges.insert({a, b});
            }

            graph[u].push_back(v);
            graph[v].push_back(u);
        }
    }

    cout << "Unique edges (undirected): " << uniqueEdges.size() << endl;
    cout << "Duplicate edge lines: " << duplicateEdgeLines << endl;
}