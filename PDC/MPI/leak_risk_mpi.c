/*
 * ============================================================
 *  Distributed Leak-Risk Propagation — MPI + Checkpoint/Restart
 *  CS-3006 Open-Book Quiz
 * ============================================================
 *
 * BEGINNER'S GUIDE TO MPI (read this first!)
 * -------------------------------------------
 * MPI = Message Passing Interface.
 * Think of it like running the SAME program on several workers
 * (called "ranks") at the same time.  Each worker owns a chunk
 * of rows and they talk to each other by sending messages.
 *
 *   Rank 0  |  Rank 1  |  Rank 2  |  Rank 3
 *  rows 0-2 | rows 3-5 | rows 6-8 | rows 9-11
 *
 * Key MPI calls used here:
 *   MPI_Init          - start MPI
 *   MPI_Comm_rank     - "what is my worker ID (rank)?"
 *   MPI_Comm_size     - "how many workers total?"
 *   MPI_Sendrecv      - point-to-point combined send+receive (no deadlock)
 *   MPI_Allreduce     - every rank sends a value; every rank gets back the result
 *   MPI_Scatterv      - rank 0 splits the grid and sends each piece to its rank
 *   MPI_Gatherv       - every rank sends its piece back to rank 0
 *   MPI_Bcast         - rank 0 sends ONE value to ALL other ranks
 *   MPI_Finalize      - shut down MPI cleanly
 *
 * HALO ROWS (ghost rows)
 * ----------------------
 * When rank 1 needs to update its top row it needs the bottom
 * row of rank 0.  We borrow that row and store it in a "halo"
 * slot above our real data before each iteration.
 *
 *   [ halo_top  ]   <- borrowed last row from rank above
 *   [ my row 0  ]
 *   [ my row 1  ]
 *        ...
 *   [ my row n  ]
 *   [ halo_bot  ]   <- borrowed first row from rank below
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <mpi.h>

/* ---- tuneable constants ---- */
#define CKPT_INTERVAL 10
#define CKPT_FMT      "ckpt_rank_%d.bin"

/* ===========================================================
 *  CHECKPOINT WRITE
 *  Saves: [int iter][double * local_rows * N]
 * =========================================================== */
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

/* ===========================================================
 *  CHECKPOINT READ
 *  Returns saved iteration number, or -1 if no file exists.
 * =========================================================== */
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

/* ===========================================================
 *  GRID INITIALISATION  (called only on rank 0)
 * =========================================================== */
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

/* ===========================================================
 *  COUNT BLOCKED NEIGHBOURS of cell at buffer-row li, column j.
 * =========================================================== */
static int blocked_neighbors(const double *buf, int N, int li, int j)
{
    int c = 0;
    if (buf[(li-1)*N + j]   < 0) c++;
    if (buf[(li+1)*N + j]   < 0) c++;
    if (buf[ li   *N+(j-1)] < 0) c++;
    if (buf[ li   *N+(j+1)] < 0) c++;
    return c;
}

/* ===========================================================
 *  HALO EXCHANGE  — THE KEY FIX IS THE TAGS
 *
 *  Original code used tag 0 for both sends.  That means
 *  rank 1 was sending tag-0 to rank 0 AND rank 2 at the
 *  same time, while rank 0 was also listening for tag-0
 *  from rank 1.  The messages got confused and everyone hung.
 *
 *  Fix: use different tags for upward vs downward messages:
 *    tag 0 = "I am sending you my TOP row"
 *    tag 1 = "I am sending you my BOTTOM row"
 *
 *  So rank 1 sends real_top (tag 0) to rank 0,
 *  and rank 0 receives it as halo_bot (tag 0). Perfectly matched.
 * =========================================================== */
