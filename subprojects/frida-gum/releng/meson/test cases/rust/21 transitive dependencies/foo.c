#include <stdint.h>

uint32_t foo_rs(void);

int main(void)
{
    return foo_rs() == 42 ? 0 : 1;
}
