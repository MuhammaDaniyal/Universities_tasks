#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NUM_TASKS 5
#define TASK_DATA_SIZE 3
#define TERMINATION_TAG 2

int main(int argc, char **argv) {
	MPI_Init(&argc, &argv);

	int rank, size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	if (size < 2) {
		fprintf(stderr, "This program requires at least 2 processes.\n");
		MPI_Abort(MPI_COMM_WORLD, 1);
	}

	if (rank == 0) {
		int task_data[NUM_TASKS][TASK_DATA_SIZE] = {
			{1, 2, 3},
			{4, 5, 6},
			{7, 8, 9},
			{10, 11, 12},
			{13, 14, 15}
		};

		MPI_Request send_requests[NUM_TASKS];
		MPI_Request recv_requests[NUM_TASKS];
		int recv_buffer[NUM_TASKS][TASK_DATA_SIZE];
		int completed_tasks = 0;

		for (int i = 0; i < NUM_TASKS; i++) {
			int worker_rank = (i % (size - 1)) + 1;
			MPI_Isend(task_data[i], TASK_DATA_SIZE, MPI_INT, worker_rank, 0, MPI_COMM_WORLD, &send_requests[i]);
			printf("Master sent task %d to worker %d\n", i, worker_rank);
		}

		for (int i = 0; i < NUM_TASKS; i++) {
			MPI_Irecv(recv_buffer[i], TASK_DATA_SIZE, MPI_INT, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &recv_requests[i]);
		}

		while (completed_tasks < NUM_TASKS) {
			int index;
			MPI_Status status;
			MPI_Waitany(NUM_TASKS, recv_requests, &index, &status);

			int count;
			MPI_Get_count(&status, MPI_INT, &count);

			printf("[Master] received result from worker %d: ", status.MPI_SOURCE);
			for (int j = 0; j < count; j++) {
				printf("%d ", recv_buffer[index][j]);
			}
			printf("\n");
			completed_tasks++;
		}

		MPI_Waitall(NUM_TASKS, send_requests, MPI_STATUSES_IGNORE);

		for (int worker = 1; worker < size; worker++) {
			MPI_Send(NULL, 0, MPI_INT, worker, TERMINATION_TAG, MPI_COMM_WORLD);
			printf("[Master] sent termination message to worker %d\n", worker);
		}
	} else {

        sleep(3); 
		MPI_Status status;
		int task_data[TASK_DATA_SIZE];
		int result_data[TASK_DATA_SIZE];

		while (1) {
			MPI_Probe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

			if (status.MPI_TAG == TERMINATION_TAG) {
				MPI_Recv(NULL, 0, MPI_INT, 0, TERMINATION_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				printf("Worker %d received termination signal. Exiting.\n", rank);
				break;
			}

			int count;
			MPI_Get_count(&status, MPI_INT, &count);
			MPI_Recv(task_data, count, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

			printf("Worker %d received task : ", rank);
			for (int i = 0; i < count; i++) {
				printf("%d ", task_data[i]);
			}
			printf("\n");

			for (int i = 0; i < count; i++) {
				result_data[i] = task_data[i] * 2;
			}

			MPI_Request send_request;
			MPI_Isend(result_data, count, MPI_INT, 0, 1, MPI_COMM_WORLD, &send_request);

			int flag;
			MPI_Test(&send_request, &flag, MPI_STATUS_IGNORE);
			if (flag) {
				printf("Worker %d finished sending result.\n", rank);
			} else {
				MPI_Wait(&send_request, MPI_STATUS_IGNORE);
				printf("Worker %d finished sending result after waiting.\n", rank);
			}
		}
	}

	MPI_Finalize();
	return 0;
}