/*
 * Copyright (C) 2009-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C)      2010 Karl Trygve Kalleberg <karltk@boblycat.org>
 * Copyright (C)      2023 Håvard Sørbø <havard@hsorbo.no>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_STALKER_H__
#define __GUM_STALKER_H__

#include <capstone.h>
#include <gum/arch-x86/gumx86writer.h>
#include <gum/arch-arm/gumarmwriter.h>
#include <gum/arch-arm/gumthumbwriter.h>
#include <gum/arch-arm64/gumarm64writer.h>
#include <gum/arch-mips/gummipswriter.h>
#include <gum/gumdefs.h>
#include <gum/gumeventsink.h>
#include <gum/gumprocess.h>

G_BEGIN_DECLS

#define GUM_TYPE_STALKER (gum_stalker_get_type ())
G_DECLARE_FINAL_TYPE (GumStalker, gum_stalker, GUM, STALKER, GObject)

#define GUM_TYPE_STALKER_TRANSFORMER (gum_stalker_transformer_get_type ())
G_DECLARE_INTERFACE (GumStalkerTransformer, gum_stalker_transformer, GUM,
                     STALKER_TRANSFORMER, GObject)

#define GUM_TYPE_DEFAULT_STALKER_TRANSFORMER \
    (gum_default_stalker_transformer_get_type ())
G_DECLARE_FINAL_TYPE (GumDefaultStalkerTransformer,
                      gum_default_stalker_transformer,
                      GUM, DEFAULT_STALKER_TRANSFORMER,
                      GObject)

#define GUM_TYPE_CALLBACK_STALKER_TRANSFORMER \
    (gum_callback_stalker_transformer_get_type ())
G_DECLARE_FINAL_TYPE (GumCallbackStalkerTransformer,
                      gum_callback_stalker_transformer,
                      GUM, CALLBACK_STALKER_TRANSFORMER,
                      GObject)

#define GUM_TYPE_STALKER_OBSERVER (gum_stalker_observer_get_type ())
G_DECLARE_INTERFACE (GumStalkerObserver, gum_stalker_observer, GUM,
                     STALKER_OBSERVER, GObject)

typedef struct _GumStalkerIterator GumStalkerIterator;
typedef struct _GumStalkerOutput GumStalkerOutput;
typedef struct _GumBackpatch GumBackpatch;
typedef struct _GumBackpatchInstruction GumBackpatchInstruction;
typedef void (* GumStalkerIncrementFunc) (GumStalkerObserver * self);
typedef void (* GumStalkerNotifyBackpatchFunc) (GumStalkerObserver * self,
    const GumBackpatch * backpatch, gsize size);
typedef void (* GumStalkerSwitchCallbackFunc) (GumStalkerObserver * self,
    gpointer from_address, gpointer start_address, gpointer from_insn,
    gpointer * target);
typedef union _GumStalkerWriter GumStalkerWriter;
typedef void (* GumStalkerTransformerCallback) (GumStalkerIterator * iterator,
    GumStalkerOutput * output, gpointer user_data);
typedef void (* GumStalkerCallout) (GumCpuContext * cpu_context,
    gpointer user_data);

typedef guint GumProbeId;
typedef struct _GumCallDetails GumCallDetails;
typedef void (* GumCallProbeCallback) (GumCallDetails * details,
    gpointer user_data);
typedef void (* GumStalkerRunOnThreadFunc) (const GumCpuContext * cpu_context,
    gpointer user_data);

struct _GumStalkerTransformerInterface
{
  GTypeInterface parent;

  void (* transform_block) (GumStalkerTransformer * self,
      GumStalkerIterator * iterator, GumStalkerOutput * output);
};

struct _GumStalkerObserverInterface
{
  GTypeInterface parent;

  /* Common */
  GumStalkerIncrementFunc increment_total;

  GumStalkerIncrementFunc increment_call_imm;
  GumStalkerIncrementFunc increment_call_reg;

  /* x86 only */
  GumStalkerIncrementFunc increment_call_mem;

  /* Arm64 only */
  GumStalkerIncrementFunc increment_excluded_call_reg;

  /* x86 only */
  GumStalkerIncrementFunc increment_ret_slow_path;

  /* Arm64 only */
  GumStalkerIncrementFunc increment_ret;

  /* Common */
  GumStalkerIncrementFunc increment_post_call_invoke;
  GumStalkerIncrementFunc increment_excluded_call_imm;

  /* Common */
  GumStalkerIncrementFunc increment_jmp_imm;
  GumStalkerIncrementFunc increment_jmp_reg;

  /* x86 only */
  GumStalkerIncrementFunc increment_jmp_mem;
  GumStalkerIncrementFunc increment_jmp_cond_imm;
  GumStalkerIncrementFunc increment_jmp_cond_mem;
  GumStalkerIncrementFunc increment_jmp_cond_reg;
  GumStalkerIncrementFunc increment_jmp_cond_jcxz;

  /* Arm64 only */
  GumStalkerIncrementFunc increment_jmp_cond_cc;
  GumStalkerIncrementFunc increment_jmp_cond_cbz;
  GumStalkerIncrementFunc increment_jmp_cond_cbnz;
  GumStalkerIncrementFunc increment_jmp_cond_tbz;
  GumStalkerIncrementFunc increment_jmp_cond_tbnz;

  /* Common */
  GumStalkerIncrementFunc increment_jmp_continuation;

  /* x86 only */
  GumStalkerIncrementFunc increment_sysenter_slow_path;

  GumStalkerNotifyBackpatchFunc notify_backpatch;

  GumStalkerSwitchCallbackFunc switch_callback;
};

