/*
 * Copyright (C) 2009-2018 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2009 Christian Berentsen <jc.berentsen@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumwallclocksampler.h"

struct _GumWallClockSampler
{
  GObject parent;
};

static void gum_wall_clock_sampler_iface_init (gpointer g_iface,
    gpointer iface_data);
static GumSample gum_wall_clock_sampler_sample (GumSampler * sampler);

G_DEFINE_TYPE_EXTENDED (GumWallClockSampler,
                        gum_wall_clock_sampler,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_SAMPLER,
                            gum_wall_clock_sampler_iface_init))

static void
gum_wall_clock_sampler_class_init (GumWallClockSamplerClass * klass)
{
}

static void
gum_wall_clock_sampler_iface_init (gpointer g_iface,
                                   gpointer iface_data)
{
  GumSamplerInterface * iface = g_iface;

  iface->sample = gum_wall_clock_sampler_sample;
}

static void
gum_wall_clock_sampler_init (GumWallClockSampler * self)
{
}

GumSampler *
gum_wall_clock_sampler_new (void)
{
  return g_object_new (GUM_TYPE_WALL_CLOCK_SAMPLER, NULL);
}

static GumSample
gum_wall_clock_sampler_sample (GumSampler * sampler)
{
  return g_get_monotonic_time ();
}
