#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define DATA_SIZE 12

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (DATA_SIZE % size != 0) {
        if (rank == 0) {
            fprintf(stderr, "DATA_SIZE must be divisible by the number of processes.\n");
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int local_size = DATA_SIZE / size;
    int *local_data = (int *)malloc(local_size * sizeof(int));
    int *global_data = NULL;
    int *send_counts = NULL;
    int *displs = NULL;

    if (rank == 0) {
        global_data = (int *)malloc(DATA_SIZE * sizeof(int));
        send_counts = (int *)malloc(size * sizeof(int));
        displs = (int *)malloc(size * sizeof(int));

        printf("Root process initialized global data: ");
        for (int i = 0; i < DATA_SIZE; i++) {
            global_data[i] = i + 1;
            printf("%d ", global_data[i]);
        }
        printf("\n");

        printf("Root process preparing send_counts and displs for Scatterv:\n");
        for (int i = 0; i < size; i++) {
            send_counts[i] = local_size;
            displs[i] = i * local_size;
        }

        for (int i = 0; i < size; i++)
        {
            printf("Process %d: send_counts[%d] = %d, displs[%d] = %d\n", rank, i, send_counts[i], i, displs[i]);
        }
        printf("\n");
    }

    MPI_Scatterv(global_data, send_counts, displs, MPI_INT, local_data, local_size, MPI_INT, 0, MPI_COMM_WORLD);

    printf("Process %d received local data: ", rank);

    for (int i = 0; i < local_size; i++) {
        printf("%d ", local_data[i]);
        local_data[i] *= (rank + 1);
    }
    printf("\n");

    printf("Process %d modified local data: ", rank);
    for (int i = 0; i < local_size; i++) {
        printf("%d ", local_data[i]);
    }
    printf("\n");


    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Gatherv(local_data, local_size, MPI_INT, global_data, send_counts, displs, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("Final result gathered by root process:\n");
        for (int i = 0; i < DATA_SIZE; i++) {
            printf("%d ", global_data[i]);
        }
        printf("\n");
    }

    int *send_buffer = (int *)malloc(size * sizeof(int));
    int *recv_buffer = (int *)malloc(size * sizeof(int));

    for (int i = 0; i < size; i++) {
        send_buffer[i] = rank + 1;
    }

    MPI_Alltoall(send_buffer, 1, MPI_INT, recv_buffer, 1, MPI_INT, MPI_COMM_WORLD);

    printf("Process %d received : ", rank);
    for (int i = 0; i < size; i++) {
        printf("%d ", recv_buffer[i]);
    }
    printf("\n");

    MPI_Finalize();
    return 0;
}
