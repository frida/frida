#include<iostream>

extern "C" {
  int get_retval(void);
}

int main(void) {
  std::cout << "C++ seems to be working." << std::endl;
#if defined(USE_ASM)
  return get_retval();
#elif defined(NO_USE_ASM)
  return 0;
#else
  #error "Forgot to pass asm define"
#endif
}
