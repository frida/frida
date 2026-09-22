#include <stdio.h>
#include <cuda_runtime.h>
#include "shared/kernels.h"


int main(void) {
    int cuda_devices = 0;
    cudaGetDeviceCount(&cuda_devices);
    if(cuda_devices == 0) {
        printf("No Cuda hardware found. Exiting.\n");
        return 0;
    }

    if(run_tests() != 0){
        printf("CUDA tests failed! Exiting.\n");
        return 0;
    }

    return 0;
}
