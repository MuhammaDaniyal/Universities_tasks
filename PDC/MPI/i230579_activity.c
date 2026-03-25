#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <mpi.h>

#define CKPT_INTERVAL 10
#define CKPT_FMT      "ckpt_rank_%d.bin"

void checkpoint_write(int rank, int iter, double *local, int local_rows, int N)
{
    char fname[64];
    snprintf(fname, sizeof(fname), CKPT_FMT, rank);
    FILE *f = fopen(fname, "wb");
    if (!f) { fprintf(stderr, "Rank %d: cannot write checkpoint\n", rank); return; }
    fwrite(&iter, sizeof(int),    1,                    f);
    fwrite(local, sizeof(double), (size_t)local_rows*N, f);
    fclose(f);
}

int checkpoint_read(int rank, double *local, int local_rows, int N)
{
    char fname[64];
    snprintf(fname, sizeof(fname), CKPT_FMT, rank);
    FILE *f = fopen(fname, "rb");
    if (!f) return -1;

    int saved_iter = -1;
    (void)fread(&saved_iter, sizeof(int),    1,                    f);
    (void)fread(local,       sizeof(double), (size_t)local_rows*N, f);
    fclose(f);
    return saved_iter;
}

void init_grid(double *A, int M, int N)
{
    for (int i = 0; i < M; i++)
        for (int j = 0; j < N; j++)
            A[i*N+j] = (i==0 || i==M-1 || j==0 || j==N-1) ? 100.0 : 0.0;

    if (M > 6 && N > 6) {
        A[3*N+3] = -1.0;
        A[3*N+4] = -1.0;
        A[4*N+3] = -1.0;
    }
}

static int blocked_neighbors(const double *buf, int N, int li, int j)
{
    int c = 0;
    if (buf[(li-1)*N + j]   < 0) c++;
    if (buf[(li+1)*N + j]   < 0) c++;
    if (buf[ li   *N+(j-1)] < 0) c++;
    if (buf[ li   *N+(j+1)] < 0) c++;
    return c;
}

