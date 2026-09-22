/*
 * Copyright (C) 2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_USER_TIME_SAMPLER_H__
#define __GUM_USER_TIME_SAMPLER_H__

#include "gumsampler.h"

#include <gum/gumprocess.h>

G_BEGIN_DECLS

#define GUM_TYPE_USER_TIME_SAMPLER (gum_user_time_sampler_get_type ())
G_DECLARE_FINAL_TYPE (GumUserTimeSampler, gum_user_time_sampler, GUM,
                      USER_TIME_SAMPLER, GObject)

GUM_API GumSampler * gum_user_time_sampler_new (void);
GUM_API GumSampler * gum_user_time_sampler_new_with_thread_id (
    GumThreadId thread_id);

GUM_API gboolean gum_user_time_sampler_is_available (GumUserTimeSampler * self);

G_END_DECLS

#endif
