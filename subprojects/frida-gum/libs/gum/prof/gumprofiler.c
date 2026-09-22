/*
 * Copyright (C) 2008-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2008 Christian Berentsen <jc.berentsen@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumprofiler.h"

#include "guminterceptor.h"
#include "gumsymbolutil.h"

#include <string.h>

#define GUM_PROFILER_LOCK()   (g_mutex_lock (&self->mutex))
#define GUM_PROFILER_UNLOCK() (g_mutex_unlock (&self->mutex))

typedef struct _GumProfilerInvocation GumProfilerInvocation;
typedef struct _GumProfilerContext GumProfilerContext;
typedef struct _GumFunctionContext GumFunctionContext;
typedef struct _GumWorstCaseInfo GumWorstCaseInfo;
typedef struct _GumWorstCase GumWorstCase;
typedef struct _GumFunctionThreadContext GumFunctionThreadContext;

struct _GumProfiler
{
  GObject parent;

  gboolean disposed;

  GMutex mutex;

  GumInterceptor * interceptor;
  GHashTable * function_by_address;
  GSList * stacks;
};

struct _GumProfilerInvocation
{
  GumProfilerContext * profiler;
  GumFunctionContext * function;
  GumFunctionThreadContext * thread;

  GumSample start_time;
};

struct _GumProfilerContext
{
  GArray * stack;
};

struct _GumWorstCaseInfo
{
  gchar buf[GUM_MAX_WORST_CASE_INFO_SIZE];
};

struct _GumWorstCase
{
  GumSample duration;
  GumWorstCaseInfo info;
};

struct _GumFunctionThreadContext
{
  GumFunctionContext * function_ctx;
  guint thread_id;

  /* statistics */
  guint64 total_calls;
  GumSample total_duration;
  GumWorstCase worst_case;

  /* state */
  gboolean is_root_node;
  gint recurse_count;
  GumWorstCaseInfo potential_info;

  GumFunctionThreadContext * child_ctx;
};

struct _GumFunctionContext
{
  gpointer function_address;

  GumSamplerInterface * sampler_interface;
  GumSampler * sampler_instance;
  GumWorstCaseInspectorFunc inspector_func;
  gpointer inspector_data;
  GDestroyNotify inspector_data_destroy;

  GumFunctionThreadContext thread_contexts[GUM_MAX_THREADS];
  volatile gint thread_context_count;
};

static void gum_profiler_invocation_listener_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_profiler_dispose (GObject * object);
static void gum_profiler_finalize (GObject * object);

static void gum_profiler_on_enter (GumInvocationListener * listener,
    GumInvocationContext * context);
static void gum_profiler_on_leave (GumInvocationListener * listener,
    GumInvocationContext * context);

static void unstrument_and_free_function (gpointer key, gpointer value,
    gpointer user_data);

static void add_to_report_if_root_node (gpointer key, gpointer value,
    gpointer user_data);
static GumProfileReportNode * make_node_from_thread_context (
    GumFunctionThreadContext * thread_ctx, GHashTable ** processed_nodes);
static GumProfileReportNode * make_node (gchar * name, guint64 total_calls,
    GumSample total_duration, GumSample worst_case_duration,
    gchar * worst_case_info, GumProfileReportNode * child);
static void thread_context_register_child_timing (
    GumFunctionThreadContext * parent_ctx,
    GumFunctionThreadContext * child_ctx);

static void get_number_of_threads_foreach (gpointer key, gpointer value,
    gpointer user_data);

static GumFunctionThreadContext * gum_function_context_get_current_thread (
    GumFunctionContext * function_ctx, GumInvocationContext * context);

G_DEFINE_TYPE_EXTENDED (GumProfiler,
                        gum_profiler,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_INVOCATION_LISTENER,
                            gum_profiler_invocation_listener_iface_init))

static void
gum_profiler_class_init (GumProfilerClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_profiler_dispose;
  object_class->finalize = gum_profiler_finalize;
}

