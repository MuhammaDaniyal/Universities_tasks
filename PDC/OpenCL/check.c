#define CL_TARGET_OPENCL_VERSION 300
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define M 176
#define N 128
#define K 144

int main() {
    float* Q = (float*)malloc(M*K*sizeof(float));
    float* D = (float*)malloc(N*K*sizeof(float));
    float* S_ref = (float*)malloc(M*N*sizeof(float));

    for (int i = 0; i < M; i++)
        for (int k = 0; k < K; k++)
            Q[i*K+k] = ((i+k) % 17) * 0.1f;

    for (int j = 0; j < N; j++)
        for (int k = 0; k < K; k++)
            D[j*K+k] = ((j+2*k) % 19) * 0.1f;

    for (int i = 0; i < M; i++)
        for (int j = 0; j < N; j++) {
            float sum = 0.0f;
            for (int k = 0; k < K; k++)
                sum += Q[i*K+k] * D[j*K+k];
            S_ref[i*N+j] = sum;
        }

    // Print first few reference values and max value
    printf("First few S_ref values:\n");
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            printf("  S_ref[%d][%d] = %.6f\n", i, j, S_ref[i*N+j]);

    float maxval = 0;
    for (int i = 0; i < M*N; i++)
        if (S_ref[i] > maxval) maxval = S_ref[i];
    printf("Max value in S: %.4f\n", maxval);

    free(Q); free(D); free(S_ref);
    return 0;
}
