#include <vector>
#include <cmath>
#include <iostream>
#include <chrono>
#include <omp.h>

using namespace std;
using namespace chrono;

static float dist(const vector<float>& a, const vector<float>& b) {
    float d = 0;
    for (int i = 0; i < (int)a.size(); i++) d += (a[i]-b[i])*(a[i]-b[i]);
    return sqrtf(d);
}

vector<int> kmeans_naive(const vector<vector<float>>& pts, vector<vector<float>>& cents, int iters) {
    int N = pts.size(), K = cents.size(), D = pts[0].size();
    vector<int> labels(N);
    for (int it = 0; it < iters; it++) {
        // assign
        for (int i = 0; i < N; i++) {
            float best = 1e30f; int bi = 0;
            for (int k = 0; k < K; k++) { float d = dist(pts[i], cents[k]); if (d < best) { best = d; bi = k; } }
            labels[i] = bi;
        }
        // update
        vector<vector<float>> sums(K, vector<float>(D, 0)); vector<int> cnt(K, 0);
        for (int i = 0; i < N; i++) { int k = labels[i]; cnt[k]++; for (int d = 0; d < D; d++) sums[k][d] += pts[i][d]; }
        for (int k = 0; k < K; k++) if (cnt[k]) for (int d = 0; d < D; d++) cents[k][d] = sums[k][d] / cnt[k];
    }
    return labels;
}

vector<int> kmeans_omp(const vector<vector<float>>& pts, vector<vector<float>>& cents, int iters) {
    int N = pts.size(), K = cents.size(), D = pts[0].size();
    vector<int> labels(N);
    for (int it = 0; it < iters; it++) {
        // assign parallel
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < N; i++) {
            float best = 1e30f; int bi = 0;
            for (int k = 0; k < K; k++) { float d = dist(pts[i], cents[k]); if (d < best) { best = d; bi = k; } }
            labels[i] = bi;
        }
        // update parallel
        vector<vector<float>> sums(K, vector<float>(D, 0)); vector<int> cnt(K, 0);
        #pragma omp parallel
        {
            vector<vector<float>> lsums(K, vector<float>(D, 0)); vector<int> lcnt(K, 0);
            #pragma omp for nowait
            for (int i = 0; i < N; i++) { int k = labels[i]; lcnt[k]++; for (int d = 0; d < D; d++) lsums[k][d] += pts[i][d]; }
            #pragma omp critical
            for (int k = 0; k < K; k++) { cnt[k] += lcnt[k]; for (int d = 0; d < D; d++) sums[k][d] += lsums[k][d]; }
        }
        for (int k = 0; k < K; k++) if (cnt[k]) for (int d = 0; d < D; d++) cents[k][d] = sums[k][d] / cnt[k];
    }
    return labels;
}

int main() {
    int N = 1000000, K = 4, ITERS = 20;
    // generate random points
    vector<vector<float>> pts(N, vector<float>(2));
    for (auto& p : pts) { p[0] = (float)rand()/RAND_MAX*100; p[1] = (float)rand()/RAND_MAX*100; }
    vector<vector<float>> c1(K, vector<float>(2)), c2(K, vector<float>(2));
    for (int k = 0; k < K; k++) { c1[k] = pts[k*100]; c2[k] = pts[k*100]; }

    // naive
    auto t0 = high_resolution_clock::now();
    auto l1 = kmeans_naive(pts, c1, ITERS);
    auto t1 = high_resolution_clock::now();
    cout << "Naive: " << duration_cast<milliseconds>(t1-t0).count() << "ms\n";

    // omp
    t0 = high_resolution_clock::now();
    auto l2 = kmeans_omp(pts, c2, ITERS);
    t1 = high_resolution_clock::now();
    cout << "OMP:   " << duration_cast<milliseconds>(t1-t0).count() << "ms\n";

    // print first 2 centroids
    for (int k = 0; k < 2; k++)
        cout << "Centroid[" << k << "]: (" << c2[k][0] << ", " << c2[k][1] << ")\n";
}