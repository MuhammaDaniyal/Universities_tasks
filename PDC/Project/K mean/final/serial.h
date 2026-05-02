#include <bits/stdc++.h>
#include "utils.h"
using namespace std;
using namespace chrono;

// ----------- Utility: Distance -----------
inline double euclideanDistance(const vector<double>& a, const vector<double>& b) {
    double sum = 0.0;
    for (size_t i = 0; i < a.size(); i++) {
        double diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sqrt(sum);
}

// ----------- Generate Random Dataset -----------
vector<Point> generatePoints(int N, int dim, double min_val = 0.0, double max_val = 100.0) {
    vector<Point> data(N);
    random_device rd;
    mt19937 gen(rd());
    uniform_real_distribution<> dis(min_val, max_val);

    for (int i = 0; i < N; i++) {
        data[i].values.resize(dim);
        for (int d = 0; d < dim; d++) {
            data[i].values[d] = dis(gen);
        }
        data[i].cluster = -1;
    }

    return data;
}

// makePoints is defined in utils.cpp

// ----------- KMeans -----------
class KMeans {
private:
    int K;
    int max_iters;
    vector<vector<double>> centroids;

public:
    KMeans(int k, int max_iters) : K(k), max_iters(max_iters) {}

    // returns execution time in milliseconds
    double fit(vector<Point>& data) {
        auto start = high_resolution_clock::now();

        int n = data.size();
        int dim = data[0].values.size();

        // --- Initialize centroids randomly ---
        centroids.clear();
        unordered_set<int> chosen;
        mt19937 gen(random_device{}());
        uniform_int_distribution<> dis(0, n - 1);

        while ((int)centroids.size() < K) {
            int idx = dis(gen);
            if (chosen.insert(idx).second) {
                centroids.push_back(data[idx].values);
            }
        }

        // --- Main loop ---
        for (int iter = 0; iter < max_iters; iter++) {
            bool changed = false;

            // Assignment step
            for (int i = 0; i < n; i++) {
                double minDist = DBL_MAX;
                int bestCluster = -1;

                for (int c = 0; c < K; c++) {
                    double dist = euclideanDistance(data[i].values, centroids[c]);
                    if (dist < minDist) {
                        minDist = dist;
                        bestCluster = c;
                    }
                }

                if (data[i].cluster != bestCluster) {
                    data[i].cluster = bestCluster;
                    changed = true;
                }
            }

            if (!changed) break;

            // Update step
            vector<vector<double>> newCentroids(K, vector<double>(dim, 0.0));
            vector<int> counts(K, 0);

            for (const auto& p : data) {
                int c = p.cluster;
                for (int d = 0; d < dim; d++) {
                    newCentroids[c][d] += p.values[d];
                }
                counts[c]++;
            }

            for (int c = 0; c < K; c++) {
                if (counts[c] == 0) continue;
                for (int d = 0; d < dim; d++) {
                    newCentroids[c][d] /= counts[c];
                }
            }

            centroids = newCentroids;
        }

        auto end = high_resolution_clock::now();
        return duration<double, milli>(end - start).count();
    }
};

// // ----------- Driver -----------
// #ifndef KMEANS_BENCHMARK
// int main() {
//     int N = 100000;   // number of points
//     int dim = 2;      // dimensions
//     int K = 5;        // clusters
//     int max_iters = 100;

//     SharedPoints shared = initializePoints(N, dim, 42);
//     vector<Point> data = makePoints(shared);

//     KMeans kmeans(K, max_iters);
//     double time_ms = kmeans.fit(data);

//     cout << "Execution Time: " << time_ms << " ms\n";

//     return 0;
// }
// #endif