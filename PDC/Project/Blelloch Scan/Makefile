# Makefile — Parallel Prefix Sum (Blelloch Scan)
# Targets: naive  openmp  opencl  all  clean

CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra

# OpenCL detection helper
OPENCL_LIBS := -lOpenCL
# macOS uses a framework instead
UNAME := $(shell uname -s)
ifeq ($(UNAME), Darwin)
  OPENCL_LIBS := -framework OpenCL
endif

.PHONY: all naive openmp opencl clean

all: naive openmp opencl

naive: naive.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<
	@echo "[✓] Built naive"

openmp: openmp.cpp
	$(CXX) $(CXXFLAGS) -fopenmp -o $@ $<
	@echo "[✓] Built openmp"

opencl: opencl.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(OPENCL_LIBS)
	@echo "[✓] Built opencl"

clean:
	rm -f naive openmp opencl
	@echo "[✓] Cleaned"