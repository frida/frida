#ifndef __FRIDA_ATOMICS_H__
#define __FRIDA_ATOMICS_H__

#include <glib.h>

G_BEGIN_DECLS

#ifdef _MSC_VER

/* TODO: Add once needed. */

#else

typedef guint64 FridaAtomicU64 __attribute__ ((aligned (8)));

static inline guint64
frida_atomics_load_u64_acquire (volatile FridaAtomicU64 * p)
{
  return __atomic_load_n (p, __ATOMIC_ACQUIRE);
}

static inline void
frida_atomics_store_u64_release (volatile FridaAtomicU64 * p, guint64 v)
{
  __atomic_store_n (p, v, __ATOMIC_RELEASE);
}

static inline guint32 frida_atomics_load_u32_acquire (volatile guint32 * p)
{
  return __atomic_load_n (p, __ATOMIC_ACQUIRE);
}

#endif

G_END_DECLS

#endif