static void
gum_profiler_invocation_listener_iface_init (gpointer g_iface,
                                             gpointer iface_data)
{
  GumInvocationListenerInterface * iface = g_iface;

  iface->on_enter = gum_profiler_on_enter;
  iface->on_leave = gum_profiler_on_leave;
}

static void
gum_profiler_init (GumProfiler * self)
{
  g_mutex_init (&self->mutex);

  self->interceptor = gum_interceptor_obtain ();
  self->function_by_address = g_hash_table_new_full (g_direct_hash,
      g_direct_equal, NULL, NULL);
}

static void
gum_profiler_dispose (GObject * object)
{
  GumProfiler * self = GUM_PROFILER (object);

  if (!self->disposed)
  {
    self->disposed = TRUE;

    gum_interceptor_detach (self->interceptor, GUM_INVOCATION_LISTENER (self));

    g_hash_table_foreach (self->function_by_address,
        unstrument_and_free_function, self);
    g_hash_table_remove_all (self->function_by_address);

    g_object_unref (self->interceptor);
    self->interceptor = NULL;
  }

  G_OBJECT_CLASS (gum_profiler_parent_class)->dispose (object);
}

static void
gum_profiler_finalize (GObject * object)
{
  GumProfiler * self = GUM_PROFILER (object);

  while (self->stacks != NULL)
  {
    GArray * stack = (GArray *) self->stacks->data;
    g_array_free (stack, TRUE);
    self->stacks = g_slist_delete_link (self->stacks, self->stacks);
  }

  g_hash_table_unref (self->function_by_address);

  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (gum_profiler_parent_class)->finalize (object);
}

static void
gum_profiler_on_enter (GumInvocationListener * listener,
                       GumInvocationContext * context)
{
  GumProfilerInvocation * inv;
  GumFunctionContext * fctx;
  GumFunctionThreadContext * tctx;

  inv = GUM_IC_GET_INVOCATION_DATA (context, GumProfilerInvocation);

  inv->profiler = GUM_IC_GET_THREAD_DATA (context, GumProfilerContext);
  if (inv->profiler->stack == NULL)
  {
    GumProfiler * self = GUM_PROFILER (listener);

    inv->profiler->stack = g_array_sized_new (FALSE, FALSE,
        sizeof (GumFunctionThreadContext *), GUM_MAX_CALL_DEPTH);

    GUM_PROFILER_LOCK ();
    self->stacks = g_slist_prepend (self->stacks, inv->profiler->stack);
    GUM_PROFILER_UNLOCK ();
  }

  inv->function = GUM_IC_GET_FUNC_DATA (context, GumFunctionContext *);
  inv->thread = gum_function_context_get_current_thread (inv->function,
      context);

  fctx = inv->function;
  tctx = inv->thread;

  g_array_append_val (inv->profiler->stack, tctx);

  tctx->total_calls++;

  if (tctx->recurse_count == 0)
  {
    GumWorstCaseInspectorFunc inspector_func;

    if ((inspector_func = fctx->inspector_func) != NULL)
    {
      inspector_func (context, tctx->potential_info.buf,
          sizeof (tctx->potential_info.buf), fctx->inspector_data);
    }

    inv->start_time = fctx->sampler_interface->sample (fctx->sampler_instance);
  }

  tctx->recurse_count++;
}

