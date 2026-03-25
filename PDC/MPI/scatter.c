#include <mpi.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    int rank, size;
    int full_array[8] = {10, 20, 30, 40, 50, 60, 70, 80}; // only rank 0 uses this
    int chunk[2];   // each process gets 2 elements

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Scatter: send 2 elements to each of the 4 processes
    MPI_Scatter(full_array, 2, MPI_INT,
                chunk,      2, MPI_INT,
                0, MPI_COMM_WORLD);

    // Each process prints its chunk
    printf("Rank %d received: [%d, %d]\n", rank, chunk[0], chunk[1]);

    // Each process works on its own chunk independently
    int local_sum = chunk[0] + chunk[1];
    printf("Rank %d local sum = %d\n", rank, local_sum);

    MPI_Finalize();
    return 0;
}