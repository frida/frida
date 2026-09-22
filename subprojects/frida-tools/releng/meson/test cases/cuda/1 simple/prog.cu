#include <iostream>

int main(void) {
    int cuda_devices = 0;
    std::cout << "CUDA version: " << CUDART_VERSION << "\n";
    cudaGetDeviceCount(&cuda_devices);
    if(cuda_devices == 0) {
        std::cout << "No Cuda hardware found. Exiting.\n";
        return 0;
    }
    std::cout << "This computer has " << cuda_devices << " Cuda device(s).\n";
    cudaDeviceProp props;
    cudaGetDeviceProperties(&props, 0);
    std::cout << "Properties of device 0.\n\n";

    std::cout << "  Name:            " << props.name << "\n";
    std::cout << "  Global memory:   " << props.totalGlobalMem << "\n";
    std::cout << "  Shared memory:   " << props.sharedMemPerBlock << "\n";
    std::cout << "  Constant memory: " << props.totalConstMem << "\n";
    std::cout << "  Block registers: " << props.regsPerBlock << "\n";

    std::cout << "  Warp size:         " << props.warpSize << "\n";
    std::cout << "  Threads per block: " << props.maxThreadsPerBlock << "\n";
    std::cout << "  Max block dimensions: [ " << props.maxThreadsDim[0] << ", " << props.maxThreadsDim[1]  << ", " << props.maxThreadsDim[2] << " ]" << "\n";
    std::cout << "  Max grid dimensions:  [ " << props.maxGridSize[0] << ", " << props.maxGridSize[1]  << ", " << props.maxGridSize[2] << " ]" << "\n";
    std::cout << "\n";

    return 0;
}
