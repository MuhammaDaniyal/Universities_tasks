#include <mpi.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    int rank;
    int data = 0;
    MPI_Request request;
    MPI_Status  status;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0) {
        data = 999;

        // Fire the send and move on immediately
        MPI_Isend(&data, 1, MPI_INT, 1, 0, MPI_COMM_WORLD, &request);

        // Do useful work while send is happening in background
        printf("Rank 0: send posted, now doing other work...\n");
        int useful = 100 + 200;  // simulate work
        printf("Rank 0: finished other work, result = %d\n", useful);

        // Now wait for the send to actually complete
        MPI_Wait(&request, &status);
        printf("Rank 0: send confirmed complete!\n");
    }

    if (rank == 1) {
        // Post the receive immediately — don't block
        MPI_Irecv(&data, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &request);

        // Do useful work while waiting for data to arrive
        printf("Rank 1: receive posted, doing other work...\n");
        int useful = 300 + 400;  // simulate work
        printf("Rank 1: finished other work, result = %d\n", useful);

        // Now actually wait for the data to be ready
        MPI_Wait(&request, &status);
        printf("Rank 1: data received = %d\n", data);
    }

    MPI_Finalize();
    return 0;
}