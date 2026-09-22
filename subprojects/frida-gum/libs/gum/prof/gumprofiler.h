/*
 * Copyright (C) 2008-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2008 Christian Berentsen <jc.berentsen@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_PROFILER_H__
#define __GUM_PROFILER_H__

#include "gumprofilereport.h"
#include "gumsampler.h"

#include <gum/guminvocationcontext.h>

G_BEGIN_DECLS

#define GUM_TYPE_PROFILER (gum_profiler_get_type ())
G_DECLARE_FINAL_TYPE (GumProfiler, gum_profiler, GUM, PROFILER, GObject)

typedef enum
{
  GUM_INSTRUMENT_OK               =  0,
  GUM_INSTRUMENT_WRONG_SIGNATURE  = -1,
  GUM_INSTRUMENT_WAS_INSTRUMENTED = -2
} GumInstrumentReturn;

typedef gboolean (* GumFunctionMatchFilterFunc) (const gchar * function_name,
    gpointer user_data);
typedef void (* GumWorstCaseInspectorFunc) (GumInvocationContext * context,
    gchar * output_buf, guint output_buf_len, gpointer user_data);

GUM_API GumProfiler * gum_profiler_new (void);

GUM_API void gum_profiler_instrument_functions_matching (GumProfiler * self,
    const gchar * match_str, GumSampler * sampler,
    GumFunctionMatchFilterFunc filter_func, gpointer user_data);
GUM_API GumInstrumentReturn gum_profiler_instrument_function (
    GumProfiler * self, gpointer function_address, GumSampler * sampler);
GUM_API GumInstrumentReturn gum_profiler_instrument_function_with_inspector (
    GumProfiler * self, gpointer function_address, GumSampler * sampler,
    GumWorstCaseInspectorFunc inspector_func, gpointer data,
    GDestroyNotify data_destroy);

GUM_API GumProfileReport * gum_profiler_generate_report (GumProfiler * self);

GUM_API guint gum_profiler_get_number_of_threads (GumProfiler * self);
GUM_API GumSample gum_profiler_get_total_duration_of (GumProfiler * self,
    guint thread_index, gpointer function_address);
GUM_API GumSample gum_profiler_get_worst_case_duration_of (GumProfiler * self,
    guint thread_index, gpointer function_address);
GUM_API const gchar * gum_profiler_get_worst_case_info_of (GumProfiler * self,
    guint thread_index, gpointer function_address);

G_END_DECLS

#endif