union _GumStalkerWriter
{
  gpointer instance;
  GumX86Writer * x86;
  GumArmWriter * arm;
  GumThumbWriter * thumb;
  GumArm64Writer * arm64;
  GumMipsWriter * mips;
};

struct _GumStalkerOutput
{
  GumStalkerWriter writer;
  GumInstructionEncoding encoding;
};

struct _GumCallDetails
{
  gpointer target_address;
  gpointer return_address;
  gpointer stack_data;
  GumCpuContext * cpu_context;
};

GUM_API gboolean gum_stalker_is_supported (void);

GUM_API GumStalker * gum_stalker_new (void);

GUM_API void gum_stalker_exclude (GumStalker * self,
    const GumMemoryRange * range);

GUM_API gint gum_stalker_get_trust_threshold (GumStalker * self);
GUM_API void gum_stalker_set_trust_threshold (GumStalker * self,
    gint trust_threshold);

GUM_API void gum_stalker_flush (GumStalker * self);
GUM_API void gum_stalker_stop (GumStalker * self);
GUM_API gboolean gum_stalker_garbage_collect (GumStalker * self);

GUM_API void gum_stalker_follow_me (GumStalker * self,
    GumStalkerTransformer * transformer, GumEventSink * sink);
GUM_API void gum_stalker_unfollow_me (GumStalker * self);
GUM_API gboolean gum_stalker_is_following_me (GumStalker * self);

GUM_API void gum_stalker_follow (GumStalker * self, GumThreadId thread_id,
    GumStalkerTransformer * transformer, GumEventSink * sink);
GUM_API void gum_stalker_unfollow (GumStalker * self, GumThreadId thread_id);

GUM_API void gum_stalker_activate (GumStalker * self, gconstpointer target);
GUM_API void gum_stalker_deactivate (GumStalker * self);

GUM_API void gum_stalker_set_observer (GumStalker * self,
    GumStalkerObserver * observer);

GUM_API void gum_stalker_prefetch (GumStalker * self, gconstpointer address,
    gint recycle_count);
GUM_API void gum_stalker_prefetch_backpatch (GumStalker * self,
    const GumBackpatch * notification);
GUM_API void gum_stalker_recompile (GumStalker * self, gconstpointer address);

GUM_API gpointer gum_stalker_backpatch_get_from (
    const GumBackpatch * backpatch);
GUM_API gpointer gum_stalker_backpatch_get_to (
    const GumBackpatch * backpatch);

GUM_API void gum_stalker_invalidate (GumStalker * self, gconstpointer address);
GUM_API void gum_stalker_invalidate_for_thread (GumStalker * self,
    GumThreadId thread_id, gconstpointer address);

