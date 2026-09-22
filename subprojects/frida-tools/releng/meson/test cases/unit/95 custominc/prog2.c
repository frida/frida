#include<stdlib.h>
#include<generated.h>

int func(void);

int main(int argc, char **argv) {
    (void)argc;
    (void)(argv);
    return func() + RETURN_VALUE;
}
