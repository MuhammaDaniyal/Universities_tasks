// opencl.cpp — Blelloch (exclusive) prefix-sum with OpenCL
// GPU preferred; automatic fallback to CPU OpenCL device.
// Compile: g++ -O2 -o opencl opencl.cpp -lOpenCL
// Usage  : ./opencl [array_size]

#ifdef __APPLE__
#  include <OpenCL/opencl.h>
#else
#  include <CL/cl.h>
#endif

#include <iostream>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>
#include <stdexcept>

// ---------------------------------------------------------------------------
// OpenCL kernel source — upsweep and downsweep passes
// ---------------------------------------------------------------------------
static const char* KERNEL_SRC = R"CLC(
// Upsweep: a[right] += a[left]  where right = gid*step+(step-1), left = right-stride
__kernel void upsweep(__global long* a,
                      const ulong stride,
                      const ulong step,
                      const ulong n)
{
    ulong gid = get_global_id(0);
    ulong i   = gid * step + (step - 1UL);
    if (i < n)
        a[i] += a[i - stride];
}

// Downsweep: swap-and-add
__kernel void downsweep(__global long* a,
                        const ulong stride,
                        const ulong step,
                        const ulong n)
{
    ulong gid  = get_global_id(0);
    ulong i    = gid * step + (step - 1UL);
    if (i < n) {
        long tmp    = a[i - stride];
        a[i - stride] = a[i];
        a[i]        += tmp;
    }
}
)CLC";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
#define CL_CHECK(err, msg) \
    if ((err) != CL_SUCCESS) { \
        std::cerr << "[OpenCL error] " << (msg) << " — code " << (err) << "\n"; \
        return false; \
    }

static std::string deviceTypeName(cl_device_type t) {
    if (t & CL_DEVICE_TYPE_GPU)         return "GPU";
    if (t & CL_DEVICE_TYPE_CPU)         return "CPU";
    if (t & CL_DEVICE_TYPE_ACCELERATOR) return "Accelerator";
    return "Unknown";
}

// ---------------------------------------------------------------------------
// Try to pick a device: prefer GPU, fall back to CPU OpenCL device.
// Returns false if nothing is available.
// ---------------------------------------------------------------------------
static bool pickDevice(cl_platform_id& platOut,
                       cl_device_id&   devOut,
                       std::string&    devName,
                       std::string&    devType)
{
    // Enumerate platforms
    cl_uint numPlat = 0;
    cl_int  err     = clGetPlatformIDs(0, nullptr, &numPlat);
    if (err != CL_SUCCESS || numPlat == 0) {
        std::cerr << "[OpenCL] No platforms found (err=" << err << ").\n";
        return false;
    }
    std::vector<cl_platform_id> plats(numPlat);
    clGetPlatformIDs(numPlat, plats.data(), nullptr);

    // First pass: look for GPU
    for (auto p : plats) {
        cl_uint n = 0;
        if (clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, 0, nullptr, &n) == CL_SUCCESS && n > 0) {
            std::vector<cl_device_id> devs(n);
            clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, n, devs.data(), nullptr);
            platOut = p;
            devOut  = devs[0];
            char buf[256] = {};
            clGetDeviceInfo(devOut, CL_DEVICE_NAME, sizeof(buf), buf, nullptr);
            devName = buf;
            devType = "GPU";
            return true;
        }
    }

    // Second pass: fall back to CPU
    std::cerr << "[OpenCL] No GPU device found — falling back to CPU OpenCL device.\n";
    for (auto p : plats) {
        cl_uint n = 0;
        if (clGetDeviceIDs(p, CL_DEVICE_TYPE_CPU, 0, nullptr, &n) == CL_SUCCESS && n > 0) {
            std::vector<cl_device_id> devs(n);
            clGetDeviceIDs(p, CL_DEVICE_TYPE_CPU, n, devs.data(), nullptr);
            platOut = p;
            devOut  = devs[0];
            char buf[256] = {};
            clGetDeviceInfo(devOut, CL_DEVICE_NAME, sizeof(buf), buf, nullptr);
            devName = buf;
            devType = "CPU (OpenCL fallback)";
            return true;
        }
    }

    std::cerr << "[OpenCL] No OpenCL device found at all.\n";
    return false;
}

