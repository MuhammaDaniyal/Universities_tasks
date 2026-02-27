#ifndef NAIVE_H
#define NAIVE_H

#include <vector>

using namespace std;

int intersect_naive(vector<int>& Adj_A, vector<int>& Adj_B, int v)
{
    int i = 0, j = 0, count = 0;

    while(i < Adj_A.size() && j < Adj_B.size())
    {
        if (Adj_A[i] <= v) { i++; continue; }
        if (Adj_B[j] <= v) { j++; continue; }
        
        if(Adj_A[i] < Adj_B[j])
        {
            i++;
        }
        else if(Adj_A[i] > Adj_B[j])
        {
            j++;
        }
        else
        {
            i++;
            j++;
            count++;
        }
    }

    return count;
}

#endif