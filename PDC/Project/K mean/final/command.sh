g++ -O3 -march=native -ffast-math -fopenmp parallel.h main.cpp serial.h -o kmeans_benchmark
./kmeans_benchmark