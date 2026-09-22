#include <signal.h>
#include <unistd.h>

int main(void) {
    kill(getpid(), SIGSEGV);
}
