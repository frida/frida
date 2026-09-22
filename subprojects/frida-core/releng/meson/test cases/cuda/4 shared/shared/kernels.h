/* Include Guard */
#ifndef SHARED_KERNELS_H
#define SHARED_KERNELS_H

/**
 * Includes
 */

#include <cuda_runtime.h>


/**
 * Defines
 */

/**
 * When building a library, it is a good idea to expose as few as possible
 * internal symbols (functions, objects, data structures). Not only does it
 * prevent users from relying on private portions of the library that are
 * subject to change without any notice, but it can have performance
 * advantages:
 *
 *   - It can make shared libraries link faster at dynamic-load time.
 *   - It can make internal function calls faster by bypassing the PLT.
 *
 * Thus, the compilation should by default hide all symbols, while the API
 * headers will explicitly mark public the few symbols the users are permitted
 * to use with a PUBLIC tag. We also define a HIDDEN tag, since it may be
 * required to explicitly tag certain C++ types as visible in order for
 * exceptions to function correctly.
 *
 * Additional complexity comes from non-POSIX-compliant systems, which
 * artificially impose a requirement on knowing whether we are building or
 * using a DLL.
 *
 * The above commentary and below code is inspired from
 *                   'https://gcc.gnu.org/wiki/Visibility'
 */

#if   defined(_WIN32) || defined(__CYGWIN__)
# define TAG_ATTRIBUTE_EXPORT __declspec(dllexport)
# define TAG_ATTRIBUTE_IMPORT __declspec(dllimport)
# define TAG_ATTRIBUTE_HIDDEN
#elif __GNUC__ >= 4
# define TAG_ATTRIBUTE_EXPORT __attribute__((visibility("default")))
# define TAG_ATTRIBUTE_IMPORT __attribute__((visibility("default")))
# define TAG_ATTRIBUTE_HIDDEN __attribute__((visibility("hidden")))
#else
# define TAG_ATTRIBUTE_EXPORT
# define TAG_ATTRIBUTE_IMPORT
# define TAG_ATTRIBUTE_HIDDEN
#endif

#if TAG_IS_SHARED
# if TAG_IS_BUILDING
#  define TAG_PUBLIC TAG_ATTRIBUTE_EXPORT
# else
#  define TAG_PUBLIC TAG_ATTRIBUTE_IMPORT
# endif
# define  TAG_HIDDEN TAG_ATTRIBUTE_HIDDEN
#else
# define  TAG_PUBLIC
# define  TAG_HIDDEN
#endif
#define   TAG_STATIC static




/* Extern "C" Guard */
#ifdef __cplusplus
extern "C" {
#endif



/* Function Prototypes */
TAG_PUBLIC int run_tests(void);



/* End Extern "C" and Include Guard */
#ifdef __cplusplus
}
#endif
#endif
