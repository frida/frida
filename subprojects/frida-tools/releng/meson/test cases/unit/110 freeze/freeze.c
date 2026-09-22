#include<stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

static void do_nothing(int signo, siginfo_t *info, void *context) {
}

int main(int argc, char **argv) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_sigaction = do_nothing;
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        printf("Could not set up signal handler.\n");
        return 1;
    }
    printf("Freezing forever.\n");
    while(1) {
    }
    return 0;
}
