#include<simdconfig.h>
#include<simdfuncs.h>

#ifdef _MSC_VER
#include<intrin.h>
int sse_available(void) {
  return 1;
}
#else

#include<xmmintrin.h>
#include<cpuid.h>
#include<stdint.h>

#if defined(__APPLE__)
int sse_available(void) { return 1; }
#else
int sse_available(void) {
    return __builtin_cpu_supports("sse");
}
#endif
#endif

void increment_sse(float arr[4]) {
    __m128 val = _mm_load_ps(arr);
    __m128 one = _mm_set_ps1(1.0);
    __m128 result = _mm_add_ps(val, one);
    _mm_storeu_ps(arr, result);
}
