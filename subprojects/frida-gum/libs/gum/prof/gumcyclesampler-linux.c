/*
 * Copyright (C) 2015-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumcyclesampler.h"

#include "gumlibc.h"

#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

# define PERF_TYPE_HARDWARE       0
# define PERF_COUNT_HW_CPU_CYCLES 0

#ifndef __NR_perf_event_open
# if defined (HAVE_ARM)
#  define __NR_perf_event_open (__NR_SYSCALL_BASE + 364)
# elif defined (HAVE_MIPS)
#  if _MIPS_SIM == _MIPS_SIM_ABI32
#   define __NR_perf_event_open 4333
#  elif _MIPS_SIM == _MIPS_SIM_ABI64
#   define __NR_perf_event_open 5292
#  elif _MIPS_SIM == _MIPS_SIM_NABI32
#   define __NR_perf_event_open 6296
#  else
#   error Unexpected MIPS ABI
#  endif
# else
#  error Please implement for your architecture
# endif
#endif

struct _GumCycleSampler
{
  GObject parent;

  gint device;
};

struct perf_event_attr
{
  guint32 type;
  guint32 size;
  guint64 config;

  union
  {
    guint64 sample_period;
    guint64 sample_freq;
  };

  guint64 sample_type;
  guint64 read_format;

  guint64 disabled       :  1,
          inherit        :  1,
          pinned         :  1,
          exclusive      :  1,
          exclude_user   :  1,
          exclude_kernel :  1,
          exclude_hv     :  1,
          exclude_idle   :  1,
          mmap           :  1,
          comm           :  1,
          freq           :  1,
          inherit_stat   :  1,
          enable_on_exec :  1,
          task           :  1,
          watermark      :  1,
          __reserved_1   : 49;

  union
  {
    guint32 wakeup_events;
    guint32 wakeup_watermark;
  };

  guint32 __reserved_2;
  guint64 __reserved_3;
};

static void gum_cycle_sampler_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_cycle_sampler_dispose (GObject * object);
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

  object_class->dispose = gum_cycle_sampler_dispose;
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
  struct perf_event_attr attr = { 0, };

  attr.type = PERF_TYPE_HARDWARE;
  attr.config = PERF_COUNT_HW_CPU_CYCLES;

  self->device = syscall (__NR_perf_event_open, &attr, 0, -1, -1, 0);
}

static void
gum_cycle_sampler_dispose (GObject * object)
{
  GumCycleSampler * self = GUM_CYCLE_SAMPLER (object);

  if (self->device != -1)
  {
    close (self->device);
    self->device = -1;
  }

  G_OBJECT_CLASS (gum_cycle_sampler_parent_class)->dispose (object);
}

GumSampler *
gum_cycle_sampler_new (void)
{
  return g_object_new (GUM_TYPE_CYCLE_SAMPLER, NULL);
}

gboolean
gum_cycle_sampler_is_available (GumCycleSampler * self)
{
  return self->device != -1;
}

static GumSample
gum_cycle_sampler_sample (GumSampler * sampler)
{
  GumCycleSampler * self = (GumCycleSampler *) sampler;
  long long result = 0;

  if (read (self->device, &result, sizeof (result)) < sizeof (result))
    return 0;

  return result;
}
