/*
 * Copyright (C) 2010-2019 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_SANITY_CHECKER_H__
#define __GUM_SANITY_CHECKER_H__

#include <gum/gumheapapi.h>

G_BEGIN_DECLS

typedef guint GumSanityCheckFlags;

typedef struct _GumSanityChecker GumSanityChecker;
typedef struct _GumSanityCheckerPrivate GumSanityCheckerPrivate;

typedef void (* GumSanityOutputFunc) (const gchar * text, gpointer user_data);
typedef void (* GumSanitySequenceFunc) (gpointer user_data);

enum _GumSanityCheckFlags
{
  GUM_CHECK_INSTANCE_LEAKS  = (1 << 0),
  GUM_CHECK_BLOCK_LEAKS     = (1 << 1),
  GUM_CHECK_BOUNDS          = (1 << 2)
};

struct _GumSanityChecker
{
  GumSanityCheckerPrivate * priv;
};

GUM_API GumSanityChecker * gum_sanity_checker_new (GumSanityOutputFunc func,
    gpointer user_data);
GUM_API GumSanityChecker * gum_sanity_checker_new_with_heap_apis (
    const GumHeapApiList * heap_apis, GumSanityOutputFunc func,
    gpointer user_data);
GUM_API void gum_sanity_checker_destroy (GumSanityChecker * checker);

GUM_API void gum_sanity_checker_enable_backtraces_for_blocks_of_all_sizes (
  GumSanityChecker * self);
GUM_API void gum_sanity_checker_enable_backtraces_for_blocks_of_size (
    GumSanityChecker * self, guint size);
GUM_API void gum_sanity_checker_set_front_alignment_granularity (
    GumSanityChecker * self, guint granularity);

GUM_API gboolean gum_sanity_checker_run (GumSanityChecker * self,
    GumSanitySequenceFunc func, gpointer user_data);

GUM_API void gum_sanity_checker_begin (GumSanityChecker * self, guint flags);
GUM_API gboolean gum_sanity_checker_end (GumSanityChecker * self);

G_END_DECLS

#endif
