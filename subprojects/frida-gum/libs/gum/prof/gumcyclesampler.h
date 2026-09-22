/*
 * Copyright (C) 2008-2018 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2008 Christian Berentsen <jc.berentsen@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_CYCLE_SAMPLER_H__
#define __GUM_CYCLE_SAMPLER_H__

#include "gumsampler.h"

G_BEGIN_DECLS

#define GUM_TYPE_CYCLE_SAMPLER (gum_cycle_sampler_get_type ())
G_DECLARE_FINAL_TYPE (GumCycleSampler, gum_cycle_sampler, GUM, CYCLE_SAMPLER,
    GObject)

GUM_API GumSampler * gum_cycle_sampler_new (void);

GUM_API gboolean gum_cycle_sampler_is_available (GumCycleSampler * self);

G_END_DECLS

#endif
