#include<simdconfig.h>
#include<simdfuncs.h>

#include<stdint.h>

#ifdef _MSC_VER
#include<intrin.h>
int mmx_available(void) {
  return 1;
}
/* Contrary to MSDN documentation, MMX intrinsics
 * just plain don't work.
 */
void increment_mmx(float arr[4]) {
  arr[0]++;
  arr[1]++;
  arr[2]++;
  arr[3]++;
}
#elif defined(__MINGW32__)
int mmx_available(void) {
  return 1;
}
/* MinGW does not seem to ship with MMX or it is broken.
 */
void increment_mmx(float arr[4]) {
  arr[0]++;
  arr[1]++;
  arr[2]++;
  arr[3]++;
}
#else
#include<mmintrin.h>
#include<cpuid.h>

#if defined(__APPLE__)
int mmx_available(void) { return 1; }
#else
int mmx_available(void) {
    return __builtin_cpu_supports("mmx");
}
#endif
void increment_mmx(float arr[4]) {
    /* Super ugly but we know that values in arr are always small
     * enough to fit in int16;
     */
    int i;
    /* This is unused due to below comment about GCC 8.
    __m64 packed = _mm_set_pi16(arr[3], arr[2], arr[1], arr[0]);
    __m64 incr = _mm_set1_pi16(1);
    __m64 result = _mm_add_pi16(packed, incr);
    int64_t unpacker = (int64_t)(result);
     */
    /* The above should be
     * int64_t unpacker = _m_to_int64(result);
     * but it does not exist on 32 bit platforms for some reason.
     */
    _mm_empty();
    for(i=0; i<4; i++) {
      /* This fails on GCC 8 when optimizations are enabled.
       * Disable it. Patches welcome to fix this.
      arr[i] = (float)(unpacker & ((1<<16)-1));
        unpacker >>= 16;
      */
      arr[i] += 1.0f;
    }
}

#endif
