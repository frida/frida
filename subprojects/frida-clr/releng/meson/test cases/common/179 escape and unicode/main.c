#include <string.h>

const char* does_it_work(void);

int a_fun(void);

int main(void) {
    if(strcmp(does_it_work(), "yes it does") != 0) {
        return -a_fun();
    }
    return 0;
}
