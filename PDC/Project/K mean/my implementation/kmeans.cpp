#include <iostream>
#include <vector>
#include <cmath>
#include <limits>
#include <random>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <omp.h>

struct Point { double x, y; };

// ─── Utilities ───────────────────────────────────────────────────────────────

inline double dist2(const Point& a, const Point& b) {
    double dx = a.x - b.x, dy = a.y - b.y;
    return dx*dx + dy*dy;
}

std::vector<Point> generate_points(int n, unsigned seed = 42) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(0.0, 1000.0);
    std::vector<Point> pts(n);
    for (auto& p : pts) { p.x = dist(rng); p.y = dist(rng); }
    return pts;
}

// k-means++ initialization
std::vector<Point> init_centroids_pp(const std::vector<Point>& pts, int k, unsigned seed = 7) {
    std::mt19937 rng(seed);
    int n = (int)pts.size();
    std::vector<Point> centroids;
    centroids.reserve(k);

    std::uniform_int_distribution<int> pick(0, n - 1);
    centroids.push_back(pts[pick(rng)]);

    std::vector<double> d2(n, std::numeric_limits<double>::max());
    for (int c = 1; c < k; ++c) {
        double total = 0.0;
        for (int i = 0; i < n; ++i) {
            d2[i] = std::min(d2[i], dist2(pts[i], centroids.back()));
            total += d2[i];
        }
        std::uniform_real_distribution<double> wheel(0.0, total);
        double r = wheel(rng);
        double cum = 0.0;
        int chosen = 0;
        for (int i = 0; i < n; ++i) {
            cum += d2[i];
            if (cum >= r) { chosen = i; break; }
        }
        centroids.push_back(pts[chosen]);
    }
    return centroids;
}

// ─── Sequential K-Means ──────────────────────────────────────────────────────

