#include <cuda_runtime.h>
#include <iostream>

#ifndef NDEBUG
#error "NDEBUG not defined, this is a Meson bug"
#endif

int cuda_devices(void) {
    int result = 0;
    cudaGetDeviceCount(&result);
    return result;
}


int main(void) {
    int n = cuda_devices();
    if (n == 0) {
        std::cout << "No Cuda hardware found. Exiting.\n";
        return 0;
    }

    std::cout << "Found " << n << "Cuda devices.\n";
    return 0;
}
