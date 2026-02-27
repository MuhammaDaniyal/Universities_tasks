#ifndef OPENMP_SIMD_H
#define OPENMP_SIMD_H

#include <vector>

using namespace std;

int intersect_omp_simd(vector<int>& Adj_A, vector<int>& Adj_B, int v)
{
    const int* a = Adj_A.data();
    const int* b = Adj_B.data();
    int na = (int)Adj_A.size();
    int nb = (int)Adj_B.size();
    int count = 0;
    int i = 0, j = 0;

    while (i < na && a[i] <= v) i++;
    while (j < nb && b[j] <= v) j++;

    // SIMD zone: while both have >= 8 elements
    while (i + 8 <= na && j + 8 <= nb)
    {
        int a_max = a[i + 7];
        int b_max = b[j + 7];

        // For each of B's 8 elements, compare against A's 8 elements
        // Compiler vectorizes this 8-iteration loop using SIMD
        for (int bk = 0; bk < 8; bk++)
        {
            int bval = b[j + bk];
            int local_count = 0;

            // This inner loop is the one that gets vectorized
            #pragma omp simd reduction(+:local_count)
            for (int ak = 0; ak < 8; ak++)
            {
                local_count += (a[i + ak] == bval) ? 1 : 0;
            }

            count += local_count;
        }

        // Same pointer advancement as intrinsic version
        if      (a_max < b_max) i += 8;
        else if (b_max < a_max) j += 8;
        else                  { i += 8; j += 8; }
    }

    // Scalar tail
    while (i < na && j < nb)
    {
        if      (a[i] < b[j]) i++;
        else if (a[i] > b[j]) j++;
        else { count++; i++; j++; }
    }

    return count;
}

#endif