static void
gum_profiler_on_leave (GumInvocationListener * listener,
                       GumInvocationContext * context)
{
  GumProfilerInvocation * inv;
  GumFunctionContext * fctx;
  GumFunctionThreadContext * tctx;
  GArray * stack;

  inv = GUM_IC_GET_INVOCATION_DATA (context, GumProfilerInvocation);

  fctx = inv->function;
  tctx = inv->thread;
  stack = inv->profiler->stack;

  if (tctx->recurse_count == 1)
  {
    GumSample now, duration;
    GumFunctionThreadContext * parent;
    guint i;

    now = fctx->sampler_interface->sample (fctx->sampler_instance);
    duration = now - inv->start_time;

    tctx->total_duration += duration;

    if (duration > tctx->worst_case.duration)
    {
      tctx->worst_case.duration = duration;
      memcpy (&tctx->worst_case.info, &tctx->potential_info,
          sizeof (tctx->potential_info));
    }

    parent = NULL;
    for (i = 0; i != stack->len; i++)
    {
      GumFunctionThreadContext * cur;

      cur = g_array_index (stack, GumFunctionThreadContext *, i);
      if (cur != tctx)
        parent = cur;
      else
        break;
    }

    if (parent == NULL)
      tctx->is_root_node = TRUE;
    else
      thread_context_register_child_timing (parent, tctx);
  }

  tctx->recurse_count--;

  g_array_set_size (stack, stack->len - 1);
}

GumProfiler *
gum_profiler_new (void)
{
  return g_object_new (GUM_TYPE_PROFILER, NULL);
}

void
gum_profiler_instrument_functions_matching (GumProfiler * self,
                                            const gchar * match_str,
                                            GumSampler * sampler,
                                            GumFunctionMatchFilterFunc filter,
                                            gpointer user_data)
{
  GumInterceptor * interceptor = self->interceptor;
  GArray * matches;
  guint i;

  matches = gum_find_functions_matching (match_str);

  gum_interceptor_begin_transaction (interceptor);

  for (i = 0; i < matches->len; i++)
  {
    gpointer address = g_array_index (matches, gpointer, i);
    gboolean approved = TRUE;

    if (filter != NULL)
    {
      gchar * func_name;

      func_name = gum_symbol_name_from_address (address);
      approved = filter (func_name, user_data);
      g_free (func_name);
    }

    if (approved)
      gum_profiler_instrument_function (self, address, sampler);
  }

  gum_interceptor_end_transaction (interceptor);

  g_array_free (matches, TRUE);
}

GumInstrumentReturn
gum_profiler_instrument_function (GumProfiler * self,
                                  gpointer function_address,
                                  GumSampler * sampler)
{
  return gum_profiler_instrument_function_with_inspector (self,
      function_address, sampler, NULL, NULL, NULL);
}

GumInstrumentReturn
gum_profiler_instrument_function_with_inspector (
    GumProfiler * self,
    gpointer function_address,
    GumSampler * sampler,
    GumWorstCaseInspectorFunc inspector_func,
    gpointer data,
    GDestroyNotify data_destroy)
{
  GumInstrumentReturn result = GUM_INSTRUMENT_OK;
  GumFunctionContext * ctx;
  GumAttachOptions options = { 0, };
  GumAttachReturn attach_ret;

  ctx = g_new0 (GumFunctionContext, 1);

  options.listener_function_data = ctx;
  attach_ret = gum_interceptor_attach (self->interceptor, function_address,
      GUM_INVOCATION_LISTENER (self), &options);
  if (attach_ret != GUM_ATTACH_OK)
    goto error;

  ctx->function_address = function_address;
  ctx->sampler_interface = GUM_SAMPLER_GET_IFACE (sampler);
  ctx->sampler_instance = g_object_ref (sampler);
  ctx->inspector_func = inspector_func;
  ctx->inspector_data = data;
  ctx->inspector_data_destroy = data_destroy;

  GUM_PROFILER_LOCK ();
  g_hash_table_insert (self->function_by_address, function_address, ctx);
  GUM_PROFILER_UNLOCK ();

  return result;

error:
  g_free (ctx);

  if (attach_ret == GUM_ATTACH_WRONG_SIGNATURE)
    result = GUM_INSTRUMENT_WRONG_SIGNATURE;
  else if (attach_ret == GUM_ATTACH_ALREADY_ATTACHED)
    result = GUM_INSTRUMENT_WAS_INSTRUMENTED;
  else
    g_assert_not_reached ();

  return result;
}

