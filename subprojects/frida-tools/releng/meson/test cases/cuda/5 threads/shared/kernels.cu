#include <stdio.h>
#include <cuda_runtime.h>
#include "kernels.h"


TAG_HIDDEN __global__ void kernel (void){
}

TAG_PUBLIC int run_tests(void) {
  kernel<<<1,1>>>();

  return (int)cudaDeviceSynchronize();
}
