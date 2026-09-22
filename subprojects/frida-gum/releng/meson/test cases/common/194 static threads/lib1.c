#if defined _WIN32
#include<windows.h>
#else
#include<pthread.h>
#endif

void *f(void) {
#if defined _WIN32
  return CreateThread;
#else
  return pthread_create;
#endif
}
