/*
 * Copyright (C) 2008-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2021 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumx86backtracer.h"

#include "guminterceptor.h"
#include "gummemorymap.h"

struct _GumX86Backtracer
{
  GObject parent;

  GumMemoryMap * code;
  GumMemoryMap * writable;
};

static void gum_x86_backtracer_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_x86_backtracer_dispose (GObject * object);
static void gum_x86_backtracer_generate (GumBacktracer * backtracer,
    const GumCpuContext * cpu_context, GumReturnAddressArray * return_addresses,
    guint limit);

G_DEFINE_TYPE_EXTENDED (GumX86Backtracer,
                        gum_x86_backtracer,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_BACKTRACER,
                                               gum_x86_backtracer_iface_init))

static void
gum_x86_backtracer_class_init (GumX86BacktracerClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_x86_backtracer_dispose;
}

static void
gum_x86_backtracer_iface_init (gpointer g_iface,
                               gpointer iface_data)
{
  GumBacktracerInterface * iface = g_iface;

  iface->generate = gum_x86_backtracer_generate;
}

static void
gum_x86_backtracer_init (GumX86Backtracer * self)
{
  self->code = gum_memory_map_new (GUM_PAGE_EXECUTE);
  self->writable = gum_memory_map_new (GUM_PAGE_WRITE);
}

static void
gum_x86_backtracer_dispose (GObject * object)
{
  GumX86Backtracer * self = GUM_X86_BACKTRACER (object);

  g_clear_object (&self->code);
  g_clear_object (&self->writable);

  G_OBJECT_CLASS (gum_x86_backtracer_parent_class)->dispose (object);
}

GumBacktracer *
gum_x86_backtracer_new (void)
{
  return g_object_new (GUM_TYPE_X86_BACKTRACER, NULL);
}

#define OPCODE_CALL_NEAR_RELATIVE     0xE8
#define OPCODE_CALL_NEAR_ABS_INDIRECT 0xFF

static void
gum_x86_backtracer_generate (GumBacktracer * backtracer,
                             const GumCpuContext * cpu_context,
                             GumReturnAddressArray * return_addresses,
                             guint limit)
{
  GumX86Backtracer * self;
  GumInvocationStack * invocation_stack;
  const gsize * start_address, * end_address;
  guint start_index, depth, n, i;
  GumMemoryRange stack_ranges[2];
  const gsize * p;

  self = GUM_X86_BACKTRACER (backtracer);
  invocation_stack = gum_interceptor_get_current_stack ();

  if (cpu_context != NULL)
  {
    start_address = GSIZE_TO_POINTER (GUM_CPU_CONTEXT_XSP (cpu_context) +
        sizeof (gpointer));
    return_addresses->items[0] = gum_invocation_stack_translate (
        invocation_stack, *((GumReturnAddress *) GSIZE_TO_POINTER (
            GUM_CPU_CONTEXT_XSP (cpu_context))));
    start_index = 1;
  }
  else
  {
    start_address = (gsize *) &backtracer;
    start_index = 0;
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

  depth = MIN (limit, G_N_ELEMENTS (return_addresses->items));

  for (i = start_index, p = start_address; p < end_address; p++)
  {
    gboolean valid = FALSE;
    gsize value;
    GumMemoryRange vr;

    if ((GPOINTER_TO_SIZE (p) & (4096 - 1)) == 0)
    {
      GumMemoryRange next_range;
      next_range.base_address = GUM_ADDRESS (p);
      next_range.size = 4096;
      if (!gum_memory_map_contains (self->writable, &next_range))
        break;
    }

    value = *p;
    vr.base_address = value - 6;
    vr.size = 6;

    if (value > 4096 + 6 && gum_memory_map_contains (self->code, &vr))
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
        guint8 * code_ptr = GSIZE_TO_POINTER (value);

        if (*(code_ptr - 5) == OPCODE_CALL_NEAR_RELATIVE ||
            *(code_ptr - 6) == OPCODE_CALL_NEAR_ABS_INDIRECT ||
            *(code_ptr - 3) == OPCODE_CALL_NEAR_ABS_INDIRECT ||
            *(code_ptr - 2) == OPCODE_CALL_NEAR_ABS_INDIRECT)
        {
          valid = TRUE;
        }
      }
    }

    if (valid)
    {
      return_addresses->items[i++] = GSIZE_TO_POINTER (value);
      if (i == depth)
        break;
    }
  }

  return_addresses->len = i;
}