// ---------------------------------------------------------------------------
// Main scan routine
// ---------------------------------------------------------------------------
static bool runOpenCLScan(std::size_t N, double& msOut, long long& lastOut,
                          std::string& devNameOut, std::string& devTypeOut)
{
    // ---- Device selection ----
    cl_platform_id plat{};
    cl_device_id   dev{};
    if (!pickDevice(plat, dev, devNameOut, devTypeOut)) return false;

    // ---- Context + queue ----
    cl_int err;
    cl_context ctx = clCreateContext(nullptr, 1, &dev, nullptr, nullptr, &err);
    CL_CHECK(err, "clCreateContext");

    // Use deprecated but widely supported clCreateCommandQueue for CL 1.x compat
    cl_command_queue queue = clCreateCommandQueue(ctx, dev, 0, &err);
    CL_CHECK(err, "clCreateCommandQueue");

    // ---- Compile kernels ----
    cl_program prog = clCreateProgramWithSource(ctx, 1, &KERNEL_SRC, nullptr, &err);
    CL_CHECK(err, "clCreateProgramWithSource");

    err = clBuildProgram(prog, 1, &dev, "-cl-std=CL1.2", nullptr, nullptr);
    if (err != CL_SUCCESS) {
        // Print build log
        std::size_t logLen = 0;
        clGetProgramBuildInfo(prog, dev, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logLen);
        std::string log(logLen, '\0');
        clGetProgramBuildInfo(prog, dev, CL_PROGRAM_BUILD_LOG, logLen, &log[0], nullptr);
        std::cerr << "[OpenCL] Build error:\n" << log << "\n";
        clReleaseProgram(prog); clReleaseCommandQueue(queue); clReleaseContext(ctx);
        return false;
    }

    cl_kernel kUp   = clCreateKernel(prog, "upsweep",   &err); CL_CHECK(err, "clCreateKernel upsweep");
    cl_kernel kDown = clCreateKernel(prog, "downsweep", &err); CL_CHECK(err, "clCreateKernel downsweep");

    // ---- Allocate & fill host buffer (padded to power-of-two) ----
    std::size_t padded = 1;
    while (padded < N) padded <<= 1;

    std::vector<cl_long> h_data(padded, 0LL);
    for (std::size_t i = 0; i < N; ++i) h_data[i] = 1LL;

    // ---- Device buffer ----
    cl_mem d_buf = clCreateBuffer(ctx,
                                  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                  padded * sizeof(cl_long),
                                  h_data.data(), &err);
    CL_CHECK(err, "clCreateBuffer");

    // ---- Query max work-group size for kernels ----
    std::size_t maxWGS = 0;
    clGetKernelWorkGroupInfo(kUp, dev, CL_KERNEL_WORK_GROUP_SIZE,
                             sizeof(maxWGS), &maxWGS, nullptr);
    if (maxWGS == 0) maxWGS = 64;

    // ---- Timed scan ----
    clFinish(queue);  // make sure upload is done before timing

    auto t0 = std::chrono::high_resolution_clock::now();

    // Upsweep
    for (std::size_t stride = 1; stride < padded; stride <<= 1) {
        cl_ulong uStride = static_cast<cl_ulong>(stride);
        cl_ulong uStep   = static_cast<cl_ulong>(stride << 1);
        cl_ulong uN      = static_cast<cl_ulong>(padded);

        std::size_t numGroups = padded / (stride << 1);
        if (numGroups == 0) numGroups = 1;
        std::size_t gws = numGroups;  // one work-item per pair

        clSetKernelArg(kUp, 0, sizeof(cl_mem),   &d_buf);
        clSetKernelArg(kUp, 1, sizeof(cl_ulong),  &uStride);
        clSetKernelArg(kUp, 2, sizeof(cl_ulong),  &uStep);
        clSetKernelArg(kUp, 3, sizeof(cl_ulong),  &uN);

        err = clEnqueueNDRangeKernel(queue, kUp, 1, nullptr, &gws, nullptr, 0, nullptr, nullptr);
        CL_CHECK(err, "clEnqueueNDRangeKernel upsweep");
    }

    // Set root to 0
    cl_long zero = 0;
    err = clEnqueueWriteBuffer(queue, d_buf, CL_FALSE,
                               (padded - 1) * sizeof(cl_long),
                               sizeof(cl_long), &zero, 0, nullptr, nullptr);
    CL_CHECK(err, "clEnqueueWriteBuffer (set root)");

    // Downsweep
    for (std::size_t stride = padded >> 1; stride >= 1; stride >>= 1) {
        cl_ulong uStride = static_cast<cl_ulong>(stride);
        cl_ulong uStep   = static_cast<cl_ulong>(stride << 1);
        cl_ulong uN      = static_cast<cl_ulong>(padded);

        std::size_t numGroups = padded / (stride << 1);
        if (numGroups == 0) numGroups = 1;
        std::size_t gws = numGroups;

        clSetKernelArg(kDown, 0, sizeof(cl_mem),   &d_buf);
        clSetKernelArg(kDown, 1, sizeof(cl_ulong),  &uStride);
        clSetKernelArg(kDown, 2, sizeof(cl_ulong),  &uStep);
        clSetKernelArg(kDown, 3, sizeof(cl_ulong),  &uN);

        err = clEnqueueNDRangeKernel(queue, kDown, 1, nullptr, &gws, nullptr, 0, nullptr, nullptr);
        CL_CHECK(err, "clEnqueueNDRangeKernel downsweep");
    }

    // Read back
    err = clEnqueueReadBuffer(queue, d_buf, CL_TRUE,
                              0, padded * sizeof(cl_long),
                              h_data.data(), 0, nullptr, nullptr);
    CL_CHECK(err, "clEnqueueReadBuffer");

    auto t1 = std::chrono::high_resolution_clock::now();
    msOut   = std::chrono::duration<double, std::milli>(t1 - t0).count();
    lastOut = static_cast<long long>(h_data[N - 1]);

    // ---- Cleanup ----
    clReleaseMemObject(d_buf);
    clReleaseKernel(kUp);
    clReleaseKernel(kDown);
    clReleaseProgram(prog);
    clReleaseCommandQueue(queue);
    clReleaseContext(ctx);

    return true;
}

