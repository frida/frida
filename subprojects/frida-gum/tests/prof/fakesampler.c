/*
 * Copyright (C) 2008-2018 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2008 Christian Berentsen <jc.berentsen@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "fakesampler.h"

struct _GumFakeSampler
{
  GObject parent;

  GumSample now;
};

static void gum_fake_sampler_iface_init (gpointer g_iface,
    gpointer iface_data);
static GumSample gum_fake_sampler_sample (GumSampler * sampler);

G_DEFINE_TYPE_EXTENDED (GumFakeSampler,
                        gum_fake_sampler,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_SAMPLER,
                                               gum_fake_sampler_iface_init))

static void
gum_fake_sampler_class_init (GumFakeSamplerClass * klass)
{
}

static void
gum_fake_sampler_iface_init (gpointer g_iface,
                             gpointer iface_data)
{
  GumSamplerInterface * iface = g_iface;

  iface->sample = gum_fake_sampler_sample;
}

static void
gum_fake_sampler_init (GumFakeSampler * self)
{
  self->now = 0;
}

GumSampler *
gum_fake_sampler_new (void)
{
  GumFakeSampler * sampler;

  sampler = g_object_new (GUM_TYPE_FAKE_SAMPLER, NULL);

  return GUM_SAMPLER (sampler);
}

void
gum_fake_sampler_advance (GumFakeSampler * self,
                          GumSample delta)
{
  self->now += delta;
}

static GumSample
gum_fake_sampler_sample (GumSampler * sampler)
{
  GumFakeSampler * self = GUM_FAKE_SAMPLER (sampler);
  return self->now;
}
