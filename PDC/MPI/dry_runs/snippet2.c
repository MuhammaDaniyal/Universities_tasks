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
	int recv[2] = {-1, -1};

	if (rank == 0) {
		MPI_Send(sendBuf, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
		printf("Data sent from rank %d\n", rank);
	} else if (rank == 1) {
		// sleep(5);
		MPI_Status status;
		int dataReceived;
		MPI_Recv(recv, 3, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
		MPI_Get_count(&status, MPI_INT, &dataReceived);
		printf("Count of data received = %d\n", dataReceived);
		printf("Data received in rank %d, data = %d, %d\n", rank, recv[0], recv[1]);
	}
    else {
        printf("Rank %d is saying FUCK YOU\n", rank);
    }

	MPI_Finalize();
	free(sendBuf);
	return 0;
}