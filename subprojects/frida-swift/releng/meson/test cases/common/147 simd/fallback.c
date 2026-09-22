#include<simdfuncs.h>

void increment_fallback(float arr[4]) {
    int i;
    for(i=0; i<4; i++) {
        arr[i]++;
    }
}