double kmeans_sequential(const std::vector<Point>& pts, int k,
                         int max_iter, double eps,
                         std::vector<int>& assignment) {
    int n = (int)pts.size();
    auto centroids = init_centroids_pp(pts, k);
    assignment.assign(n, 0);

    auto t0 = std::chrono::high_resolution_clock::now();

    for (int iter = 0; iter < max_iter; ++iter) {
        bool changed = false;

        // Assignment
        for (int i = 0; i < n; ++i) {
            double best = std::numeric_limits<double>::max();
            int   label = 0;
            for (int c = 0; c < k; ++c) {
                double d = dist2(pts[i], centroids[c]);
                if (d < best) { best = d; label = c; }
            }
            if (label != assignment[i]) { assignment[i] = label; changed = true; }
        }
        if (!changed) break;

        // Update
        std::vector<Point> sums(k, {0,0});
        std::vector<int>   cnt(k, 0);
        for (int i = 0; i < n; ++i) {
            int c = assignment[i];
            sums[c].x += pts[i].x;
            sums[c].y += pts[i].y;
            ++cnt[c];
        }
        double shift = 0.0;
        for (int c = 0; c < k; ++c) {
            if (cnt[c] == 0) continue;
            Point nc = { sums[c].x / cnt[c], sums[c].y / cnt[c] };
            shift = std::max(shift, std::sqrt(dist2(nc, centroids[c])));
            centroids[c] = nc;
        }
        if (shift < eps) break;
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double>(t1 - t0).count();
}

// ─── Parallel K-Means (OpenMP) ───────────────────────────────────────────────
// Improvements over paper:
//   1. k-means++ init
//   2. Thread-local accumulators (no critical section)
//   3. #pragma omp simd on inner distance loop
//   4. Convergence by centroid-shift (eps) instead of fixed iterations
//   5. static scheduling tuned per workload

double kmeans_parallel(const std::vector<Point>& pts, int k,
                       int max_iter, double eps,
                       std::vector<int>& assignment, int nthreads = 0) {
    if (nthreads > 0) omp_set_num_threads(nthreads);
    int n       = (int)pts.size();
    int nT      = omp_get_max_threads();
    auto centroids = init_centroids_pp(pts, k);
    assignment.assign(n, 0);

    // Flat centroid arrays for SIMD-friendly access
    std::vector<double> cx(k), cy(k);
    for (int c = 0; c < k; ++c) { cx[c] = centroids[c].x; cy[c] = centroids[c].y; }

    // Thread-local accumulators: [thread][cluster]
    std::vector<std::vector<double>> lsx(nT, std::vector<double>(k, 0.0));
    std::vector<std::vector<double>> lsy(nT, std::vector<double>(k, 0.0));
    std::vector<std::vector<int>>    lcnt(nT, std::vector<int>(k, 0));

    auto t0 = std::chrono::high_resolution_clock::now();

    for (int iter = 0; iter < max_iter; ++iter) {
        int changed_global = 0;

        // ── Assignment (data-parallel, SIMD inner loop) ──────────────────
        #pragma omp parallel reduction(+:changed_global)
        {
            int tid = omp_get_thread_num();
            auto& sx  = lsx[tid];
            auto& sy  = lsy[tid];
            auto& cnt = lcnt[tid];
            std::fill(sx.begin(),  sx.end(),  0.0);
            std::fill(sy.begin(),  sy.end(),  0.0);
            std::fill(cnt.begin(), cnt.end(), 0);

            #pragma omp for schedule(static, 1024)
            for (int i = 0; i < n; ++i) {
                double px = pts[i].x, py = pts[i].y;
                double best = std::numeric_limits<double>::max();
                int    label = 0;

                // SIMD over clusters
                #pragma omp simd reduction(min:best)
                for (int c = 0; c < k; ++c) {
                    double dx = px - cx[c], dy = py - cy[c];
                    double d  = dx*dx + dy*dy;
                    if (d < best) { best = d; label = c; }
                }

                if (label != assignment[i]) {
                    assignment[i] = label;
                    changed_global++;
                }
                sx[label]  += px;
                sy[label]  += py;
                cnt[label] += 1;
            }
        }
        if (changed_global == 0) break;

        // ── Update: reduce thread-local → global ─────────────────────────
        double shift = 0.0;
        for (int c = 0; c < k; ++c) {
            double gsx = 0.0, gsy = 0.0;
            int    gcnt = 0;
            for (int t = 0; t < nT; ++t) {
                gsx  += lsx[t][c];
                gsy  += lsy[t][c];
                gcnt += lcnt[t][c];
            }
            if (gcnt == 0) continue;
            double nx = gsx / gcnt, ny = gsy / gcnt;
            double dx = nx - cx[c], dy = ny - cy[c];
            shift = std::max(shift, std::sqrt(dx*dx + dy*dy));
            cx[c] = nx; cy[c] = ny;
        }
        if (shift < eps) break;
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double>(t1 - t0).count();
}

// ─── Benchmark ───────────────────────────────────────────────────────────────

void benchmark(int n, int k, int max_iter = 100, double eps = 1e-4) {
    auto pts = generate_points(n);
    std::vector<int> asgn;

    double t_seq = kmeans_sequential(pts, k, max_iter, eps, asgn);
    double t_par = kmeans_parallel  (pts, k, max_iter, eps, asgn);

    double speedup    = t_seq / t_par;
    int    nT         = omp_get_max_threads();
    double efficiency = speedup / nT * 100.0;

    printf("N=%-9d K=%-4d threads=%-2d | seq=%7.3fs  par=%7.3fs  "
           "speedup=%5.2fx  efficiency=%5.1f%%\n",
           n, k, nT, t_seq, t_par, speedup, efficiency);
}

int main() {
    printf("%-10s %-5s %-8s | %-9s  %-9s  %-10s  %s\n",
           "N", "K", "threads", "seq", "par", "speedup", "efficiency");
    printf("%s\n", std::string(75, '-').c_str());

    benchmark(100'000,  10);
    benchmark(100'000,  20);
    benchmark(500'000,  20);
    benchmark(1'000'000, 20);
    benchmark(1'000'000, 50);
    benchmark(2'000'000, 50);

    return 0;
}