GUM_API GumProbeId gum_stalker_add_call_probe (GumStalker * self,
    gpointer target_address, GumCallProbeCallback callback, gpointer data,
    GDestroyNotify notify);
GUM_API void gum_stalker_remove_call_probe (GumStalker * self,
    GumProbeId id);

GUM_API gboolean gum_stalker_run_on_thread (GumStalker * self,
    GumThreadId thread_id, GumStalkerRunOnThreadFunc func, gpointer data,
    GDestroyNotify data_destroy);
GUM_API gboolean gum_stalker_run_on_thread_sync (GumStalker * self,
    GumThreadId thread_id, GumStalkerRunOnThreadFunc func, gpointer data);

GUM_API GumStalkerTransformer * gum_stalker_transformer_make_default (void);
GUM_API GumStalkerTransformer * gum_stalker_transformer_make_from_callback (
    GumStalkerTransformerCallback callback, gpointer data,
    GDestroyNotify data_destroy);

GUM_API void gum_stalker_transformer_transform_block (
    GumStalkerTransformer * self, GumStalkerIterator * iterator,
    GumStalkerOutput * output);

GUM_API gboolean gum_stalker_iterator_next (GumStalkerIterator * self,
    const cs_insn ** insn);
GUM_API void gum_stalker_iterator_keep (GumStalkerIterator * self);
GUM_API GumMemoryAccess gum_stalker_iterator_get_memory_access (
    GumStalkerIterator * self);
GUM_API void gum_stalker_iterator_put_callout (GumStalkerIterator * self,
    GumStalkerCallout callout, gpointer data, GDestroyNotify data_destroy);
GUM_API void gum_stalker_iterator_put_chaining_return (
    GumStalkerIterator * self);
GUM_API csh gum_stalker_iterator_get_capstone (GumStalkerIterator * self);

#define GUM_DECLARE_OBSERVER_INCREMENT(name) \
    GUM_API void gum_stalker_observer_increment_##name ( \
        GumStalkerObserver * observer);

GUM_DECLARE_OBSERVER_INCREMENT (total)

GUM_DECLARE_OBSERVER_INCREMENT (call_imm)
GUM_DECLARE_OBSERVER_INCREMENT (call_reg)

GUM_DECLARE_OBSERVER_INCREMENT (call_mem)

GUM_DECLARE_OBSERVER_INCREMENT (excluded_call_reg)

GUM_DECLARE_OBSERVER_INCREMENT (ret_slow_path)

GUM_DECLARE_OBSERVER_INCREMENT (ret)

GUM_DECLARE_OBSERVER_INCREMENT (post_call_invoke)
GUM_DECLARE_OBSERVER_INCREMENT (excluded_call_imm)

GUM_DECLARE_OBSERVER_INCREMENT (jmp_imm)
GUM_DECLARE_OBSERVER_INCREMENT (jmp_reg)

GUM_DECLARE_OBSERVER_INCREMENT (jmp_mem)
GUM_DECLARE_OBSERVER_INCREMENT (jmp_cond_imm)
GUM_DECLARE_OBSERVER_INCREMENT (jmp_cond_mem)
GUM_DECLARE_OBSERVER_INCREMENT (jmp_cond_reg)
GUM_DECLARE_OBSERVER_INCREMENT (jmp_cond_jcxz)

GUM_DECLARE_OBSERVER_INCREMENT (jmp_cond_cc)
GUM_DECLARE_OBSERVER_INCREMENT (jmp_cond_cbz)
GUM_DECLARE_OBSERVER_INCREMENT (jmp_cond_cbnz)
GUM_DECLARE_OBSERVER_INCREMENT (jmp_cond_tbz)
GUM_DECLARE_OBSERVER_INCREMENT (jmp_cond_tbnz)

GUM_DECLARE_OBSERVER_INCREMENT (jmp_continuation)

GUM_DECLARE_OBSERVER_INCREMENT (sysenter_slow_path)

GUM_API void gum_stalker_observer_notify_backpatch (
    GumStalkerObserver * observer, const GumBackpatch * backpatch, gsize size);

GUM_API void gum_stalker_observer_switch_callback (
    GumStalkerObserver * observer, gpointer from_address,
    gpointer start_address, gpointer from_insn, gpointer * target);

G_END_DECLS

#endif
