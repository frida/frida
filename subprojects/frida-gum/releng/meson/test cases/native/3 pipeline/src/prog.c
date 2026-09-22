#include"input_src.h"

int main(void) {
    void *foo = printf;
    if(foo) {
        return 0;
    }
    return 1;
}
