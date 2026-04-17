#define CL_TARGET_OPENCL_VERSION 300
#include <stdio.h>
#include <CL/cl.h>

int main(int argc, char* argv[]) {
    int target = atoi(argv[1]);
    
    cl_uint numPlatforms;
    clGetPlatformIDs(0, NULL, &numPlatforms);
    
    cl_platform_id platforms[10];
    clGetPlatformIDs(numPlatforms, platforms, NULL);

    char pname[256] = {0};
    clGetPlatformInfo(platforms[target], CL_PLATFORM_NAME, sizeof(pname), pname, NULL);
    printf("Platform %d: %s\n", target, pname);

    cl_uint numDevices = 0;
    clGetDeviceIDs(platforms[target], CL_DEVICE_TYPE_ALL, 0, NULL, &numDevices);
    printf("Devices: %u\n", numDevices);

    cl_device_id devices[10];
    clGetDeviceIDs(platforms[target], CL_DEVICE_TYPE_ALL, numDevices, devices, NULL);

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

        printf("  Device: %s\n", dname);
        printf("  Type: %s\n", (dtype & CL_DEVICE_TYPE_GPU) ? "GPU" : "CPU");
        printf("  Compute Units: %u\n", cu);
        printf("  Max WG Size: %zu\n", wgsize);
        printf("  Local Mem: %llu bytes\n", (unsigned long long)localmem);
        printf("  Preferred WG Multiple: %zu\n", wgmult);
    }
    return 0;
}
