#include <stdio.h>
#include <iostream>

__global__ void kernel (void){
}

int do_cuda_stuff(void) {
  kernel<<<1,1>>>();

  printf("Hello, World!\n");
  return 0;
}
