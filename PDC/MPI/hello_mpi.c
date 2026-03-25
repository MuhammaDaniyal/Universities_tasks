#include <mpi.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    int rank, size;
    
    MPI_Init(&argc, &argv);                        // Start MPI
    printf("\n\n");

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);          // Who am I?
    MPI_Comm_size(MPI_COMM_WORLD, &size);          // How many of us?

    // printf("Hello from process %d out of %d!\n", rank, size);


    if (rank == 0) {
        printf("\nI am the MASTER (rank 0). Total processes: %d\n", size);
    } else {
        printf("I am a WORKER (rank %d), waiting for orders!\n", rank);
    }

    MPI_Finalize();                                // End MPI
    return 0;
}