#include <stdio.h>
#include <stdlib.h>
#define CL_TARGET_OPENCL_VERSION 200
#include <CL/cl.h>

#define N 1000000
#define CHECK_ERROR(err, msg) \
    if (err != CL_SUCCESS) { \
        printf("%s (error code: %d)\n", msg, err); \
        exit(1); \
    }

const char* kernel_source = 
"__kernel void vector_add(__global const float* A,"
"                         __global const float* B,"
"                         __global float* C,"
"                         const int N)"
"{"
"    int gid = get_global_id(0);"
"    if (gid < N) {"
"        C[gid] = A[gid] + B[gid];"
"    }"
"}";

int main() {
    cl_int err;
    

    /////////////////////////////////////////////
    /////////// Create Data on CPU //////////////
    /////////////////////////////////////////////

    float *h_A = (float*)malloc(N * sizeof(float));
    float *h_B = (float*)malloc(N * sizeof(float));
    float *h_C = (float*)malloc(N * sizeof(float));
    
    for (int i = 0; i < N; i++) {
        h_A[i] = (float)i;
        h_B[i] = (float)i;
    }
    
    
    //////////////////////////////////////////////
    // OpenCL setup //////////////////////////////
    //////////////////////////////////////////////
    
    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_kernel kernel;
    cl_mem d_A, d_B, d_C;
    

    ///////////////////////////////////////////////////////////////////////
    // Finds GPU platform and device, falls back to CPU if no GPU found ///
    ///////////////////////////////////////////////////////////////////////

    err = clGetPlatformIDs(1, &platform, NULL);
    CHECK_ERROR(err, "Failed to get platform");
    
    // Get device
    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    if (err == CL_DEVICE_NOT_FOUND) {
        err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &device, NULL);
        CHECK_ERROR(err, "Failed to get device");
        printf("Using CPU device\n");
    } else {
        printf("Using GPU device\n");
    }
    
    // A workspace that holds all your GPU resources
    context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    CHECK_ERROR(err, "Failed to create context");
    
    // A FIFO line where you put commands in order
    queue = clCreateCommandQueue(context, device, 0, &err);
    CHECK_ERROR(err, "Failed to create command queue");
    
    ////////////////////////////////////////////////////////////////////////////
    // Create empty GPU memory /////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////

    d_A = clCreateBuffer(context, CL_MEM_READ_ONLY, N * sizeof(float), NULL, &err);
    CHECK_ERROR(err, "Failed to create buffer A");
    
    d_B = clCreateBuffer(context, CL_MEM_READ_ONLY, N * sizeof(float), NULL, &err);
    CHECK_ERROR(err, "Failed to create buffer B");
    
    d_C = clCreateBuffer(context, CL_MEM_WRITE_ONLY, N * sizeof(float), NULL, &err);
    CHECK_ERROR(err, "Failed to create buffer C");
    

    ////////////////////////////////////////////////////////////////////////////
    // Copy data to device
    ////////////////////////////////////////////////////////////////////////////
    
    err = clEnqueueWriteBuffer(queue, d_A, CL_TRUE, 0, N * sizeof(float), h_A, 0, NULL, NULL);
    CHECK_ERROR(err, "Failed to write buffer A");
    
    err = clEnqueueWriteBuffer(queue, d_B, CL_TRUE, 0, N * sizeof(float), h_B, 0, NULL, NULL);
    CHECK_ERROR(err, "Failed to write buffer B");
    

    ////////////////////////////////////////////////////////////////////////////
    // Create and build program
    ////////////////////////////////////////////////////////////////////////////

    program = clCreateProgramWithSource(context, 1, &kernel_source, NULL, &err);
    CHECK_ERROR(err, "Failed to create program");
    
    err = clBuildProgram(program, 1, &device, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        // Print build errors
        size_t log_size;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        char *log = (char*)malloc(log_size);
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
        printf("Build error:\n%s\n", log);
        free(log);
        exit(1);
    }
    
    // Create kernel
    kernel = clCreateKernel(program, "vector_add", &err);
    CHECK_ERROR(err, "Failed to create kernel");
    
    // Kernal arguments
    err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_A);
    err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_B);
    err |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &d_C);
    err |= clSetKernelArg(kernel, 3, sizeof(int), &n);
    CHECK_ERROR(err, "Failed to set kernel arguments");
    
    ////////////////////////////////////////////////////////////////////////////
    // Execute kernel
    ////////////////////////////////////////////////////////////////////////////

    size_t global_size = N;
    size_t local_size = 256;
    
    err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
    CHECK_ERROR(err, "Failed to enqueue kernel");
    
    ////////////////////////////////////////////////////////////////////////////
    // Read result
    ////////////////////////////////////////////////////////////////////////////

    err = clEnqueueReadBuffer(queue, d_C, CL_TRUE, 0, N * sizeof(float), h_C, 0, NULL, NULL);
    CHECK_ERROR(err, "Failed to read buffer C");
    


    // Verify (check first 10)
    printf("Verifying first 10 results:\n");
    for (int i = 0; i < 10; i++) {
        printf("C[%d] = %.0f + %.0f = %.0f\n", i, h_A[i], h_B[i], h_C[i]);
    }
    
    ////////////////////////////////////////////////////////////////////////////
    // Cleanup 
    ////////////////////////////////////////////////////////////////////////////

    clReleaseMemObject(d_A);
    clReleaseMemObject(d_B);
    clReleaseMemObject(d_C);
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
    
    free(h_A);
    free(h_B);
    free(h_C);
    
    return 0;
}