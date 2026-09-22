/*
 * Copyright (C) 2008-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2021 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

/**
 * GumBacktracer:
 *
 * Generates a backtrace by walking a thread's stack.
 *
 * ## Using `GumBacktracer`
 *
 * ```c
 * g_autoptr(GumBacktracer) backtracer = gum_backtracer_make_accurate ();
 *                                               // or: make_fuzzy
 *
 * GumCpuContext *cpu_context = NULL; // walk from here
 * GumReturnAddressArray retaddrs;
 * gum_backtracer_generate (backtracer, cpu_context, &retaddrs);
 *
 * for (guint i = 0; i != retaddrs.len; i++)
 *   {
 *     g_print ("retaddrs[%u] = %p\n", i, retaddrs->items[i]);
 *   }
 * ```
 */

#include "gumbacktracer.h"

#ifdef HAVE_WINDOWS
# include "gum/gumdbghelpbacktracer.h"
# include "arch-x86/gumx86backtracer.h"
#elif defined (HAVE_DARWIN)
# include "gum/gumdarwinbacktracer.h"
#elif defined (HAVE_LIBUNWIND)
# include "gum/gumunwbacktracer.h"
#endif

#if defined (HAVE_I386)
# include "arch-x86/gumx86backtracer.h"
#elif defined (HAVE_ARM)
# include "arch-arm/gumarmbacktracer.h"
#elif defined (HAVE_ARM64)
# include "arch-arm64/gumarm64backtracer.h"
#elif defined (HAVE_MIPS)
# include "arch-mips/gummipsbacktracer.h"
#endif

G_DEFINE_INTERFACE (GumBacktracer, gum_backtracer, G_TYPE_OBJECT)

static void
gum_backtracer_default_init (GumBacktracerInterface * iface)
{
}

/**
 * gum_backtracer_make_accurate:
 *
 * Creates a new accurate backtracer, optimized for debugger-friendly binaries
 * or presence of debug information. Resulting backtraces will never contain
 * bogus entries but may be cut short when encountering code built without
 * frame pointers *and* lack of debug information.
 *
 * Returns: (nullable) (transfer full): the newly created backtracer instance
 */
GumBacktracer *
gum_backtracer_make_accurate (void)
{
#if defined (HAVE_WINDOWS)
  GumDbghelpImpl * dbghelp;

  dbghelp = gum_dbghelp_impl_try_obtain ();
  if (dbghelp == NULL)
    return NULL;
  return gum_dbghelp_backtracer_new (dbghelp);
#elif defined (HAVE_DARWIN)
  return gum_darwin_backtracer_new ();
#elif defined (HAVE_LIBUNWIND)
  return gum_unw_backtracer_new ();
#else
  return NULL;
#endif
}

/**
 * gum_backtracer_make_fuzzy:
 *
 * Creates a new fuzzy backtracer, optimized for debugger-unfriendly binaries
 * that lack debug information. Performs forensics on the stack in order to
 * guess the return addresses. Resulting backtraces will often contain bogus
 * entries, but will never be cut short upon encountering code built without
 * frame pointers *and* lack of debug information.
 *
 * Returns: (nullable) (transfer full): the newly created backtracer instance
 */
GumBacktracer *
gum_backtracer_make_fuzzy (void)
{
#if defined (HAVE_I386)
  return gum_x86_backtracer_new ();
#elif defined (HAVE_ARM)
  return gum_arm_backtracer_new ();
#elif defined (HAVE_ARM64)
  return gum_arm64_backtracer_new ();
#elif defined (HAVE_MIPS)
  return gum_mips_backtracer_new ();
#else
  return NULL;
#endif
}

/**
 * gum_backtracer_generate:
 * @self: a backtracer
 * @cpu_context: (nullable): the location to start walking from
 * @return_addresses: (out caller-allocates): the resulting backtrace
 *
 * Walks a thread's stack and stores each return address in `return_addresses`.
 * Omit `cpu_context` to start walking from where this function is called from.
 */
void
gum_backtracer_generate (GumBacktracer * self,
                         const GumCpuContext * cpu_context,
                         GumReturnAddressArray * return_addresses)
{
  gum_backtracer_generate_with_limit (self, cpu_context, return_addresses,
      GUM_MAX_BACKTRACE_DEPTH);
}

/**
 * gum_backtracer_generate_with_limit:
 * @self: a backtracer
 * @cpu_context: (nullable): the location to start walking from
 * @return_addresses: (out caller-allocates): the resulting backtrace
 * @limit: the limit on how far to walk
 *
 * Walks a thread's stack and stores each return address in `return_addresses`,
 * stopping after `limit` entries. Omit `cpu_context` to start walking from
 * where this function is called from.
 */
void
gum_backtracer_generate_with_limit (GumBacktracer * self,
                                    const GumCpuContext * cpu_context,
                                    GumReturnAddressArray * return_addresses,
                                    guint limit)
{
  GumBacktracerInterface * iface = GUM_BACKTRACER_GET_IFACE (self);

  g_assert (iface->generate != NULL);

  iface->generate (self, cpu_context, return_addresses, limit);
}
