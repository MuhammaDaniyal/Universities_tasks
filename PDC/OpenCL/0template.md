```c
// 1. Write kernel string
const char* kernel = "__kernel void add(...) {...}";

// 2-4. Setup OpenCL
clGetPlatformIDs(...);
clGetDeviceIDs(...);
context = clCreateContext(...);
queue = clCreateCommandQueue(...);

// 5. Create GPU memory
d_A = clCreateBuffer(...);
d_B = clCreateBuffer(...);
d_C = clCreateBuffer(...);

// 6. Copy to GPU
clEnqueueWriteBuffer(queue, d_A, ...);
clEnqueueWriteBuffer(queue, d_B, ...);

// 7. Create & build program
program = clCreateProgramWithSource(context, ...);
clBuildProgram(program, ...);
kernel_obj = clCreateKernel(program, "add", ...);

// 8. Set arguments
clSetKernelArg(kernel_obj, 0, ...);
clSetKernelArg(kernel_obj, 1, ...);
clSetKernelArg(kernel_obj, 2, ...);

// 9. Execute
clEnqueueNDRangeKernel(queue, kernel_obj, ...);

// 10. Read back
clEnqueueReadBuffer(queue, d_C, ..., h_C, ...);

// 11. Cleanup
clRelease... (everything);
free(h_A, h_B, h_C);
```