/*
 * Copyright (C) 2015-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2021 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumarm64backtracer.h"

#include "guminterceptor.h"
#include "gummemorymap.h"

#ifdef _MSC_VER
# include <intrin.h>
#endif

struct _GumArm64Backtracer
{
  GObject parent;

  GumMemoryMap * code;
  GumMemoryMap * writable;
};

static void gum_arm64_backtracer_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_arm64_backtracer_dispose (GObject * object);
static void gum_arm64_backtracer_generate (GumBacktracer * backtracer,
    const GumCpuContext * cpu_context, GumReturnAddressArray * return_addresses,
    guint limit);

static gsize gum_strip_item (gsize address);

G_DEFINE_TYPE_EXTENDED (GumArm64Backtracer,
                        gum_arm64_backtracer,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_BACKTRACER,
                                               gum_arm64_backtracer_iface_init))

static void
gum_arm64_backtracer_class_init (GumArm64BacktracerClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_arm64_backtracer_dispose;
}

static void
gum_arm64_backtracer_iface_init (gpointer g_iface,
                                 gpointer iface_data)
{
  GumBacktracerInterface * iface = g_iface;

  iface->generate = gum_arm64_backtracer_generate;
}

static void
gum_arm64_backtracer_init (GumArm64Backtracer * self)
{
  self->code = gum_memory_map_new (GUM_PAGE_EXECUTE);
  self->writable = gum_memory_map_new (GUM_PAGE_WRITE);
}

static void
gum_arm64_backtracer_dispose (GObject * object)
{
  GumArm64Backtracer * self = GUM_ARM64_BACKTRACER (object);

  g_clear_object (&self->code);
  g_clear_object (&self->writable);

  G_OBJECT_CLASS (gum_arm64_backtracer_parent_class)->dispose (object);
}

GumBacktracer *
gum_arm64_backtracer_new (void)
{
  return g_object_new (GUM_TYPE_ARM64_BACKTRACER, NULL);
}

static void
gum_arm64_backtracer_generate (GumBacktracer * backtracer,
                               const GumCpuContext * cpu_context,
                               GumReturnAddressArray * return_addresses,
                               guint limit)
{
  GumArm64Backtracer * self;
  GumInvocationStack * invocation_stack;
  const gsize * start_address, * end_address;
  guint start_index, skips_pending, depth, n, i;
  GumMemoryRange stack_ranges[2];
  gsize page_size;
  const gsize * p;

  self = GUM_ARM64_BACKTRACER (backtracer);
  invocation_stack = gum_interceptor_get_current_stack ();

  if (cpu_context != NULL)
  {
    start_address = GSIZE_TO_POINTER (cpu_context->sp);
    return_addresses->items[0] = gum_invocation_stack_translate (
        invocation_stack, GSIZE_TO_POINTER (cpu_context->lr));
    start_index = 1;
    skips_pending = 0;
  }
  else
  {
#ifdef _MSC_VER
    start_address = _AddressOfReturnAddress ();
#else
    asm ("\tmov %0, sp" : "=r" (start_address));
#endif
    start_index = 0;
    skips_pending = 1;
  }

  end_address = start_address + 2048;

  n = gum_thread_try_get_ranges (stack_ranges, G_N_ELEMENTS (stack_ranges));
  for (i = 0; i != n; i++)
  {
    const GumMemoryRange * r = &stack_ranges[i];

    if (GUM_MEMORY_RANGE_INCLUDES (r, GUM_ADDRESS (start_address)))
    {
      end_address = GSIZE_TO_POINTER (r->base_address + r->size);
      break;
    }
  }

  page_size = gum_query_page_size ();

  depth = MIN (limit, G_N_ELEMENTS (return_addresses->items));

  for (i = start_index, p = start_address; p < end_address; p++)
  {
    gboolean valid = FALSE;
    gsize value;
    GumMemoryRange vr;

    if ((GPOINTER_TO_SIZE (p) & (page_size - 1)) == 0)
    {
      GumMemoryRange next_range;
      next_range.base_address = GUM_ADDRESS (p);
      next_range.size = page_size;
      if (!gum_memory_map_contains (self->writable, &next_range))
        break;
    }

    value = gum_strip_item (*p);

    vr.base_address = value - 4;
    vr.size = 4;

    if (value > page_size + 4 &&
        (value & 0x3) == 0 &&
        gum_memory_map_contains (self->code, &vr))
    {
      gsize translated_value;

      translated_value = GPOINTER_TO_SIZE (gum_invocation_stack_translate (
          invocation_stack, GSIZE_TO_POINTER (value)));
      if (translated_value != value)
      {
        value = translated_value;
        valid = TRUE;
      }
      else
      {
        const guint32 insn = *((guint32 *) GSIZE_TO_POINTER (value - 4));
        if ((insn & 0xfc000000) == 0x94000000)
        {
          /* BL <imm26> */
          valid = TRUE;
        }
        else if ((insn & 0xfffffc1f) == 0xd63f0000)
        {
          /* BLR <reg> */
          valid = TRUE;
        }
        else if ((insn & 0xfffffc1f) == 0xd63f081f)
        {
          /* BLRAAZ <reg> */
          valid = TRUE;
        }
      }
    }

    if (valid)
    {
      if (skips_pending == 0)
      {
        return_addresses->items[i++] = GSIZE_TO_POINTER (value);
        if (i == depth)
          break;
      }
      else
      {
        skips_pending--;
      }
    }
  }

  return_addresses->len = i;
}

static gsize
gum_strip_item (gsize address)
{
#ifdef HAVE_DARWIN
  /*
   * Even if the current program isn't using pointer authentication, it may be
   * running on a system where the shared cache is arm64e, which will result in
   * some stack frames using pointer authentication.
   */
  return address & G_GUINT64_CONSTANT (0x7fffffffff);
#else
  return address;
#endif
}
