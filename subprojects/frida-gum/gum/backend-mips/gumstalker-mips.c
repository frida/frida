/*
 * Copyright (C) 2009-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumstalker.h"

#include "gumstalker-priv.h"

struct _GumStalker
{
  GObject parent;
};

G_DEFINE_TYPE (GumStalker, gum_stalker, G_TYPE_OBJECT)

gboolean
gum_stalker_is_supported (void)
{
  return FALSE;
}

static void
gum_stalker_class_init (GumStalkerClass * klass)
{
}

static void
gum_stalker_init (GumStalker * self)
{
}

GumStalker *
gum_stalker_new (void)
{
  return g_object_new (GUM_TYPE_STALKER, NULL);
}

void
gum_stalker_exclude (GumStalker * self,
                     const GumMemoryRange * range)
{
}

gint
gum_stalker_get_trust_threshold (GumStalker * self)
{
  return -1;
}

void
gum_stalker_set_trust_threshold (GumStalker * self,
                                 gint trust_threshold)
{
}

void
gum_stalker_flush (GumStalker * self)
{
}

void
gum_stalker_stop (GumStalker * self)
{
}

gboolean
gum_stalker_garbage_collect (GumStalker * self)
{
  return FALSE;
}

void
gum_stalker_follow_me (GumStalker * self,
                       GumStalkerTransformer * transformer,
                       GumEventSink * sink)
{
}

void
gum_stalker_unfollow_me (GumStalker * self)
{
}

gboolean
gum_stalker_is_following_me (GumStalker * self)
{
  return FALSE;
}

void
gum_stalker_follow (GumStalker * self,
                    GumThreadId thread_id,
                    GumStalkerTransformer * transformer,
                    GumEventSink * sink)
{
}

void
gum_stalker_unfollow (GumStalker * self,
                      GumThreadId thread_id)
{
}

void
gum_stalker_activate (GumStalker * self,
                      gconstpointer target)
{
}

void
gum_stalker_deactivate (GumStalker * self)
{
}

void
gum_stalker_set_observer (GumStalker * self,
                          GumStalkerObserver * observer)
{
}

void
gum_stalker_prefetch (GumStalker * self,
                      gconstpointer address,
                      gint recycle_count)
{
}

void
gum_stalker_prefetch_backpatch (GumStalker * self,
                                const GumBackpatch * backpatch)
{
}

void
gum_stalker_recompile (GumStalker * self,
                       gconstpointer address)
{
}

gpointer
gum_stalker_backpatch_get_from (const GumBackpatch * backpatch)
{
  return NULL;
}

gpointer
gum_stalker_backpatch_get_to (const GumBackpatch * backpatch)
{
  return NULL;
}

void
gum_stalker_invalidate (GumStalker * self,
                        gconstpointer address)
{
}

void
gum_stalker_invalidate_for_thread (GumStalker * self,
                                   GumThreadId thread_id,
                                   gconstpointer address)
{
}

GumProbeId
gum_stalker_add_call_probe (GumStalker * self,
                            gpointer target_address,
                            GumCallProbeCallback callback,
                            gpointer data,
                            GDestroyNotify notify)
{
  return 0;
}

void
gum_stalker_remove_call_probe (GumStalker * self,
                               GumProbeId id)
{
}

void
_gum_stalker_modify_to_run_on_thread (GumStalker * self,
                                      GumThreadId thread_id,
                                      GumCpuContext * cpu_context,
                                      GumStalkerRunOnThreadFunc func,
                                      gpointer data)
{
}

gboolean
gum_stalker_iterator_next (GumStalkerIterator * self,
                           const cs_insn ** insn)
{
  return FALSE;
}

void
gum_stalker_iterator_keep (GumStalkerIterator * self)
{
}

GumMemoryAccess
gum_stalker_iterator_get_memory_access (GumStalkerIterator * self)
{
  return GUM_MEMORY_ACCESS_OPEN;
}

void
gum_stalker_iterator_put_callout (GumStalkerIterator * self,
                                  GumStalkerCallout callout,
                                  gpointer data,
                                  GDestroyNotify data_destroy)
{
}

void
gum_stalker_iterator_put_chaining_return (GumStalkerIterator * self)
{
}

csh
gum_stalker_iterator_get_capstone (GumStalkerIterator * self)
{
  return 0;
}