void halo_exchange(double *buf, int local_rows, int N, int rank, int size)
{
    double *halo_top = buf;
    double *halo_bot = buf + (local_rows + 1) * N;
    double *real_top = buf + 1          * N;
    double *real_bot = buf + local_rows * N;

    int up   = (rank > 0)      ? rank-1 : MPI_PROC_NULL;
    int down = (rank < size-1) ? rank+1 : MPI_PROC_NULL;

    /*
     * Send real_top UP with tag 0.
     * The rank above receives this as its halo_bot — it listens for tag 0 from below.
     */
    MPI_Sendrecv(real_top, N, MPI_DOUBLE, up,   0,
                 halo_bot, N, MPI_DOUBLE, down, 0,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    /*
     * Send real_bot DOWN with tag 1.
     * The rank below receives this as its halo_top — it listens for tag 1 from above.
     */
    MPI_Sendrecv(real_bot, N, MPI_DOUBLE, down, 1,
                 halo_top, N, MPI_DOUBLE, up,   1,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

/* ===========================================================
 *  MAIN
 * =========================================================== */
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

    /* ====================================================
     *  DECOMPOSITION — split M rows across 'size' ranks
     * ==================================================== */
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

    /* Scatterv/Gatherv need element counts */
    int *sc = (int*)malloc(size * sizeof(int));
    int *ds = (int*)malloc(size * sizeof(int));
    for (int r = 0; r < size; r++) {
        sc[r] = rowcounts[r] * N;
        ds[r] = rowdispls[r] * N;
    }

    int local_rows = rowcounts[rank];

    /* ====================================================
     *  LOCAL BUFFER:  (local_rows + 2) rows
     *  row 0            = halo_top
     *  rows 1..local_rows = real data
     *  row local_rows+1 = halo_bot
     * ==================================================== */
    int buf_total = (local_rows + 2) * N;
    double *A = (double*)calloc(buf_total, sizeof(double));
    double *B = (double*)calloc(buf_total, sizeof(double));
    if (!A || !B) {
        fprintf(stderr, "Rank %d: allocation failed\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    double *A_real = A + N;   /* pointer to row 1 (first real row) */

    /* ====================================================
     *  RANK 0: initialise and scatter
     * ==================================================== */
    double *full_grid = NULL;
    if (rank == 0) {
        full_grid = (double*)malloc((size_t)M * N * sizeof(double));
        if (!full_grid) { fprintf(stderr,"alloc failed\n"); MPI_Abort(MPI_COMM_WORLD,1); }
        init_grid(full_grid, M, N);
    }

    MPI_Scatterv(full_grid, sc, ds, MPI_DOUBLE,
                 A_real, local_rows * N, MPI_DOUBLE,
                 0, MPI_COMM_WORLD);

    /* ====================================================
     *  CHECKPOINT RESTART
     * ==================================================== */
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

    /* ====================================================
     *  MAIN LOOP
     * ==================================================== */
    int iter = start_iter;
    for (; iter < max_iters; iter++) {

        /* 1. Halo exchange with neighbours */
        halo_exchange(A, local_rows, N, rank, size);

        /* 2. Compute new values */
        double local_diff = 0.0;

        for (int li = 1; li <= local_rows; li++) {
            int gi = rowdispls[rank] + (li - 1);  /* global row */

            for (int j = 0; j < N; j++) {
                double old_val = A[li*N + j];

                /* fixed cells — copy as-is */
                if (j == 0 || j == N-1)  { B[li*N+j] = old_val; continue; }
                if (gi == 0 || gi == M-1){ B[li*N+j] = old_val; continue; }
                if (old_val < 0.0)       { B[li*N+j] = old_val; continue; }
                if (old_val == 100.0)    { B[li*N+j] = old_val; continue; }

                /* update rule */
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

        /* 3. Copy B back to A (non-fixed cells only) */
        for (int li = 1; li <= local_rows; li++) {
            int gi = rowdispls[rank] + (li - 1);
            if (gi == 0 || gi == M-1) continue;
            for (int j = 1; j < N-1; j++) {
                double v = A[li*N+j];
                if (v >= 0.0 && v != 100.0)
                    A[li*N+j] = B[li*N+j];
            }
        }

        /* 4. Global convergence — all ranks share their max diff */
        double global_diff = 0.0;
        MPI_Allreduce(&local_diff, &global_diff, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
        if (global_diff < eps) { iter++; break; }

        /* 5. Checkpoint */
        if ((iter + 1) % CKPT_INTERVAL == 0)
            checkpoint_write(rank, iter, A_real, local_rows, N);
    }

    /* ====================================================
     *  GATHER results to rank 0
     * ==================================================== */
    MPI_Gatherv(A_real, local_rows * N, MPI_DOUBLE,
                full_grid, sc, ds, MPI_DOUBLE,
                0, MPI_COMM_WORLD);

    /* ====================================================
     *  FINAL STATISTICS (rank 0 only)
     * ==================================================== */
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