/*
 * Copyright (C) 2008-2018 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2008 Christian Berentsen <jc.berentsen@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_PROFILE_REPORT_H__
#define __GUM_PROFILE_REPORT_H__

#include "gumsampler.h"

G_BEGIN_DECLS

#define GUM_TYPE_PROFILE_REPORT (gum_profile_report_get_type ())
G_DECLARE_FINAL_TYPE (GumProfileReport, gum_profile_report, GUM, PROFILE_REPORT,
    GObject)

typedef struct _GumProfileReportNode GumProfileReportNode;

struct _GumProfileReportNode
{
  gchar * name;
  guint64 total_calls;
  GumSample total_duration;
  GumSample worst_case_duration;
  gchar * worst_case_info;
  GumProfileReportNode * child;
};

GUM_API GumProfileReport * gum_profile_report_new (void);

GUM_API gchar * gum_profile_report_emit_xml (GumProfileReport * self);

GUM_API GPtrArray * gum_profile_report_get_root_nodes_for_thread (
    GumProfileReport * self, guint thread_index);

void _gum_profile_report_append_thread_root_node (
    GumProfileReport * self, guint thread_id,
    GumProfileReportNode * root_node);
void _gum_profile_report_sort (GumProfileReport * self);

G_END_DECLS

#endif
