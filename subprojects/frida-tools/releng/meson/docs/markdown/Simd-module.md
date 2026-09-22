# Unstable SIMD module

This module provides helper functionality to build code with SIMD instructions.
Available since 0.42.0.

**Note**: this module is unstable. It is only provided as a technology
preview. Its API may change in arbitrary ways between releases or it
might be removed from Meson altogether.

## Usage

This module is designed for the use case where you have an algorithm
with one or more SIMD implementation and you choose which one to use
at runtime.

The module provides one method, `check`, which is used like this:

    rval = simd.check('mysimds',
      mmx : 'simd_mmx.c',
      sse : 'simd_sse.c',
      sse2 : 'simd_sse2.c',
      sse3 : 'simd_sse3.c',
      ssse3 : 'simd_ssse3.c',
      sse41 : 'simd_sse41.c',
      sse42 : 'simd_sse42.c',
      avx : 'simd_avx.c',
      avx2 : 'simd_avx2.c',
      neon : 'simd_neon.c',
      compiler : cc)

Here the individual files contain the accelerated versions of the
functions in question. The `compiler` keyword argument takes the
compiler you are going to use to compile them. The function returns an
array with two values. The first value is a bunch of libraries that
contain the compiled code. Any SIMD code that the compiler can't
compile (for example, Neon instructions on an x86 machine) are
ignored. You should pass this value to the desired target using
`link_with`. The second value is a `configuration_data` object that
contains true for all the values that were supported. For example if
the compiler did support sse2 instructions, then the object would have
`HAVE_SSE2` set to 1.

Generating code to detect the proper instruction set at runtime is
straightforward. First you create a header with the configuration
object and then a chooser function that looks like this:

    void (*fptr)(type_of_function_here)  = NULL;

    #if HAVE_NEON
    if(fptr == NULL && neon_available()) {
        fptr = neon_accelerated_function;
    }
    #endif
    #if HAVE_AVX2
    if(fptr == NULL && avx2_available()) {
        fptr = avx_accelerated_function;
    }
    #endif

    ...

    if(fptr == NULL) {
        fptr = default_function;
    }

Each source file provides two functions, the `xxx_available` function
to query whether the CPU currently in use supports the instruction set
and `xxx_accelerated_function` that is the corresponding accelerated
implementation.

At the end of this function the function pointer points to the fastest
available implementation and can be invoked to do the computation.
