/*
 * Copyright (C) 2008-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2008 Christian Berentsen <jc.berentsen@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_CALL_COUNT_SAMPLER_H__
#define __GUM_CALL_COUNT_SAMPLER_H__

#include "gumsampler.h"

G_BEGIN_DECLS

#define GUM_TYPE_CALL_COUNT_SAMPLER (gum_call_count_sampler_get_type ())
G_DECLARE_FINAL_TYPE (GumCallCountSampler, gum_call_count_sampler, GUM,
    CALL_COUNT_SAMPLER, GObject)

GUM_API GumSampler * gum_call_count_sampler_new (gpointer first_function, ...);
GUM_API GumSampler * gum_call_count_sampler_new_valist (gpointer first_function,
    va_list args);
GUM_API GumSampler * gum_call_count_sampler_newv (gpointer * functions,
    guint n_functions);
GUM_API GumSampler * gum_call_count_sampler_new_by_name (
    const gchar * first_function_name, ...);
GUM_API GumSampler * gum_call_count_sampler_new_by_name_valist (
    const gchar * first_function_name, va_list args);

GUM_API void gum_call_count_sampler_add_function (GumCallCountSampler * self,
    gpointer function);

GUM_API GumSample gum_call_count_sampler_peek_total_count (
    GumCallCountSampler * self);

G_END_DECLS

#endif
