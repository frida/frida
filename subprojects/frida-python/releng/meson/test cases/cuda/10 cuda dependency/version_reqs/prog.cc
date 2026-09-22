#include <cuda_runtime.h>
#include <iostream>

int cuda_devices(void) {
    int result = 0;
    cudaGetDeviceCount(&result);
    return result;
}

int main(void) {
    std::cout << "Compiled against CUDA version: " << CUDART_VERSION << "\n";
    int runtime_version = 0;
    cudaError_t r = cudaRuntimeGetVersion(&runtime_version);
    if (r != cudaSuccess) {
        std::cout << "Couldn't obtain CUDA runtime version (error " << r << "). Exiting.\n";
        return -1;
    }
    std::cout << "CUDA runtime version: " << runtime_version << "\n";

    int n = cuda_devices();
    if (n == 0) {
        std::cout << "No CUDA hardware found. Exiting.\n";
        return 0;
    }

    std::cout << "Found " << n << " CUDA devices.\n";
    return 0;
}
