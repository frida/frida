#include<simdconfig.h>
#include<simdfuncs.h>

#include<emmintrin.h>
#include<tmmintrin.h>

#ifdef _MSC_VER
#include<intrin.h>

int ssse3_available(void) {
  return 1;
}

#else

#include<cpuid.h>
#include<stdint.h>

int ssse3_available(void) {
#ifdef __APPLE__
    return 1;
#elif defined(__clang__)
    /* https://github.com/numpy/numpy/issues/8130 */
    return __builtin_cpu_supports("sse4.1");
#else
    return __builtin_cpu_supports("ssse3");
#endif
}

#endif

void increment_ssse3(float arr[4]) {
    ALIGN_16 double darr[4];
    __m128d val1 = _mm_set_pd(arr[0], arr[1]);
    __m128d val2 = _mm_set_pd(arr[2], arr[3]);
    __m128d one = _mm_set_pd(1.0, 1.0);
    __m128d result = _mm_add_pd(val1, one);
    __m128i tmp1, tmp2;
    tmp1 = tmp2 = _mm_set1_epi16(0);
    _mm_store_pd(darr, result);
    result = _mm_add_pd(val2, one);
    _mm_store_pd(&darr[2], result);
    tmp1 = _mm_hadd_epi32(tmp1, tmp2); /* This does nothing. Only here so we use an SSSE3 instruction. */
    arr[0] = (float)darr[1];
    arr[1] = (float)darr[0];
    arr[2] = (float)darr[3];
    arr[3] = (float)darr[2];
}
