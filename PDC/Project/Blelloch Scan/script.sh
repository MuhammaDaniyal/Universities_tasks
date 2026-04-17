#!/usr/bin/env bash
# benchmark.sh — Build all versions and run comparative benchmarks
# Usage: ./benchmark.sh [array_size]

set -euo pipefail

N=${1:-10000000}    # default 10 million
SEP="──────────────────────────────────────────────────────"

echo "╔══════════════════════════════════════════════════════╗"
echo "║  Parallel Prefix Sum (Blelloch Scan) — Benchmark     ║"
echo "╚══════════════════════════════════════════════════════╝"
echo "Array size : $N"
echo ""

# ── Build ──────────────────────────────────────────────────
echo "Building all targets…"
if ! make all 2>&1; then
    echo "[WARN] Some targets may have failed to build (OpenCL not installed?)."
fi
echo ""

# ── Naive (sequential) ────────────────────────────────────
echo "$SEP"
echo "1 / 3  NAIVE (sequential)"
echo "$SEP"
if [ -x ./naive ]; then
    ./naive "$N"
else
    echo "[SKIP] ./naive not built"
fi
echo ""

# ── OpenMP ────────────────────────────────────────────────
echo "$SEP"
echo "2 / 3  OPENMP (parallel CPU)"
echo "$SEP"
if [ -x ./openmp ]; then
    for T in 1 2 4 8; do
        printf "  OMP_NUM_THREADS=%-2s  " "$T"
        OMP_NUM_THREADS=$T ./openmp "$N" 2>&1 | grep -E "Time|Correct"
    done
else
    echo "[SKIP] ./openmp not built"
fi
echo ""

# ── OpenCL ────────────────────────────────────────────────
echo "$SEP"
echo "3 / 3  OPENCL (GPU, or CPU fallback)"
echo "$SEP"
if [ -x ./opencl ]; then
    ./opencl "$N"
else
    echo "[SKIP] ./opencl not built (OpenCL headers/lib not found)"
fi
echo ""

# ── Larger size test ──────────────────────────────────────
BIG=100000000
echo "$SEP"
echo "LARGE ARRAY TEST  (N = $BIG)"
echo "$SEP"
if [ -x ./naive ];  then echo -n "  naive  : "; ./naive  "$BIG" | grep Time; fi
if [ -x ./openmp ]; then echo -n "  openmp : "; OMP_NUM_THREADS=$(nproc 2>/dev/null || echo 4) ./openmp "$BIG" | grep Time; fi
if [ -x ./opencl ]; then echo -n "  opencl : "; ./opencl "$BIG" | grep Time; fi

echo ""
echo "Done."