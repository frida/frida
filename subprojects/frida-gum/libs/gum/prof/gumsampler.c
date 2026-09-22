/*
 * Copyright (C) 2008-2018 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2008 Christian Berentsen <jc.berentsen@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumsampler.h"

G_DEFINE_INTERFACE (GumSampler, gum_sampler, G_TYPE_OBJECT)

static void
gum_sampler_default_init (GumSamplerInterface * iface)
{
}

GumSample
gum_sampler_sample (GumSampler * self)
{
  GumSamplerInterface * iface = GUM_SAMPLER_GET_IFACE (self);

  g_assert (iface->sample != NULL);

  return iface->sample (self);
}
