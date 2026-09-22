/*
 * Copyright (C) 2009-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2009 Christian Berentsen <jc.berentsen@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_WALL_CLOCK_SAMPLER_H__
#define __GUM_WALL_CLOCK_SAMPLER_H__

#include "gumsampler.h"

G_BEGIN_DECLS

#define GUM_TYPE_WALL_CLOCK_SAMPLER (gum_wall_clock_sampler_get_type ())
G_DECLARE_FINAL_TYPE (GumWallClockSampler, gum_wall_clock_sampler, GUM,
    WALL_CLOCK_SAMPLER, GObject)

GUM_API GumSampler * gum_wall_clock_sampler_new (void);

G_END_DECLS

#endif
