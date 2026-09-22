/*
 * Copyright (C) 2008-2018 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2008 Christian Berentsen <jc.berentsen@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __FAKE_SAMPLER_H__
#define __FAKE_SAMPLER_H__

#include <glib-object.h>
#include <gum/prof/gumsampler.h>

G_BEGIN_DECLS

#define GUM_TYPE_FAKE_SAMPLER (gum_fake_sampler_get_type ())
G_DECLARE_FINAL_TYPE (GumFakeSampler, gum_fake_sampler, GUM, FAKE_SAMPLER,
    GObject)

GumSampler * gum_fake_sampler_new (void);

void gum_fake_sampler_advance (GumFakeSampler * self, GumSample delta);

G_END_DECLS

#endif
