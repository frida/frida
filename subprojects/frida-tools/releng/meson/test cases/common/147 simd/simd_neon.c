#include<simdconfig.h>
#include<simdfuncs.h>

#include<arm_neon.h>
#include<stdint.h>

int neon_available(void) {
    return 1; /* Incorrect, but I don't know how to check this properly. */
}

void increment_neon(float arr[4]) {
    float32x2_t a1, a2, one;
    a1 = vld1_f32(arr);
    a2 = vld1_f32(&arr[2]);
    one = vdup_n_f32(1.0);
    a1 = vadd_f32(a1, one);
    a2 = vadd_f32(a2, one);
    vst1_f32(arr, a1);
    vst1_f32(&arr[2], a2);
}
