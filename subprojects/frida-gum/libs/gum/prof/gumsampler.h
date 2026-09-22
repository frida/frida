/*
 * Copyright (C) 2008-2018 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2008 Christian Berentsen <jc.berentsen@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_SAMPLER_H__
#define __GUM_SAMPLER_H__

#include <glib-object.h>
#include <gum/gumdefs.h>

G_BEGIN_DECLS

#define GUM_TYPE_SAMPLER (gum_sampler_get_type ())
G_DECLARE_INTERFACE (GumSampler, gum_sampler, GUM, SAMPLER, GObject)

typedef guint64 GumSample;

struct _GumSamplerInterface
{
  GTypeInterface parent;

  GumSample (* sample) (GumSampler * self);
};

GUM_API GumSample gum_sampler_sample (GumSampler * self);

G_END_DECLS

#endif
