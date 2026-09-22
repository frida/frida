/*
 * Copyright (C) 2008-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2008 Christian Berentsen <jc.berentsen@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumcyclesampler.h"

#include "gumx86writer.h"
#include "gummemory.h"

typedef void (GUM_X86_THUNK * ReadTimestampCounterFunc) (GumSample * sample);

struct _GumCycleSampler
{
  GObject parent;

  ReadTimestampCounterFunc read_timestamp_counter;

  gpointer code;
};

static void gum_cycle_sampler_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_cycle_sampler_finalize (GObject * object);
static GumSample gum_cycle_sampler_sample (GumSampler * sampler);

G_DEFINE_TYPE_EXTENDED (GumCycleSampler,
                        gum_cycle_sampler,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_SAMPLER,
                                               gum_cycle_sampler_iface_init))

static void
gum_cycle_sampler_class_init (GumCycleSamplerClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gum_cycle_sampler_finalize;
}

static void
gum_cycle_sampler_iface_init (gpointer g_iface,
                              gpointer iface_data)
{
  GumSamplerInterface * iface = g_iface;

  iface->sample = gum_cycle_sampler_sample;
}

static void
gum_cycle_sampler_init (GumCycleSampler * self)
{
  gsize page_size;
  GumX86Writer cw;
  GumX86Reg first_arg_reg;

  page_size = gum_query_page_size ();
  self->code = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RWX);
  gum_x86_writer_init (&cw, self->code);
  gum_x86_writer_put_lfence (&cw);
  gum_x86_writer_put_rdtsc (&cw);
  first_arg_reg = gum_x86_writer_get_cpu_register_for_nth_argument (&cw, 0);
  gum_x86_writer_put_mov_reg_ptr_reg (&cw, first_arg_reg, GUM_X86_EAX);
  gum_x86_writer_put_mov_reg_offset_ptr_reg (&cw, first_arg_reg, 4,
      GUM_X86_EDX);
  gum_x86_writer_put_ret (&cw);
  gum_x86_writer_clear (&cw);

  self->read_timestamp_counter =
      GUM_POINTER_TO_FUNCPTR (ReadTimestampCounterFunc, self->code);
}

static void
gum_cycle_sampler_finalize (GObject * object)
{
  GumCycleSampler * self = GUM_CYCLE_SAMPLER (object);

  gum_memory_free (self->code, gum_query_page_size ());

  G_OBJECT_CLASS (gum_cycle_sampler_parent_class)->finalize (object);
}

GumSampler *
gum_cycle_sampler_new (void)
{
  return g_object_new (GUM_TYPE_CYCLE_SAMPLER, NULL);
}

gboolean
gum_cycle_sampler_is_available (GumCycleSampler * self)
{
  return TRUE;
}

static GumSample
gum_cycle_sampler_sample (GumSampler * sampler)
{
  GumSample result;

  GUM_CYCLE_SAMPLER (sampler)->read_timestamp_counter (&result);

  return result;
}
