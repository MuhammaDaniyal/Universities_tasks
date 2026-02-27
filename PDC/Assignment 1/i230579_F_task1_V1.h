/*
    Name: Muhammad Daniyal
    Roll Number: i230579
    Assignment: 2
*/

#ifndef SIMD_ONLY_H
#define SIMD_ONLY_H

#include <vector>
#include <immintrin.h> 

using namespace std;

int intersect_SIMD_only(vector<int>& Adj_A, vector<int>& Adj_B, int v)
{
    const int* a  = Adj_A.data();
    const int* b  = Adj_B.data();
    int na = (int)Adj_A.size();
    int nb = (int)Adj_B.size();
    int count = 0;

    int i = 0, j = 0;

    while (i < na && a[i] <= v) i++;
    while (j < nb && b[j] <= v) j++;

    while (i + 8 <= na && j + 8 <= nb)
    {
        int a_max = a[i+7];
        int b_max = b[j+7];

        __m256i va = _mm256_loadu_si256((__m256i*)(a + i));
        __m256i vb = _mm256_loadu_si256((__m256i*)(b + j));

        __m256i b0 = _mm256_set1_epi32(b[j+0]);
        __m256i b1 = _mm256_set1_epi32(b[j+1]);
        __m256i b2 = _mm256_set1_epi32(b[j+2]);
        __m256i b3 = _mm256_set1_epi32(b[j+3]);
        __m256i b4 = _mm256_set1_epi32(b[j+4]);
        __m256i b5 = _mm256_set1_epi32(b[j+5]);
        __m256i b6 = _mm256_set1_epi32(b[j+6]);
        __m256i b7 = _mm256_set1_epi32(b[j+7]);

        __m256i c0 = _mm256_cmpeq_epi32(va, b0);
        __m256i c1 = _mm256_cmpeq_epi32(va, b1);
        __m256i c2 = _mm256_cmpeq_epi32(va, b2);
        __m256i c3 = _mm256_cmpeq_epi32(va, b3);
        __m256i c4 = _mm256_cmpeq_epi32(va, b4);
        __m256i c5 = _mm256_cmpeq_epi32(va, b5);
        __m256i c6 = _mm256_cmpeq_epi32(va, b6);
        __m256i c7 = _mm256_cmpeq_epi32(va, b7);

        __m256i any = _mm256_or_si256(
                        _mm256_or_si256(_mm256_or_si256(c0,c1), _mm256_or_si256(c2,c3)),
                        _mm256_or_si256(_mm256_or_si256(c4,c5), _mm256_or_si256(c6,c7))
                      );

        count += __builtin_popcount(_mm256_movemask_epi8(any)) / 4;

        if (a_max < b_max)      i += 8;
        else if (b_max < a_max) j += 8;
        else { i += 8; j += 8; }
    }

    while (i < na && j < nb)
    {
        if (a[i] <= v) { i++; continue; }
        if (b[j] <= v) { j++; continue; }
        if      (a[i] < b[j]) i++;
        else if (a[i] > b[j]) j++;
        else { count++; i++; j++; }
    }

    return count;
}

#endif