void halo_exchange(double *buf, int local_rows, int N, int rank, int size)
{
    double *halo_top = buf;
    double *halo_bot = buf + (local_rows + 1) * N;
    double *real_top = buf + 1          * N;
    double *real_bot = buf + local_rows * N;

    int up   = (rank > 0)      ? rank-1 : MPI_PROC_NULL;
    int down = (rank < size-1) ? rank+1 : MPI_PROC_NULL;

    MPI_Sendrecv(real_top, N, MPI_DOUBLE, up,   0,
                 halo_bot, N, MPI_DOUBLE, down, 0,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    MPI_Sendrecv(real_bot, N, MPI_DOUBLE, down, 1,
                 halo_top, N, MPI_DOUBLE, up,   1,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int    M         = (argc > 1) ? atoi(argv[1]) : 8;
    int    N         = (argc > 2) ? atoi(argv[2]) : 8;
    int    max_iters = (argc > 3) ? atoi(argv[3]) : 200;
    double eps       = (argc > 4) ? atof(argv[4]) : 1e-3;
    double THRESHOLD = (argc > 5) ? atof(argv[5]) : 60.0;

    if (rank == 0)
        printf("Grid %dx%d  ranks=%d  max_iters=%d  eps=%.2e  threshold=%.1f\n",
               M, N, size, max_iters, eps, THRESHOLD);

    int *rowcounts = (int*)malloc(size * sizeof(int));
    int *rowdispls = (int*)malloc(size * sizeof(int));
    {
        int base = M / size, rem = M % size, off = 0;
        for (int r = 0; r < size; r++) {
            rowcounts[r] = base + (r < rem ? 1 : 0);
            rowdispls[r] = off;
            off += rowcounts[r];
        }
    }

    int *sc = (int*)malloc(size * sizeof(int));
    int *ds = (int*)malloc(size * sizeof(int));
    for (int r = 0; r < size; r++) {
        sc[r] = rowcounts[r] * N;
        ds[r] = rowdispls[r] * N;
    }

    int local_rows = rowcounts[rank];

    int buf_total = (local_rows + 2) * N;
    double *A = (double*)calloc(buf_total, sizeof(double));
    double *B = (double*)calloc(buf_total, sizeof(double));
    if (!A || !B) {
        fprintf(stderr, "Rank %d: allocation failed\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    double *A_real = A + N;

    double *full_grid = NULL;
    if (rank == 0) {
        full_grid = (double*)malloc((size_t)M * N * sizeof(double));
        if (!full_grid) { fprintf(stderr,"alloc failed\n"); MPI_Abort(MPI_COMM_WORLD,1); }
        init_grid(full_grid, M, N);
    }

    MPI_Scatterv(full_grid, sc, ds, MPI_DOUBLE,
                 A_real, local_rows * N, MPI_DOUBLE,
                 0, MPI_COMM_WORLD);

    int start_iter = 0;
    {
        int my_saved = checkpoint_read(rank, A_real, local_rows, N);
        int ref = (rank == 0) ? my_saved : -1;
        MPI_Bcast(&ref, 1, MPI_INT, 0, MPI_COMM_WORLD);
        if (ref >= 0) {
            start_iter = ref + 1;
            if (rank == 0)
                printf("[restart] Resuming from iter %d\n", start_iter);
        }
    }

    int iter = start_iter;
    for (; iter < max_iters; iter++) {

        halo_exchange(A, local_rows, N, rank, size);
        double local_diff = 0.0;

        for (int li = 1; li <= local_rows; li++) {
            int gi = rowdispls[rank] + (li - 1);

            for (int j = 0; j < N; j++) {
                double old_val = A[li*N + j];

                if (j == 0 || j == N-1)  { B[li*N+j] = old_val; continue; }
                if (gi == 0 || gi == M-1){ B[li*N+j] = old_val; continue; }
                if (old_val < 0.0)       { B[li*N+j] = old_val; continue; }
                if (old_val == 100.0)    { B[li*N+j] = old_val; continue; }

                double nv =
                    0.40 * A[(li-1)*N + j  ] +
                    0.20 * A[(li+1)*N + j  ] +
                    0.15 * A[ li   *N+(j-1)] +
                    0.15 * A[ li   *N+(j+1)] +
                    0.10 * old_val;

                if (blocked_neighbors(A, N, li, j) >= 2)
                    nv *= 0.8;

                B[li*N + j] = nv;
                double d = fabs(nv - old_val);
                if (d > local_diff) local_diff = d;
            }
        }

        for (int li = 1; li <= local_rows; li++) {
            int gi = rowdispls[rank] + (li - 1);
            if (gi == 0 || gi == M-1) continue;
            for (int j = 1; j < N-1; j++) {
                double v = A[li*N+j];
                if (v >= 0.0 && v != 100.0)
                    A[li*N+j] = B[li*N+j];
            }
        }

        double global_diff = 0.0;
        MPI_Allreduce(&local_diff, &global_diff, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
        if (global_diff < eps) { iter++; break; }

        if ((iter + 1) % CKPT_INTERVAL == 0)
            checkpoint_write(rank, iter, A_real, local_rows, N);
    }

    MPI_Gatherv(A_real, local_rows * N, MPI_DOUBLE,
                full_grid, sc, ds, MPI_DOUBLE,
                0, MPI_COMM_WORLD);

    if (rank == 0) {
        double checksum = 0.0, maxv = 0.0;
        int alarms = 0;
        for (int k = 0; k < M*N; k++) {
            if (full_grid[k] >= 0.0) {
                checksum += full_grid[k];
                if (full_grid[k] > maxv)       maxv = full_grid[k];
                if (full_grid[k] >= THRESHOLD) alarms++;
            }
        }
        printf("iters=%d max=%.6f alarms=%d checksum=%.6f\n",
               iter, maxv, alarms, checksum);
        free(full_grid);
    }

    free(A); free(B);
    free(rowcounts); free(rowdispls);
    free(sc); free(ds);

    MPI_Finalize();
    return 0;
}