// ---------------------------------------------------------------------------
// Sequential fallback (mirrors naive.cpp) — used when OpenCL unavailable
// ---------------------------------------------------------------------------
static void blelloch_seq_fallback(std::vector<long long>& a, std::size_t N)
{
    std::size_t padded = 1;
    while (padded < N) padded <<= 1;
    a.resize(padded, 0LL);

    for (std::size_t stride = 1; stride < padded; stride <<= 1)
        for (std::size_t i = (stride<<1)-1; i < padded; i += stride<<1)
            a[i] += a[i - stride];

    a[padded - 1] = 0;

    for (std::size_t stride = padded >> 1; stride >= 1; stride >>= 1)
        for (std::size_t i = (stride<<1)-1; i < padded; i += stride<<1) {
            long long tmp   = a[i - stride];
            a[i - stride]  = a[i];
            a[i]           += tmp;
        }
}

// ---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    std::size_t N = (argc > 1) ? static_cast<std::size_t>(std::atoll(argv[1]))
                               : 10'000'000ULL;

    if (N == 0) { std::cout << "Empty array — nothing to do.\n"; return 0; }

    std::cout << "=== OpenCL Blelloch Scan ===\n";
    std::cout << "Array size : " << N << "\n";

    double      ms       = 0.0;
    long long   last     = 0LL;
    std::string devName, devType;

    bool clOk = runOpenCLScan(N, ms, last, devName, devType);

    if (!clOk) {
        std::cerr << "[OpenCL] Falling back to sequential CPU scan.\n";
        devName = "CPU (sequential fallback)";
        devType = "CPU";

        std::vector<long long> data(N, 1LL);
        auto t0 = std::chrono::high_resolution_clock::now();
        blelloch_seq_fallback(data, N);
        auto t1 = std::chrono::high_resolution_clock::now();
        ms   = std::chrono::duration<double, std::milli>(t1 - t0).count();
        last = data[N - 1];
    }

    bool ok = (last == static_cast<long long>(N - 1));

    std::cout << "Device     : " << devName << " [" << devType << "]\n";
    std::cout << "Time       : " << ms << " ms\n";
    std::cout << "data[N-1]  : " << last << "  (expected " << N - 1 << ")\n";
    std::cout << "Correct    : " << (ok ? "YES" : "NO") << "\n";

    return ok ? 0 : 1;
}