static void
unstrument_and_free_function (gpointer key,
                              gpointer value,
                              gpointer user_data)
{
  GumFunctionContext * function_ctx = (GumFunctionContext *) value;

  if (function_ctx->inspector_data_destroy != NULL)
    function_ctx->inspector_data_destroy (function_ctx->inspector_data);

  g_object_unref (function_ctx->sampler_instance);

  g_free (function_ctx);
}

GumProfileReport *
gum_profiler_generate_report (GumProfiler * self)
{
  GumProfileReport * report;

  report = gum_profile_report_new ();
  GUM_PROFILER_LOCK ();
  g_hash_table_foreach (self->function_by_address, add_to_report_if_root_node,
      report);
  GUM_PROFILER_UNLOCK ();
  _gum_profile_report_sort (report);

  return report;
}

static void
add_to_report_if_root_node (gpointer key,
                            gpointer value,
                            gpointer user_data)
{
  GumProfileReport * report = GUM_PROFILE_REPORT (user_data);
  GumFunctionContext * function_ctx = (GumFunctionContext *) value;

  if (function_ctx->thread_context_count > 0)
  {
    gint i;

    for (i = 0; i != function_ctx->thread_context_count; i++)
    {
      GumFunctionThreadContext * thread_ctx =
          &function_ctx->thread_contexts[i];

      if (thread_ctx->is_root_node)
      {
        GHashTable * processed_nodes = NULL;
        GumProfileReportNode * root_node;

        root_node = make_node_from_thread_context (thread_ctx,
            &processed_nodes);
        _gum_profile_report_append_thread_root_node (report,
            thread_ctx->thread_id, root_node);
      }
    }
  }
}

static GumProfileReportNode *
make_node_from_thread_context (GumFunctionThreadContext * thread_ctx,
                               GHashTable ** processed_nodes)
{
  gpointer parent_function_address;
  gchar * parent_node_name;
  GumProfileReportNode * parent_node;
  GumFunctionThreadContext * child_ctx;
  GumProfileReportNode * child_node = NULL;

  if (*processed_nodes != NULL)
    g_hash_table_ref (*processed_nodes);

  parent_function_address = thread_ctx->function_ctx->function_address;
  parent_node_name = gum_symbol_name_from_address (parent_function_address);

  child_ctx = thread_ctx->child_ctx;
  if (child_ctx != NULL)
  {
    if (*processed_nodes == NULL)
      *processed_nodes = g_hash_table_new (g_direct_hash, g_direct_equal);

    if (g_hash_table_lookup (*processed_nodes, child_ctx) == NULL)
    {
      g_hash_table_insert (*processed_nodes, thread_ctx, GSIZE_TO_POINTER (1));

      child_node = make_node_from_thread_context (child_ctx, processed_nodes);
    }
  }

  parent_node = make_node (parent_node_name, thread_ctx->total_calls,
      thread_ctx->total_duration, thread_ctx->worst_case.duration,
      g_strdup (thread_ctx->worst_case.info.buf),
      child_node);

  if (*processed_nodes != NULL)
    g_hash_table_unref (*processed_nodes);

  return parent_node;
}

static GumProfileReportNode *
make_node (gchar * name,
           guint64 total_calls,
           GumSample total_duration,
           GumSample worst_case_duration,
           gchar * worst_case_info,
           GumProfileReportNode * child)
{
  GumProfileReportNode * node;

  node = g_new (GumProfileReportNode, 1);
  node->name = name;
  node->total_calls = total_calls;
  node->total_duration = total_duration;
  node->worst_case_duration = worst_case_duration;
  node->worst_case_info = worst_case_info;
  node->child = child;

  return node;
}

guint
gum_profiler_get_number_of_threads (GumProfiler * self)
{
  guint result;
  GHashTable * unique_thread_id_set;

  unique_thread_id_set = g_hash_table_new (g_direct_hash, g_direct_equal);
  GUM_PROFILER_LOCK ();
  g_hash_table_foreach (self->function_by_address,
      get_number_of_threads_foreach, unique_thread_id_set);
  GUM_PROFILER_UNLOCK ();
  result = g_hash_table_size (unique_thread_id_set);
  g_hash_table_unref (unique_thread_id_set);

  return result;
}

