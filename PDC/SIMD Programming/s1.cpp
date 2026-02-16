// basic_simd_add.cpp - Our first real SIMD program
#include <iostream>
#include <immintrin.h>  // SIMD instructions
#include <chrono>       // For timing

int main() {
    // ===========================================
    // STEP 1: Create our data (aligned for SIMD!)
    // ===========================================
    const int N = 8;  // Using 8 elements to match AVX (8 floats)
    
    // alignas(32) means: "put this at an address divisible by 32"
    // AVX needs 32-byte alignment for fastest loads!
    alignas(32) float a[N] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
    alignas(32) float b[N] = {8.0, 7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0};
    alignas(32) float c[N] = {0};  // Result array
    
    // ===========================================
    // STEP 2: Print input arrays
    // ===========================================
    std::cout << "Array A: ";
    for(int i = 0; i < N; i++) std::cout << a[i] << " ";
    std::cout << std::endl;
    
    std::cout << "Array B: ";
    for(int i = 0; i < N; i++) std::cout << b[i] << " ";
    std::cout << std::endl;
    
    // ===========================================
    // STEP 3: The SIMD part (THIS IS THE MAGIC!)
    // ===========================================
    
    // __m256 = a variable that holds 8 floats inside CPU
    __m256 vec_a, vec_b, vec_c;
    
    // Load 8 floats from a[] into the SIMD register
    // Think: "load packed single-precision" (ps)
    vec_a = _mm256_load_ps(a);  // vec_a now has [1,2,3,4,5,6,7,8]
    
    // Load 8 floats from b[] into another SIMD register
    vec_b = _mm256_load_ps(b);  // vec_b now has [8,7,6,5,4,3,2,1]
    
    // ADD ALL 8 PAIRS AT ONCE!
    // One instruction does 8 additions in parallel
    
    vec_c = _mm256_add_ps(vec_a, vec_b);
    // vec_c = _mm256_mul_ps(vec_a, vec_b);  // Multiply instead of add
    // vec_c = _mm256_sub_ps(vec_a, vec_b);  // Subtract
    // vec_c = _mm256_div_ps(vec_a, vec_b);  // Divide (slower, but works)
    
    __m256 five = _mm256_set1_ps(5.0);
    vec_c = _mm256_add_ps(vec_c, five);  // Add 5 to every element of c
    
    // Store the result back to c[] array
    _mm256_store_ps(c, vec_c);
    
    
    
    // ===========================================
    // STEP 4: Print the result
    // ===========================================
    std::cout << "Result C: ";
    for(int i = 0; i < N; i++) std::cout << c[i] << " ";
    std::cout << std::endl;
    
    return 0;
}