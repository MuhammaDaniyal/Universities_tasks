#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define ROOT 0

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int *data = NULL;
    int *recv_chunk = NULL;
    int *send_counts = NULL;
    int *displs = NULL;
    int *gather_displs = NULL;
    int *gather_counts = NULL;
    int *alltoall_send = NULL;
    int *alltoall_recv = NULL;

    int total_size = size * (size + 1) / 2;
    int recv_size = rank + 1;

    if (rank == ROOT) {
        data = (int *)malloc(total_size * sizeof(int));
        send_counts = (int *)malloc(size * sizeof(int));
        displs = (int *)malloc(size * sizeof(int));

        int index = 0;
        for (int i = 0; i < size; i++) {
            send_counts[i] = i + 1;
            displs[i] = index;
            index += send_counts[i];
        }

        for (int i = 0; i < total_size; i++) {
            data[i] = i + 1;
        }

        printf("Root initialized data: ");
        for (int i = 0; i < total_size; i++) {
            printf("%d ", data[i]);
        }
        printf("\n");
    }

    recv_chunk = (int *)malloc(recv_size * sizeof(int));

    MPI_Scatterv(data, send_counts, displs, MPI_INT, recv_chunk, recv_size, MPI_INT, ROOT, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);

    for (int i = 0; i < recv_size; i++) {
        recv_chunk[i] *= (rank + 2);
    }

    alltoall_send = (int *)malloc(size * sizeof(int));
    alltoall_recv = (int *)malloc(size * sizeof(int));

    for (int i = 0; i < size; i++) {
        alltoall_send[i] = rank * 100 + i;
    }

    printf("Process %d sending in Alltoall: ", rank);
    for (int i = 0; i < size; i++) {
        printf("%d ", alltoall_send[i]);   
    }
    printf("\n");

    MPI_Alltoall(alltoall_send, 1, MPI_INT, alltoall_recv, 1, MPI_INT, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);

    printf("Process %d received in Alltoall: ", rank);
    for (int i = 0; i < size; i++) {
        printf("%d ", alltoall_recv[i]);
    }
    printf("\n");

    if (rank == ROOT) {
        gather_counts = (int *)malloc(size * sizeof(int));
        gather_displs = (int *)malloc(size * sizeof(int));

        int index = 0;
        for (int i = 0; i < size; i++) {
            gather_counts[i] = i + 1;
            gather_displs[i] = index;
            index += gather_counts[i];
        }
    }

    MPI_Gatherv(recv_chunk, recv_size, MPI_INT, data, gather_counts, gather_displs, MPI_INT, ROOT, MPI_COMM_WORLD);

    if (rank == ROOT) {
        printf("Root gathered modified data: ");
        for (int i = 0; i < total_size; i++) {
            printf("%d ", data[i]);
        }
        printf("\n");
    }

    MPI_Finalize();
    return 0;
}