GumSample
gum_profiler_get_total_duration_of (GumProfiler * self,
                                    guint thread_index,
                                    gpointer function_address)
{
  GumFunctionContext * function_ctx;

  GUM_PROFILER_LOCK ();
  function_ctx = (GumFunctionContext *)
      g_hash_table_lookup (self->function_by_address, function_address);
  GUM_PROFILER_UNLOCK ();

  if (function_ctx != NULL
      && (gint) thread_index < function_ctx->thread_context_count)
    return function_ctx->thread_contexts[thread_index].total_duration;
  else
    return 0;
}

GumSample
gum_profiler_get_worst_case_duration_of (GumProfiler * self,
                                         guint thread_index,
                                         gpointer function_address)
{
  GumFunctionContext * function_ctx;

  GUM_PROFILER_LOCK ();
  function_ctx = (GumFunctionContext *)
      g_hash_table_lookup (self->function_by_address, function_address);
  GUM_PROFILER_UNLOCK ();

  if (function_ctx != NULL
      && (gint) thread_index < function_ctx->thread_context_count)
    return function_ctx->thread_contexts[thread_index].worst_case.duration;
  else
    return 0;
}

const gchar *
gum_profiler_get_worst_case_info_of (GumProfiler * self,
                                     guint thread_index,
                                     gpointer function_address)
{
  GumFunctionContext * function_ctx;

  GUM_PROFILER_LOCK ();
  function_ctx = (GumFunctionContext *)
      g_hash_table_lookup (self->function_by_address, function_address);
  GUM_PROFILER_UNLOCK ();

  if (function_ctx != NULL
      && (gint) thread_index < function_ctx->thread_context_count)
    return function_ctx->thread_contexts[thread_index].worst_case.info.buf;
  else
    return "";
}

static void
thread_context_register_child_timing (GumFunctionThreadContext * parent_ctx,
                                      GumFunctionThreadContext * child_ctx)
{
  GumFunctionThreadContext * cur_child = parent_ctx->child_ctx;

  if (cur_child != NULL)
  {
    if (child_ctx->total_duration > cur_child->total_duration)
      parent_ctx->child_ctx = child_ctx;
  }
  else
  {
    parent_ctx->child_ctx = child_ctx;
  }
}

static void
get_number_of_threads_foreach (gpointer key,
                               gpointer value,
                               gpointer user_data)
{
  GumFunctionContext * function_ctx = value;
  GHashTable * unique_thread_id_set = user_data;
  guint thread_count = function_ctx->thread_context_count;
  guint i;

  for (i = 0; i < thread_count; i++)
  {
    g_hash_table_insert (unique_thread_id_set,
        GUINT_TO_POINTER (function_ctx->thread_contexts[i].thread_id), NULL);
  }
}

GumFunctionThreadContext *
gum_function_context_get_current_thread (GumFunctionContext * function_ctx,
                                         GumInvocationContext * context)
{
  guint32 current_thread_id;
  guint thread_count;
  guint i;
  GumFunctionThreadContext * thread_ctx;

  current_thread_id = gum_invocation_context_get_thread_id (context);

  thread_count = g_atomic_int_get (&function_ctx->thread_context_count);
  for (i = 0; i != thread_count; i++)
  {
    thread_ctx = &function_ctx->thread_contexts[i];

    if (thread_ctx->thread_id == current_thread_id)
      return thread_ctx;
  }

  i = g_atomic_int_add (&function_ctx->thread_context_count, 1);
  g_assert (i < G_N_ELEMENTS (function_ctx->thread_contexts));
  thread_ctx = &function_ctx->thread_contexts[i];
  thread_ctx->function_ctx = function_ctx;
  thread_ctx->thread_id = current_thread_id;

  return thread_ctx;
}
