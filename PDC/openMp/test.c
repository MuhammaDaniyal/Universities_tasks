#include <omp.h>
#include <stdio.h>

int main() {

    int x = 40;

    #pragma omp parallel for reduction(-:x) num_threads(5) 
    for(int i=0;i<5;i++)
    {
        printf("Thread %d initial private x = %d\n",
            omp_get_thread_num(), x);

        x += 1;

        printf("Thread %d after increment private x = %d\n",
            omp_get_thread_num(), x);
    }

    printf("Final value of x: %d\n", x);
}