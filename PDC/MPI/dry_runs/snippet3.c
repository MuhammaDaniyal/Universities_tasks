#include <mpi.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	MPI_Init(&argc, &argv);

	int rank, size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	int *send_data = (int *)malloc(size * sizeof(int));
	int *recv_data = (int *)malloc(size * sizeof(int));

	for (int i = 0; i < size; i++) {
		send_data[i] = i + rank * 10;
	}

	printf("Process %d : ", rank);
	for (int i = 0; i < size; i++) 
		printf("%d ", send_data[i]);
	printf("\n");

	MPI_Request *send_requests = (MPI_Request *)malloc(size * sizeof(MPI_Request));

	for (int i = 0; i < size; i++) {
		MPI_Isend(&send_data[i], 1, MPI_INT, i, 0, MPI_COMM_WORLD, &send_requests[i]);
	}
	for (int i = 0; i < size; i++) {
		MPI_Recv(&recv_data[i], 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	}
    sleep(1);
	MPI_Waitall(size, send_requests, MPI_STATUSES_IGNORE);

	printf("Process %d received : ", rank);
	for (int i = 0; i < size; i++) {
		printf("%d ", recv_data[i]);
	}

	printf("\n");

	MPI_Finalize();
	return 0;
}