#include<stdint.h>

int is_big_endian(void) {
    uint32_t one = 1;
    if(*((uint8_t*) &one) == 1)
        return 0;
    return 1;
}


int main(void) {
    int is_be_check = is_big_endian();
    int is_be;
#ifdef IS_BE
    is_be = 1;
#else
    is_be = 0;
#endif
    if(is_be_check && is_be)
        return 0;
    if(!is_be_check && !is_be)
        return 0;
    return 1;
}
