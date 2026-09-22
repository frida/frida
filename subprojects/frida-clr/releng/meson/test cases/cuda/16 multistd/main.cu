#include <cuda_runtime.h>
#include <iostream>

auto cuda_devices(void) {
    int result = 0;
    cudaGetDeviceCount(&result);
    return result;
}

int do_cuda_stuff();

int main(void) {
    int n = cuda_devices();
    if (n == 0) {
        std::cout << "No Cuda hardware found. Exiting.\n";
        return 0;
    }

    std::cout << "Found " << n << "Cuda devices.\n";
    return do_cuda_stuff();
}
