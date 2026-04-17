#include <stdio.h>
#include <string.h>
#include <CL/cl.h>

#define CL_TARGET_OPENCL_VERSION 300

int main() {
    cl_uint numPlatforms;
    clGetPlatformIDs(0, NULL, &numPlatforms);
    printf("Number of platforms: %u\n\n", numPlatforms);

    cl_platform_id platforms[10];
    clGetPlatformIDs(numPlatforms, platforms, NULL);

    for (cl_uint i = 0; i < numPlatforms; i++) {
        char pname[256] = {0};
        clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, sizeof(pname), pname, NULL);
        printf("=== Platform %u: %s ===\n", i, pname);

        cl_uint numDevices = 0;
        cl_int err = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, 0, NULL, &numDevices);
        if (err != CL_SUCCESS || numDevices == 0) {
            printf("  No devices or error: %d\n\n", err);
            continue;
        }

        cl_device_id devices[10];
        clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, numDevices, devices, NULL);

        for (cl_uint j = 0; j < numDevices; j++) {
            char dname[256] = {0};
            cl_device_type dtype = 0;
            cl_uint cu = 0;
            size_t wgsize = 0;
            cl_ulong localmem = 0;
            size_t wgmult = 0;

            clGetDeviceInfo(devices[j], CL_DEVICE_NAME, sizeof(dname), dname, NULL);
            clGetDeviceInfo(devices[j], CL_DEVICE_TYPE, sizeof(dtype), &dtype, NULL);
            clGetDeviceInfo(devices[j], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cu), &cu, NULL);
            clGetDeviceInfo(devices[j], CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(wgsize), &wgsize, NULL);
            clGetDeviceInfo(devices[j], CL_DEVICE_LOCAL_MEM_SIZE, sizeof(localmem), &localmem, NULL);
            clGetDeviceInfo(devices[j], CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(wgmult), &wgmult, NULL);

            printf("  Device %u: %s\n", j, dname);
            printf("    Type:                    %s\n", (dtype & CL_DEVICE_TYPE_GPU) ? "GPU" : "CPU");
            printf("    Compute Units:           %u\n", cu);
            printf("    Max Work Group Size:     %zu\n", wgsize);
            printf("    Local Mem Size:          %llu bytes (%.0f KB)\n", (unsigned long long)localmem, (double)localmem/1024);
            printf("    Preferred WG Multiple:   %zu\n\n", wgmult);
        }
    }
    return 0;
}
