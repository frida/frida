/*
 * Copyright (C) 2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumcyclesampler.h"

#include <time.h>

struct _GumCycleSampler
{
  GObject parent;
};

static void gum_cycle_sampler_iface_init (gpointer g_iface,
    gpointer iface_data);
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
  struct timespec t;

  if (clock_gettime (CLOCK_PROF, &t) != 0)
    return 0;

  return (t.tv_sec * G_GUINT64_CONSTANT (1000000000)) + t.tv_nsec;
}
