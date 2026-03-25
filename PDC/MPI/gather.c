#include <mpi.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    int rank, size;
    int full_array[8]   = {10, 20, 30, 40, 50, 60, 70, 80};
    int result_array[8] = {0};  // root will collect here
    int chunk[2];               // each process works on 2 elements

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // STEP 1 — Scatter: give each process 2 elements
    MPI_Scatter(full_array, 2, MPI_INT,
                chunk,      2, MPI_INT,
                0, MPI_COMM_WORLD);

    printf("Rank %d got: [%d, %d]\n", rank, chunk[0], chunk[1]);

    // STEP 2 — Each process does work on its chunk (double each value)
    chunk[0] *= 2;
    chunk[1] *= 2;

    printf("Rank %d after doubling: [%d, %d]\n", rank, chunk[0], chunk[1]);

    // STEP 3 — Gather: collect all chunks back to rank 0
    MPI_Gather(chunk,        2, MPI_INT,
               result_array, 2, MPI_INT,
               0, MPI_COMM_WORLD);

    // Only root prints the final result
    if (rank == 0) {
        printf("\nRoot gathered final array: ");
        for (int i = 0; i < 8; i++) {
            printf("%d ", result_array[i]);
        }
        printf("\n");
    }

    MPI_Finalize();
    return 0;
}