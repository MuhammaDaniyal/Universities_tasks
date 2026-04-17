#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define DATA_SIZE 12
#define CONFIG_VALUE 5

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
    int config_value;

    if (rank == 0) {
        global_data = (int *)malloc(DATA_SIZE * sizeof(int));
        printf("Root process initialized global data: ");
        for (int i = 0; i < DATA_SIZE; i++) {
            global_data[i] = i + 1;
            printf("%d ", global_data[i]);
        }
        printf("\n");
    }

    MPI_Bcast(&config_value, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (rank == 0) {
        config_value = CONFIG_VALUE;
    }
    MPI_Bcast(&config_value, 1, MPI_INT, 0, MPI_COMM_WORLD);
    printf("Process %d received config_value = %d\n", rank, config_value);

    MPI_Scatter(global_data, local_size, MPI_INT, local_data, local_size, MPI_INT, 0, MPI_COMM_WORLD);

    for (int i = 0; i < local_size; i++) {
        local_data[i] *= config_value;
    }

    MPI_Gather(local_data, local_size, MPI_INT, global_data, local_size, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("Final result gathered by root process:\n");
        for (int i = 0; i < DATA_SIZE; i++) {
            printf("%d ", global_data[i]);
        }
        printf("\n");
    }

    MPI_Finalize();
    return 0;
}
