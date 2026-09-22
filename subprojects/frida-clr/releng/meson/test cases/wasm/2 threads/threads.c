#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

void inthread(void * args) {
    sleep(1);
    printf("In thread\n");
}

int main() {
#ifdef __EMSCRIPTEN_PTHREADS__
    pthread_t thread_id;
    printf("Before Thread\n");
    pthread_create(&thread_id, NULL, (void *)*inthread, NULL);
    pthread_join(thread_id, NULL);
    printf("After Thread\n");
    return 0;
#else
# error "threads not enabled\n"
#endif
}
