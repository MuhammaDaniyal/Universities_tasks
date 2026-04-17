#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define ARRAY_SIZE 16

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int chunk_size = ARRAY_SIZE / size;
    int *data1 = (int *)malloc(ARRAY_SIZE * sizeof(int));
    int *data2 = (int *)malloc(ARRAY_SIZE * sizeof(int));
    int *sub_data1 = (int *)malloc(chunk_size * sizeof(int));
    int *sub_data2 = (int *)malloc(chunk_size * sizeof(int));

    if (rank == 0)
    {
        data1 = (int *)malloc(ARRAY_SIZE * sizeof(int));
        data2 = (int *)malloc(ARRAY_SIZE * sizeof(int));
        for (int i = 0; i < ARRAY_SIZE; i++)
        {
            data1[i] = i + 1;
            data2[i] = (i + 1) * 3;
        }
        printf(" Root initialized data1 : ");
        for (int i = 0; i < ARRAY_SIZE; i++)
            printf("%d ", data1[i]);
        printf("\n");

        printf(" Root initialized data2 : ");
        for (int i = 0; i < ARRAY_SIZE; i++)
            printf("%d ", data2[i]);
        printf("\n");
    }

    MPI_Scatter(data1, chunk_size, MPI_INT, sub_data1, chunk_size, MPI_INT, 0, MPI_COMM_WORLD);

    int threshold = 10;
    MPI_Bcast(&threshold, 1, MPI_INT, 0, MPI_COMM_WORLD);

    for (int i = 0; i < chunk_size; i++)
    {
        if (sub_data1[i] > threshold)
            sub_data1[i] *= 2;
    }

    MPI_Gather(sub_data1, chunk_size, MPI_INT, data1, chunk_size, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0)
    {
        printf(" Root gathered modified data1 : ");
        for (int i = 0; i < ARRAY_SIZE; i++)
            printf("%d ", data1[i]);
        printf("\n");
    }

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Bcast(data1, ARRAY_SIZE, MPI_INT, 0, MPI_COMM_WORLD);

    MPI_Scatter(data2, chunk_size, MPI_INT, sub_data2, chunk_size, MPI_INT, 0, MPI_COMM_WORLD);
    int local_sum1 = 0, local_sum2 = 0;
    for (int i = 0; i < chunk_size; i++)
    {
        local_sum1 += sub_data1[i];
        local_sum2 += sub_data2[i];
    }

    int total_sum1 = 0, total_sum2 = 0;
    MPI_Reduce(&local_sum1, &total_sum1, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_sum2, &total_sum2, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    MPI_Bcast(&total_sum1, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&total_sum2, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int recv_val;
    MPI_Request send_request, recv_request;
    int next = (rank + 1) % size;
    int prev = (rank - 1 + size) % size;

    MPI_Isend(&local_sum1, 1, MPI_INT, next, 0, MPI_COMM_WORLD, &send_request);
    MPI_Irecv(&recv_val, 1, MPI_INT, prev, 0, MPI_COMM_WORLD, &recv_request);

    MPI_Wait(&send_request, MPI_STATUS_IGNORE);
    MPI_Wait(&recv_request, MPI_STATUS_IGNORE);

    printf(" Process %d received value %d from process %d\n", rank, recv_val, prev);

    MPI_Finalize();
    return 0;
}