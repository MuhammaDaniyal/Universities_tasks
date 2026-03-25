#include <mpi.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    int rank, size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Full array — only rank 0 actually uses this
    int full_array[9] = {10, 20, 30, 40, 50, 60, 70, 80, 90};

    // How many elements each process gets
    // Rank 0 → 3 elements, Rank 1 → 2, Rank 2 → 1, Rank 3 → 3
    int sendcounts[4] = {3, 2, 1, 3};

    // Where in full_array each process's chunk starts
    // Rank 0 starts at index 0, Rank 1 at index 3, Rank 2 at 5, Rank 3 at 6
    int displs[4] = {0, 3, 5, 6};

    // Each process needs a buffer big enough for its chunk
    // Rank 0 gets 3, Rank 1 gets 2, Rank 2 gets 1, Rank 3 gets 3
    int recv_size = sendcounts[rank];
    int chunk[3]; // max size is 3 so safe for all

    // The actual scatter
    MPI_Scatterv(
        full_array,   // send buffer (only used by root)
        sendcounts,   // how many to send to each process
        displs,       // starting offset for each process
        MPI_INT,      // send type
        chunk,        // receive buffer
        recv_size,    // how many THIS process will receive
        MPI_INT,      // receive type
        0,            // root rank
        MPI_COMM_WORLD
    );

    // Each process prints what it got
    printf("Rank %d received %d elements: ", rank, recv_size);
    for (int i = 0; i < recv_size; i++) {
        printf("%d ", chunk[i]);
    }
    printf("\n");

    // Each process computes its own local sum
    int local_sum = 0;
    for (int i = 0; i < recv_size; i++) {
        local_sum += chunk[i];
    }
    printf("Rank %d local sum = %d\n", rank, local_sum);

    MPI_Finalize();
    return 0;
}