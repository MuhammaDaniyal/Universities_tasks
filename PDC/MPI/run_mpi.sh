#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "Usage: $0 <source.c> [num_processes]"
  exit 1
fi

src="$1"
np="${2:-4}"

if [[ ! -f "$src" ]]; then
  echo "Source file not found: $src"
  exit 1
fi

out="${src%.c}"

mpicc "$src" -o "$out"
HWLOC_COMPONENTS=-opencl mpirun --mca btl self,vader -np "$np" "./$out"
