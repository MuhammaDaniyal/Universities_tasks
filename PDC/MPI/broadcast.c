#include <mpi.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    int rank;
    int data = 0;   // everyone starts with 0

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // Only rank 0 sets the real value
    if (rank == 0) {
        data = 42;
        printf("Rank 0: I have data = %d, broadcasting now...\n", data);
    }

    // ALL processes call this — rank 0 sends, others receive
    MPI_Bcast(&data, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Now everyone has it
    printf("Rank %d: I received data = %d\n", rank, data);

    MPI_Finalize();
    return 0;
}