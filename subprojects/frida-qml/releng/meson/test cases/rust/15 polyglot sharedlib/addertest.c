#include<stdlib.h>
#include<adder.h>

int main(int argc, char **argv) {
    adder *a = adder_create(3);
    int result = adder_add(a, 4);
    if(result != 7) {
        return 1;
    }
    adder_destroy(a);
    return 0;
}
