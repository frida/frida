/*
 * Copyright (C) 2015-2021 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2021 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummipsbacktracer.h"

#include "guminterceptor.h"
#include "gummemorymap.h"

struct _GumMipsBacktracer
{
  GObject parent;

  GumMemoryMap * code;
  GumMemoryMap * writable;
};

static void gum_mips_backtracer_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_mips_backtracer_dispose (GObject * object);
static void gum_mips_backtracer_generate (GumBacktracer * backtracer,
    const GumCpuContext * cpu_context, GumReturnAddressArray * return_addresses,
    guint limit);

G_DEFINE_TYPE_EXTENDED (GumMipsBacktracer,
                        gum_mips_backtracer,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_BACKTRACER,
                                               gum_mips_backtracer_iface_init))

static void
gum_mips_backtracer_class_init (GumMipsBacktracerClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_mips_backtracer_dispose;
}

static void
gum_mips_backtracer_iface_init (gpointer g_iface,
                                gpointer iface_data)
{
  GumBacktracerInterface * iface = g_iface;

  iface->generate = gum_mips_backtracer_generate;
}

static void
gum_mips_backtracer_init (GumMipsBacktracer * self)
{
  self->code = gum_memory_map_new (GUM_PAGE_EXECUTE);
  self->writable = gum_memory_map_new (GUM_PAGE_WRITE);
}

static void
gum_mips_backtracer_dispose (GObject * object)
{
  GumMipsBacktracer * self = GUM_MIPS_BACKTRACER (object);

  g_clear_object (&self->code);
  g_clear_object (&self->writable);

  G_OBJECT_CLASS (gum_mips_backtracer_parent_class)->dispose (object);
}

GumBacktracer *
gum_mips_backtracer_new (void)
{
  return g_object_new (GUM_TYPE_MIPS_BACKTRACER, NULL);
}

static void
gum_mips_backtracer_generate (GumBacktracer * backtracer,
                              const GumCpuContext * cpu_context,
                              GumReturnAddressArray * return_addresses,
                              guint limit)
{
  GumMipsBacktracer * self;
  GumInvocationStack * invocation_stack;
  const gsize * start_address, * end_address;
  guint start_index, skips_pending, depth, n, i;
  GumMemoryRange stack_ranges[2];
  const gsize * p;

  self = GUM_MIPS_BACKTRACER (backtracer);
  invocation_stack = gum_interceptor_get_current_stack ();

  if (cpu_context != NULL)
  {
    start_address = GSIZE_TO_POINTER (cpu_context->sp);
    return_addresses->items[0] = gum_invocation_stack_translate (
        invocation_stack, GSIZE_TO_POINTER (cpu_context->ra));
    start_index = 1;
    skips_pending = 0;
  }
  else
  {
    asm ("\tmove %0, $sp" : "=r" (start_address));
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
    vr.base_address = value - 8;
    vr.size = 4;

    if (value > 4096 + 4 &&
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
        const guint32 insn = *((guint32 *) GSIZE_TO_POINTER (value - 8));
        if ((insn & 0xfc000000) == 0x0c000000)
        {
          /* JAL <imm26> */
          valid = TRUE;
        }
        else if ((insn & 0xfc00ffff) == 0x0000f809)
        {
          /* JALR $ra, <reg> */
          valid = TRUE;
        }
        else if ((insn & 0xfc1f0000) == 0x04110000)
        {
          /* BGEZAL $rs, <imm16> */
          valid = TRUE;
        }
        else if ((insn & 0xfc1f0000) == 0x04130000)
        {
          /* BGEZALL $rs, <imm16> */
          valid = TRUE;
        }
        else if ((insn & 0xfc1f0000) == 0x04100000)
        {
          /* BLTZAL $rs, <imm16> */
          valid = TRUE;
        }
        else if ((insn & 0xfc1f0000) == 0x04120000)
        {
          /* BLTZALL $rs, <imm16> */
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
