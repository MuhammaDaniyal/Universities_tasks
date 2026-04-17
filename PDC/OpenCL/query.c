#include <stdio.h>
#include <CL/cl.h>

int main() {
    cl_uint numPlatforms;
    clGetPlatformIDs(0, NULL, &numPlatforms);
    printf("Number of platforms: %u\n\n", numPlatforms);

    cl_platform_id platforms[10];
    clGetPlatformIDs(numPlatforms, platforms, NULL);

    for (cl_uint i = 0; i < numPlatforms; i++) {
        char pname[256];
        clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, sizeof(pname), pname, NULL);
        printf("Platform %u: %s\n", i, pname);

        cl_uint numDevices;
        clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, 0, NULL, &numDevices);
        cl_device_id devices[10];
        clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, numDevices, devices, NULL);

        for (cl_uint j = 0; j < numDevices; j++) {
            char dname[256]; cl_device_type dtype;
            cl_uint cu; size_t wgsize; cl_ulong localmem;
            size_t wgmult;

            clGetDeviceInfo(devices[j], CL_DEVICE_NAME, sizeof(dname), dname, NULL);
            clGetDeviceInfo(devices[j], CL_DEVICE_TYPE, sizeof(dtype), &dtype, NULL);
            clGetDeviceInfo(devices[j], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cu), &cu, NULL);
            clGetDeviceInfo(devices[j], CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(wgsize), &wgsize, NULL);
            clGetDeviceInfo(devices[j], CL_DEVICE_LOCAL_MEM_SIZE, sizeof(localmem), &localmem, NULL);
            clGetDeviceInfo(devices[j], CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(wgmult), &wgmult, NULL);

            printf("  Device %u: %s\n", j, dname);
            printf("    Type: %s\n", (dtype & CL_DEVICE_TYPE_GPU) ? "GPU" : "CPU");
            printf("    Compute Units: %u\n", cu);
            printf("    Max Work Group Size: %zu\n", wgsize);
            printf("    Local Mem Size: %llu bytes\n", (unsigned long long)localmem);
            printf("    Preferred WG Size Multiple: %zu\n\n", wgmult);
        }
    }
    return 0;
}
