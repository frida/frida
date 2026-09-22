/*
 * Copyright (C) 2023 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023 Håvard Sørbø <havard@hsorbo.no>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gum.h"

#ifdef HAVE_DARWIN
# include <os/signpost.h>

static os_log_t gum_log;
#endif

static gboolean on_match (const GumApiDetails * details, gpointer user_data);

static GumApiResolver * resolver;

void
init (void)
{
  gum_init_embedded ();

  resolver = gum_api_resolver_make ("swift");
  g_assert_nonnull (resolver);

#ifdef HAVE_DARWIN
  gum_log = os_log_create ("re.frida.gum",
      OS_LOG_CATEGORY_POINTS_OF_INTEREST);
#endif
}

void
finalize (void)
{
  g_object_unref (resolver);

  gum_deinit_embedded ();
}

guint
run (const gchar * query)
{
  guint num_matches = 0;

#ifdef HAVE_DARWIN
  os_signpost_id_t id;

  if (__builtin_available (macOS 10.14, iOS 12.0, *))
  {
    id = os_signpost_id_generate (gum_log);
    os_signpost_interval_begin (gum_log, id, "enumerate_matches",
        "query='%{public}s'", query);
  }
#endif

  gum_api_resolver_enumerate_matches (resolver, query, on_match, &num_matches,
      NULL);

#ifdef HAVE_DARWIN
  if (__builtin_available (macOS 10.14, iOS 12.0, *))
  {
    os_signpost_interval_end (gum_log, id, "enumerate_matches",
        "num_matches=%u", num_matches);
  }
#endif

  return num_matches;
}

static gboolean
on_match (const GumApiDetails * details,
          gpointer user_data)
{
  guint * num_matches = user_data;

  (*num_matches)++;

  return TRUE;
}
