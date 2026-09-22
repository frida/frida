/*
 * Copyright (C) 2015-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2021 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gum/gumdarwinbacktracer.h"

#include "guminterceptor.h"

#define GUM_FP_LINK_OFFSET 1
#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
# define GUM_FP_IS_ALIGNED(F) ((GPOINTER_TO_SIZE (F) & 0xf) == 8)
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
# define GUM_FP_IS_ALIGNED(F) ((GPOINTER_TO_SIZE (F) & 0xf) == 0)
# define GUM_FFI_STACK_SKIP (0xd8 / 8)
#else
# define GUM_FP_IS_ALIGNED(F) ((GPOINTER_TO_SIZE (F) & 0x1) == 0)
# if defined (HAVE_ARM)
#  define GUM_FFI_STACK_SKIP 1
# endif
#endif

struct _GumDarwinBacktracer
{
  GObject parent;
};

static void gum_darwin_backtracer_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_darwin_backtracer_generate (GumBacktracer * backtracer,
    const GumCpuContext * cpu_context, GumReturnAddressArray * return_addresses,
    guint limit);

static gpointer gum_strip_item (gpointer address);

G_DEFINE_TYPE_EXTENDED (GumDarwinBacktracer,
                        gum_darwin_backtracer,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_BACKTRACER,
                            gum_darwin_backtracer_iface_init))

static void
gum_darwin_backtracer_class_init (GumDarwinBacktracerClass * klass)
{
}

static void
gum_darwin_backtracer_iface_init (gpointer g_iface,
                                  gpointer iface_data)
{
  GumBacktracerInterface * iface = g_iface;

  iface->generate = gum_darwin_backtracer_generate;
}

static void
gum_darwin_backtracer_init (GumDarwinBacktracer * self)
{
}

GumBacktracer *
gum_darwin_backtracer_new (void)
{
  return g_object_new (GUM_TYPE_DARWIN_BACKTRACER, NULL);
}

static void
gum_darwin_backtracer_generate (GumBacktracer * backtracer,
                                const GumCpuContext * cpu_context,
                                GumReturnAddressArray * return_addresses,
                                guint limit)
{
  pthread_t thread;
  gpointer stack_top, stack_bottom;
  gpointer * cur;
  gint start_index, n_skip, depth, i;
  gboolean has_ffi_frames;
#ifdef HAVE_ARM
  gpointer * ffi_next = NULL;
#endif
  GumInvocationStack * invocation_stack;

  thread = pthread_self ();
  stack_top = pthread_get_stackaddr_np (thread);
  stack_bottom = stack_top - pthread_get_stacksize_np (thread);
  stack_top -= (GUM_FP_LINK_OFFSET + 1) * sizeof (gpointer);

  if (cpu_context != NULL)
  {
#if defined (HAVE_I386)
    cur = GSIZE_TO_POINTER (GUM_CPU_CONTEXT_XBP (cpu_context));

    has_ffi_frames = GUM_CPU_CONTEXT_XIP (cpu_context) == 0;

    return_addresses->items[0] = *((GumReturnAddress *) GSIZE_TO_POINTER (
        GUM_CPU_CONTEXT_XSP (cpu_context)));
#elif defined (HAVE_ARM)
    cur = GSIZE_TO_POINTER (cpu_context->r[7]);

    has_ffi_frames = cpu_context->pc == 0;

    return_addresses->items[0] = GSIZE_TO_POINTER (cpu_context->lr);
#elif defined (HAVE_ARM64)
    cur = GSIZE_TO_POINTER (cpu_context->fp);

    has_ffi_frames = cpu_context->pc == 0;

    return_addresses->items[0] = GSIZE_TO_POINTER (cpu_context->lr);
#else
# error Unsupported architecture
#endif

    if (has_ffi_frames)
    {
      start_index = 0;
      n_skip = 2;
    }
    else
    {
      start_index = 1;
      n_skip = 0;

      return_addresses->items[0] = gum_strip_item (return_addresses->items[0]);
    }
  }
  else
  {
    cur = __builtin_frame_address (0);

    start_index = 0;
    n_skip = 0;
    has_ffi_frames = FALSE;
  }

  depth = MIN (limit, G_N_ELEMENTS (return_addresses->items));

  for (i = start_index;
      i < depth &&
      cur >= (gpointer *) stack_bottom &&
      cur <= (gpointer *) stack_top &&
      GUM_FP_IS_ALIGNED (cur);
      i++)
  {
    gpointer item, * next;

    item = *(cur + GUM_FP_LINK_OFFSET);
    if (item == NULL)
      break;
    return_addresses->items[i] = gum_strip_item (item);

    next = *cur;
    if (next <= cur)
      break;

#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
    if (has_ffi_frames && n_skip == 1)
      next = cur + GUM_FFI_STACK_SKIP + 1;
#elif defined (HAVE_ARM)
    if (has_ffi_frames && n_skip == 1)
    {
      ffi_next = next;
      next = cur + GUM_FFI_STACK_SKIP + 1;
    }
    else if (n_skip == 0 && ffi_next != NULL)
    {
      next = ffi_next;
      ffi_next = NULL;
    }
#endif

    cur = next;
    if (n_skip > 0)
    {
      n_skip--;
      i--;
    }
  }
  return_addresses->len = i;

  invocation_stack = gum_interceptor_get_current_stack ();
  for (i = 0; i != return_addresses->len; i++)
  {
    return_addresses->items[i] = gum_invocation_stack_translate (
        invocation_stack, return_addresses->items[i]);
  }
}

static gpointer
gum_strip_item (gpointer address)
{
#ifdef HAVE_ARM64
  /*
   * Even if the current program isn't using pointer authentication, it may be
   * running on a system where the shared cache is arm64e, which will result in
   * some stack frames using pointer authentication.
   */
  return GSIZE_TO_POINTER (
      GPOINTER_TO_SIZE (address) & G_GUINT64_CONSTANT (0x7fffffffff));
#else
  return address;
#endif
}
