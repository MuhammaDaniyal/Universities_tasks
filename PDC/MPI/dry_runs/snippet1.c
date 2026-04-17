#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
	int rank, size;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	int *sendBuf = (int *)malloc(2 * sizeof(int));
	sendBuf[0] = 5;
	sendBuf[1] = 7;
	int recv[2] = {0, -1};

	if (rank == 0) {
		MPI_Send(sendBuf, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
		MPI_Send(sendBuf + 1, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
		printf("Both of the data send\n");
	} else if (rank == 1) {
		sleep(5);
        printf("Rank %d is ready to receive data\n", rank);

        MPI_Recv(recv, 2, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Data received in rank %d, data = %d, %d\n", rank, recv[0], recv[1]);

        MPI_Recv(recv, 2, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		printf("Data received in rank %d, data = %d, %d\n", rank, recv[0], recv[1]);
	}
	else if (rank == 2) {
		MPI_Recv(&recv, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		printf("Data received in rank %d, data = %d, %d\n", rank, recv[0], recv[1]);
	}

	MPI_Finalize();
	free(sendBuf);
	return 0;
}