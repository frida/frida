/*
 * Copyright (C) 2008-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2008 Christian Berentsen <jc.berentsen@gmail.com>
 * Copyright (C) 2024-2025 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2024 Yannis Juglaret <yjuglaret@mozilla.com>
 * Copyright (C) 2026 Sam Sun <samsun@nvidia.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "guminterceptor.h"

#include "gumcodesegment.h"
#include "guminterceptor-priv.h"
#include "gumunwindbroker.h"
#include "gumlibc.h"
#include "gummemory.h"
#include "gummetalarray.h"
#include "gumprocess-priv.h"
#include "gumtls.h"

#include <string.h>
#ifdef HAVE_DARWIN
# include <mach/mach.h>
#endif

#if defined (HAVE_I386)
# define GUM_INTERCEPTOR_CODE_SLICE_SIZE 512
#elif defined (HAVE_MIPS)
# define GUM_INTERCEPTOR_CODE_SLICE_SIZE 1024
#else
# define GUM_INTERCEPTOR_CODE_SLICE_SIZE 256
#endif

#define GUM_INTERCEPTOR_LOCK(o) g_rec_mutex_lock (&(o)->mutex)
#define GUM_INTERCEPTOR_UNLOCK(o) g_rec_mutex_unlock (&(o)->mutex)

#if defined (HAVE_I386)
# define GUM_INTERCEPTOR_CPU_CONTEXT_SP(c) \
    ((gpointer) GUM_CPU_CONTEXT_XSP (c))
#else
# define GUM_INTERCEPTOR_CPU_CONTEXT_SP(c) ((gpointer) (c)->sp)
#endif

typedef struct _GumInterceptorTransaction GumInterceptorTransaction;
typedef guint GumInstrumentationError;
typedef struct _GumDestroyTask GumDestroyTask;
typedef struct _GumUpdateTask GumUpdateTask;
typedef struct _ListenerEntry ListenerEntry;
typedef struct _InterceptorThreadContext InterceptorThreadContext;
typedef struct _GumInvocationStackEntry GumInvocationStackEntry;
typedef struct _ListenerDataSlot ListenerDataSlot;
typedef struct _ListenerInvocationState ListenerInvocationState;

typedef void (* GumUpdateTaskFunc) (GumInterceptor * self,
    GumFunctionContext * ctx, gpointer prologue);

struct _GumInterceptorTransaction
{
  gboolean is_dirty;
  gint level;
  GQueue * pending_destroy_tasks;
  GHashTable * pending_update_tasks;

  GumInterceptor * interceptor;
};

struct _GumInterceptor
{
  GObject parent;

  GRecMutex mutex;

  GHashTable * function_by_address;

  GumInterceptorBackend * backend;
  GumCodeAllocator allocator;

  GumInterceptorOptions options;

  volatile guint selected_thread_id;

  GumInterceptorTransaction current_transaction;

  GumUnwindBroker * unwind_broker;
};

enum _GumInstrumentationError
{
  GUM_INSTRUMENTATION_ERROR_NONE,
  GUM_INSTRUMENTATION_ERROR_WRONG_SIGNATURE,
  GUM_INSTRUMENTATION_ERROR_POLICY_VIOLATION,
  GUM_INSTRUMENTATION_ERROR_WRONG_TYPE,
};

struct _GumDestroyTask
{
  GumFunctionContext * ctx;
  GDestroyNotify notify;
  gpointer data;
};

struct _GumUpdateTask
{
  GumFunctionContext * ctx;
  GumUpdateTaskFunc func;
};

struct _ListenerEntry
{
  GumInvocationListenerInterface * listener_interface;
  GumInvocationListener * listener_instance;
  gpointer function_data;
  gboolean unignorable;
};

struct _InterceptorThreadContext
{
  GumInvocationBackend listener_backend;
  GumInvocationBackend replacement_backend;

  gint ignore_level;

  GumInvocationStack * stack;

  GArray * listener_data_slots;
};

struct _GumInvocationStackEntry
{
  GumFunctionContext * function_ctx;
  gpointer caller_ret_addr;
  gpointer stack_address;
  GumInvocationContext invocation_context;
  GumCpuContext cpu_context;
#ifdef GUM_CPU_CONTEXT_HAS_OUT_OF_LINE_VECTORS
  GumX86VectorReg cpu_context_vectors[GUM_X86_XMM_REG_COUNT];
#endif
  guint8 listener_invocation_data[GUM_MAX_LISTENERS_PER_FUNCTION]
      [GUM_MAX_LISTENER_DATA];
  gboolean calling_replacement;
  gboolean only_invoke_unignorable_listeners;
  gint original_system_error;
};

struct _ListenerDataSlot
{
  GumInvocationListener * owner;
  guint8 data[GUM_MAX_LISTENER_DATA];
};

struct _ListenerInvocationState
{
  GumPointCut point_cut;
  ListenerEntry * entry;
  InterceptorThreadContext * interceptor_ctx;
  guint8 * invocation_data;
};

static void gum_interceptor_dispose (GObject * object);
static void gum_interceptor_finalize (GObject * object);

static void the_interceptor_weak_notify (gpointer data,
    GObject * where_the_object_was);
static GumReplaceReturn gum_interceptor_replace_with_type (
    GumInterceptor * self, GumInterceptorType type, gpointer function_address,
    gpointer replacement_function, gpointer replacement_data,
    gpointer * original_function, const GumInterceptorOptions * options);
static GumFunctionContext * gum_interceptor_instrument (GumInterceptor * self,
    GumInterceptorType type, gpointer function_address,
    const GumInterceptorOptions * instrumentation,
    GumInstrumentationError * error);
static void gum_interceptor_activate (GumInterceptor * self,
    GumFunctionContext * ctx, gpointer prologue);
static void gum_interceptor_deactivate (GumInterceptor * self,
    GumFunctionContext * ctx, gpointer prologue);

static void gum_interceptor_transaction_init (
    GumInterceptorTransaction * transaction, GumInterceptor * interceptor);
static void gum_interceptor_transaction_destroy (
    GumInterceptorTransaction * transaction);
static void gum_interceptor_transaction_begin (
    GumInterceptorTransaction * self);
static void gum_interceptor_transaction_end (GumInterceptorTransaction * self);
static void gum_apply_updates (gpointer source_page, gpointer target_page,
    guint n_pages, gpointer user_data);
static void gum_interceptor_transaction_schedule_destroy (
    GumInterceptorTransaction * self, GumFunctionContext * ctx,
    GDestroyNotify notify, gpointer data);
static void gum_interceptor_transaction_schedule_update (
    GumInterceptorTransaction * self, GumFunctionContext * ctx,
    GumUpdateTaskFunc func);

static GumFunctionContext * gum_function_context_new (
    GumInterceptor * interceptor, gpointer function_address,
    GumInterceptorType type);
static void gum_function_context_finalize (GumFunctionContext * function_ctx);
static void gum_function_context_destroy (GumFunctionContext * function_ctx);
static void gum_function_context_discard (GumFunctionContext * function_ctx);
static void gum_function_context_perform_destroy (
    GumFunctionContext * function_ctx);
static gboolean gum_function_context_is_empty (
    GumFunctionContext * function_ctx);
static void gum_function_context_add_listener (
    GumFunctionContext * function_ctx, GumInvocationListener * listener,
    gpointer function_data, gboolean unignorable);
static void gum_function_context_remove_listener (
    GumFunctionContext * function_ctx, GumInvocationListener * listener);
static void listener_entry_free (ListenerEntry * entry);
static gboolean gum_function_context_has_listener (
    GumFunctionContext * function_ctx, GumInvocationListener * listener);
static ListenerEntry ** gum_function_context_find_listener (
    GumFunctionContext * function_ctx, GumInvocationListener * listener);
static ListenerEntry ** gum_function_context_find_taken_listener_slot (
    GumFunctionContext * function_ctx);
static void gum_function_context_fixup_cpu_context (
    GumFunctionContext * function_ctx, GumCpuContext * cpu_context);

static InterceptorThreadContext * get_interceptor_thread_context (void);
static void release_interceptor_thread_context (
    InterceptorThreadContext * context);
static InterceptorThreadContext * interceptor_thread_context_new (void);
static void interceptor_thread_context_destroy (
    InterceptorThreadContext * context);
static gpointer interceptor_thread_context_get_listener_data (
    InterceptorThreadContext * self, GumInvocationListener * listener,
    gsize required_size);
static void interceptor_thread_context_forget_listener_data (
    InterceptorThreadContext * self, GumInvocationListener * listener);
static GumInvocationStackEntry * gum_invocation_stack_push (
    GumInvocationStack * stack, GumFunctionContext * function_ctx,
    gpointer caller_ret_addr, gpointer stack_address,
    gboolean only_invoke_unignorable_listeners);
static gpointer gum_invocation_stack_pop (GumInvocationStack * stack);
static void gum_invocation_stack_reap_unwound (GumInvocationStack * stack,
    gpointer live_stack_address);
static void gum_invocation_stack_reap_unwound_above (
    GumInvocationStack * stack, GumFunctionContext * returning_ctx);
static void gum_invocation_stack_entry_snapshot_cpu_context (
    GumInvocationStackEntry * entry, const GumCpuContext * cpu_context);
static gboolean gum_invocation_stack_entry_was_unwound_past (
    const GumInvocationStackEntry * entry, gpointer live_stack_address);
static void gum_invocation_stack_entry_release_trampoline (
    const GumInvocationStackEntry * entry);
static GumInvocationStackEntry * gum_invocation_stack_peek_top (
    GumInvocationStack * stack);

static gpointer gum_interceptor_resolve (GumInterceptor * self,
    gpointer address);
static gboolean gum_interceptor_has (GumInterceptor * self,
    gpointer function_address);
static void gum_interceptor_discard_function_contexts_in_range (
    GumInterceptor * self, const GumMemoryRange * range);

static gpointer gum_page_address_from_pointer (gpointer ptr);
static gint gum_page_address_compare (gconstpointer * a, gconstpointer * b);

/**
 * GumInterceptor:
 *
 * Intercepts execution through inline hooking.
 *
 * Three complementary mechanisms are offered:
 *
 * - *Attaching* a [iface@Gum.InvocationListener] to a function, to be notified
 *   right before it is entered and right after it returns, while leaving the
 *   original function in place. This is the classic enter/leave hook.
 * - *Probing* a single point in the code with a listener that implements only
 *   `on_enter`, typically one from [func@Gum.make_probe_listener]. Because the
 *   target is an arbitrary address rather than a function entry, a probe can be
 *   placed in the middle of a function to observe execution reaching a specific
 *   instruction. A call listener given a %NULL `on_leave` is not equivalent: it
 *   still traps the return and counts toward
 *   [method@Gum.InvocationContext.get_depth].
 * - *Replacing* a function outright with your own implementation. Your
 *   replacement can still reach the original by calling the function's own
 *   address: the interceptor routes such a call to the original instead of
 *   recursing back into the replacement. (The lighter
 *   [method@Gum.Interceptor.replace_fast] instead hands you a dedicated
 *   pointer for this.)
 *
 * A batch of changes can be grouped into a transaction so that they are
 * activated as a unit, which is both faster and atomic from the target's point
 * of view; see [method@Gum.Interceptor.begin_transaction].
 *
 * ## Attaching a listener
 *
 * ```c
 * static void on_enter (GumInvocationContext * ic, gpointer user_data);
 * static void on_leave (GumInvocationContext * ic, gpointer user_data);
 *
 * void
 * instrument (void)
 * {
 *   g_autoptr(GumInterceptor) interceptor = gum_interceptor_obtain ();
 *   GumInvocationListener * listener =
 *       gum_make_call_listener (on_enter, on_leave, NULL, NULL);
 *
 *   gum_interceptor_begin_transaction (interceptor);
 *   gum_interceptor_attach (interceptor,
 *       GSIZE_TO_POINTER (gum_module_find_global_export_by_name ("open")),
 *       listener, NULL);
 *   gum_interceptor_end_transaction (interceptor);
 * }
 * ```
 *
 * ## Replacing a function
 *
 * ```c
 * static int (* libc_open) (const char * path, int oflag, ...);
 *
 * static int
 * replacement_open (const char * path, int oflag, ...)
 * {
 *   g_printerr ("open(\"%s\")\n", path);
 *   return libc_open (path, oflag); // reaches the original
 * }
 *
 * void
 * instrument (void)
 * {
 *   g_autoptr(GumInterceptor) interceptor = gum_interceptor_obtain ();
 *
 *   libc_open = GSIZE_TO_POINTER (
 *       gum_module_find_global_export_by_name ("open"));
 *
 *   gum_interceptor_replace (interceptor, libc_open, replacement_open,
 *       NULL, NULL);
 * }
 * ```
 *
 * ## Ahead-of-time instrumentation
 *
 * Inline hooking normally rewrites code at runtime, but some platforms forbid
 * that: where code signing is strictly enforced (e.g. iOS), executable pages
 * cannot be patched on the fly. For these, trampolines can be *grafted* into a
 * Mach-O binary ahead of time with [class@Gum.DarwinGrafter] — exposed as the
 * `gum-graft` command-line tool — which reserves a trampoline at each code
 * offset you intend to hook.
 *
 * At runtime [method@Gum.Interceptor.attach] and
 * [method@Gum.Interceptor.replace] then claim the matching grafted trampoline
 * instead of patching code, so interception works without writable code. If a
 * target has no grafted trampoline while code signing requires one, the attach
 * or replace fails with `GUM_ATTACH_POLICY_VIOLATION` /
 * `GUM_REPLACE_POLICY_VIOLATION`.
 */

/**
 * GumInterceptorScenario:
 * @GUM_INTERCEPTOR_SCENARIO_DEFAULT: use the interceptor's configured default
 * @GUM_INTERCEPTOR_SCENARIO_ONLINE: other threads may be executing the target
 *   code, so it must be instrumented conservatively
 * @GUM_INTERCEPTOR_SCENARIO_OFFLINE: the target is quiescent, allowing more
 *   aggressive rewriting
 *
 * Whether other threads may be running the code being instrumented. When the
 * target is known to be quiescent — for example a process freshly created with
 * `spawn()` whose main thread is still suspended before `main()` —
 * %GUM_INTERCEPTOR_SCENARIO_OFFLINE lets it rewrite more freely, e.g.
 * overwriting past a `CALL` since no other thread can already be inside the
 * call waiting to return. %GUM_INTERCEPTOR_SCENARIO_ONLINE is the safe choice
 * for live processes where such concurrency is possible.
 */

G_DEFINE_TYPE (GumInterceptor, gum_interceptor, G_TYPE_OBJECT)

static GMutex _gum_interceptor_lock;
static GumInterceptor * _the_interceptor = NULL;

static GumSpinlock gum_interceptor_thread_context_lock = GUM_SPINLOCK_INIT;
static GHashTable * gum_interceptor_thread_contexts;
static GPrivate gum_interceptor_context_private =
    G_PRIVATE_INIT ((GDestroyNotify) release_interceptor_thread_context);
static GumTlsKey gum_interceptor_guard_key;

static GumInvocationStack _gum_interceptor_empty_stack = { NULL, 0 };

static void
gum_interceptor_class_init (GumInterceptorClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_interceptor_dispose;
  object_class->finalize = gum_interceptor_finalize;
}

void
_gum_interceptor_init (void)
{
  gum_interceptor_thread_contexts = g_hash_table_new_full (NULL, NULL,
      (GDestroyNotify) interceptor_thread_context_destroy, NULL);

  gum_interceptor_guard_key = gum_tls_key_new ();
}

void
_gum_interceptor_deinit (void)
{
  gum_tls_key_free (gum_interceptor_guard_key);

  g_hash_table_unref (gum_interceptor_thread_contexts);
  gum_interceptor_thread_contexts = NULL;
}

static void
gum_interceptor_init (GumInterceptor * self)
{
  g_rec_mutex_init (&self->mutex);

  self->function_by_address = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_function_context_destroy);

  gum_code_allocator_init (&self->allocator, GUM_INTERCEPTOR_CODE_SLICE_SIZE);

  self->options.scenario = GUM_INTERCEPTOR_SCENARIO_ONLINE;
  self->options.relocation_policy = GUM_RELOCATION_CHECKED;

  gum_interceptor_transaction_init (&self->current_transaction, self);
}

static void
gum_interceptor_dispose (GObject * object)
{
  GumInterceptor * self = GUM_INTERCEPTOR (object);

  GUM_INTERCEPTOR_LOCK (self);
  gum_interceptor_transaction_begin (&self->current_transaction);
  self->current_transaction.is_dirty = TRUE;

  g_hash_table_remove_all (self->function_by_address);

  gum_interceptor_transaction_end (&self->current_transaction);
  GUM_INTERCEPTOR_UNLOCK (self);

  g_clear_object (&self->unwind_broker);

  G_OBJECT_CLASS (gum_interceptor_parent_class)->dispose (object);
}

static void
gum_interceptor_finalize (GObject * object)
{
  GumInterceptor * self = GUM_INTERCEPTOR (object);

  gum_interceptor_transaction_destroy (&self->current_transaction);

  if (self->backend != NULL)
    _gum_interceptor_backend_destroy (self->backend);

  g_rec_mutex_clear (&self->mutex);

  g_hash_table_unref (self->function_by_address);

  gum_code_allocator_free (&self->allocator);

  G_OBJECT_CLASS (gum_interceptor_parent_class)->finalize (object);
}

/**
 * gum_interceptor_obtain:
 *
 * Obtains the interceptor singleton.
 *
 * Returns: (transfer full): the interceptor
 */
GumInterceptor *
gum_interceptor_obtain (void)
{
  GumInterceptor * interceptor;
  gboolean newly_created = FALSE;

  g_mutex_lock (&_gum_interceptor_lock);

  if (_the_interceptor != NULL)
  {
    interceptor = GUM_INTERCEPTOR (g_object_ref (_the_interceptor));
  }
  else
  {
    _the_interceptor = g_object_new (GUM_TYPE_INTERCEPTOR, NULL);
    g_object_weak_ref (G_OBJECT (_the_interceptor),
        the_interceptor_weak_notify, NULL);

    interceptor = _the_interceptor;
    newly_created = TRUE;
  }

  g_mutex_unlock (&_gum_interceptor_lock);

  /*
   * Activate the unwind broker so C++/Objective-C exceptions can propagate
   * through our trampolines. Done outside the lock because the broker's
   * backend re-enters gum_interceptor_obtain () to install its own hooks.
   * A freestanding target has no such exceptions, and nothing to unwind with.
   */
#ifndef G_OS_NONE
  if (newly_created)
    interceptor->unwind_broker = gum_unwind_broker_obtain ();
#endif

  return interceptor;
}

static void
the_interceptor_weak_notify (gpointer data,
                             GObject * where_the_object_was)
{
  g_mutex_lock (&_gum_interceptor_lock);

  g_assert (_the_interceptor == (GumInterceptor *) where_the_object_was);
  _the_interceptor = NULL;

  g_mutex_unlock (&_gum_interceptor_lock);
}

/**
 * gum_interceptor_set_default_options:
 * @self: the interceptor
 * @options: (not nullable): the options to use as defaults
 *
 * Sets the instrumentation options applied when a subsequent attach or replace
 * is given no options of its own.
 */
void
gum_interceptor_set_default_options (GumInterceptor * self,
                                     const GumInterceptorOptions * options)
{
  GumInterceptorOptions * defaults = &self->options;

  *defaults = *options;

  if (defaults->scenario == GUM_INTERCEPTOR_SCENARIO_DEFAULT)
    defaults->scenario = GUM_INTERCEPTOR_SCENARIO_ONLINE;
  if (defaults->relocation_policy == GUM_RELOCATION_DEFAULT)
    defaults->relocation_policy = GUM_RELOCATION_CHECKED;
}

/**
 * gum_interceptor_attach:
 * @self: the interceptor
 * @target: (not nullable): address to intercept
 * @listener: (transfer none): listener notified on enter and leave
 * @options: (nullable): attach options, or %NULL for the defaults
 *
 * Attaches @listener so that it is notified right before @target is entered and
 * right after it returns. The same listener may be attached to any number of
 * addresses, and multiple listeners may be attached to the same address. The
 * original code is left in place.
 *
 * @target need not be a function entry: a listener that implements
 * only `on_enter` acts as a probe and may be placed at an arbitrary
 * instruction to observe execution reaching that point.
 *
 * The change takes effect immediately unless a transaction is open.
 *
 * Returns: %GUM_ATTACH_OK on success, or another [enum@Gum.AttachReturn]
 *   describing why the function could not be instrumented
 */
GumAttachReturn
gum_interceptor_attach (GumInterceptor * self,
                        gpointer target,
                        GumInvocationListener * listener,
                        const GumAttachOptions * options)
{
  GumAttachReturn result = GUM_ATTACH_OK;
  GumAttachOptions default_options = { 0, };
  GumFunctionContext * function_ctx;
  GumInstrumentationError error;

  if (options == NULL)
    options = &default_options;

  gum_interceptor_ignore_current_thread (self);
  GUM_INTERCEPTOR_LOCK (self);
  gum_interceptor_transaction_begin (&self->current_transaction);
  self->current_transaction.is_dirty = TRUE;

  target = gum_interceptor_resolve (self, target);

  function_ctx = gum_interceptor_instrument (self, GUM_INTERCEPTOR_TYPE_DEFAULT,
      target, &options->instrumentation, &error);

  if (function_ctx == NULL)
    goto instrumentation_error;

  if (gum_function_context_has_listener (function_ctx, listener))
    goto already_attached;

  gum_function_context_add_listener (function_ctx, listener,
      options->listener_function_data,
      options->ignorability == GUM_INVOCATION_UNIGNORABLE);

  goto beach;

instrumentation_error:
  {
    switch (error)
    {
      case GUM_INSTRUMENTATION_ERROR_WRONG_SIGNATURE:
        result = GUM_ATTACH_WRONG_SIGNATURE;
        break;
      case GUM_INSTRUMENTATION_ERROR_POLICY_VIOLATION:
        result = GUM_ATTACH_POLICY_VIOLATION;
        break;
      case GUM_INSTRUMENTATION_ERROR_WRONG_TYPE:
        result = GUM_ATTACH_WRONG_TYPE;
        break;
      default:
        g_assert_not_reached ();
    }
    goto beach;
  }
already_attached:
  {
    result = GUM_ATTACH_ALREADY_ATTACHED;
    goto beach;
  }
beach:
  {
    gum_interceptor_transaction_end (&self->current_transaction);
    GUM_INTERCEPTOR_UNLOCK (self);
    gum_interceptor_unignore_current_thread (self);

    return result;
  }
}

/**
 * gum_interceptor_detach:
 * @self: the interceptor
 * @listener: (transfer none): the listener to detach
 *
 * Detaches @listener from every function it is currently attached to, undoing
 * any [method@Gum.Interceptor.attach] calls made with it. Functions left
 * without any listeners or replacement are restored to their original state.
 *
 * The change takes effect immediately unless a transaction is open.
 */
void
gum_interceptor_detach (GumInterceptor * self,
                        GumInvocationListener * listener)
{
  GHashTableIter iter;
  gpointer key, value;

  gum_interceptor_ignore_current_thread (self);
  GUM_INTERCEPTOR_LOCK (self);
  gum_interceptor_transaction_begin (&self->current_transaction);
  self->current_transaction.is_dirty = TRUE;

  g_hash_table_iter_init (&iter, self->function_by_address);
  while (g_hash_table_iter_next (&iter, NULL, &value))
  {
    GumFunctionContext * function_ctx = value;

    if (gum_function_context_has_listener (function_ctx, listener))
    {
      gum_function_context_remove_listener (function_ctx, listener);

      gum_interceptor_transaction_schedule_destroy (&self->current_transaction,
          function_ctx, g_object_unref, g_object_ref (listener));

      if (gum_function_context_is_empty (function_ctx))
      {
        g_hash_table_iter_remove (&iter);
      }
    }
  }

  gum_spinlock_acquire (&gum_interceptor_thread_context_lock);
  g_hash_table_iter_init (&iter, gum_interceptor_thread_contexts);
  while (g_hash_table_iter_next (&iter, &key, NULL))
  {
    InterceptorThreadContext * thread_ctx = key;

    interceptor_thread_context_forget_listener_data (thread_ctx, listener);
  }
  gum_spinlock_release (&gum_interceptor_thread_context_lock);

  gum_interceptor_transaction_end (&self->current_transaction);
  GUM_INTERCEPTOR_UNLOCK (self);
  gum_interceptor_unignore_current_thread (self);
}

/**
 * gum_interceptor_replace:
 * @self: the interceptor
 * @function_address: (not nullable): address of the function to replace
 * @replacement_function: (not nullable): address of the replacement
 * @original_function: (out) (optional) (nullable): return location for a
 *   pointer through which the original function can be called, or %NULL
 * @options: (nullable): replace options, or %NULL for the defaults
 *
 * Replaces @function_address with @replacement_function, so that any call to
 * it ends up in the replacement instead. The replacement can still reach the
 * original by calling @function_address itself — the interceptor routes that
 * call to the original rather than recursing — or through @original_function
 * if a pointer is more convenient.
 *
 * Undo with [method@Gum.Interceptor.revert]. The change takes effect
 * immediately unless a transaction is open.
 *
 * Returns: %GUM_REPLACE_OK on success, or another [enum@Gum.ReplaceReturn]
 *   describing why the function could not be instrumented
 */
GumReplaceReturn
gum_interceptor_replace (GumInterceptor * self,
                         gpointer function_address,
                         gpointer replacement_function,
                         gpointer * original_function,
                         const GumReplaceOptions * options)
{
  GumReplaceOptions default_options = { 0, };

  if (options == NULL)
    options = &default_options;

  return gum_interceptor_replace_with_type (self, GUM_INTERCEPTOR_TYPE_DEFAULT,
      function_address, replacement_function, options->replacement_data,
      original_function, &options->instrumentation);
}

/**
 * gum_interceptor_replace_fast:
 * @self: the interceptor
 * @function_address: (not nullable): address of the function to replace
 * @replacement_function: (not nullable): address of the replacement
 * @original_function: (out) (optional) (nullable): return location for a
 *   pointer through which the original function can still be called, or %NULL
 * @options: (nullable): instrumentation options, or %NULL for the defaults
 *
 * Like [method@Gum.Interceptor.replace], but trades flexibility for speed by
 * patching @function_address to branch straight to @replacement_function with
 * no trampoline in between. A trampoline is only involved if you ask for
 * @original_function, which you must use to reach the original — unlike
 * [method@Gum.Interceptor.replace], calling @function_address again would just
 * re-enter the replacement. A target replaced this way cannot also be attached
 * to; use [method@Gum.Interceptor.replace] if you need that.
 *
 * Prefer this when the hook is on a hot path and the extra machinery of the
 * default replacement is not needed.
 *
 * Returns: %GUM_REPLACE_OK on success, or another [enum@Gum.ReplaceReturn]
 *   describing why the function could not be instrumented
 */
GumReplaceReturn
gum_interceptor_replace_fast (GumInterceptor * self,
                              gpointer function_address,
                              gpointer replacement_function,
                              gpointer * original_function,
                              const GumInterceptorOptions * options)
{
  GumInterceptorOptions default_options = { 0, };

  if (options == NULL)
    options = &default_options;

  return gum_interceptor_replace_with_type (self, GUM_INTERCEPTOR_TYPE_FAST,
      function_address, replacement_function, NULL,
      original_function, options);
}

static GumReplaceReturn
gum_interceptor_replace_with_type (GumInterceptor * self,
                                   GumInterceptorType type,
                                   gpointer function_address,
                                   gpointer replacement_function,
                                   gpointer replacement_data,
                                   gpointer * original_function,
                                   const GumInterceptorOptions * options)
{
  GumReplaceReturn result = GUM_REPLACE_OK;
  GumFunctionContext * function_ctx;
  GumInstrumentationError error;

  GUM_INTERCEPTOR_LOCK (self);
  gum_interceptor_transaction_begin (&self->current_transaction);
  self->current_transaction.is_dirty = TRUE;

  function_address = gum_interceptor_resolve (self, function_address);

  function_ctx = gum_interceptor_instrument (self, type, function_address,
      options, &error);

  if (function_ctx == NULL)
    goto instrumentation_error;

  if (function_ctx->replacement_function != NULL)
    goto already_replaced;

  function_ctx->replacement_data = replacement_data;
  function_ctx->replacement_function = replacement_function;

  if (original_function != NULL)
    *original_function = function_ctx->on_invoke_trampoline;

  goto beach;

instrumentation_error:
  {
    switch (error)
    {
      case GUM_INSTRUMENTATION_ERROR_WRONG_SIGNATURE:
        result = GUM_REPLACE_WRONG_SIGNATURE;
        break;
      case GUM_INSTRUMENTATION_ERROR_POLICY_VIOLATION:
        result = GUM_REPLACE_POLICY_VIOLATION;
        break;
      case GUM_INSTRUMENTATION_ERROR_WRONG_TYPE:
        result = GUM_REPLACE_WRONG_TYPE;
        break;
      default:
        g_assert_not_reached ();
    }
    goto beach;
  }
already_replaced:
  {
    result = GUM_REPLACE_ALREADY_REPLACED;
    goto beach;
  }
beach:
  {
    gum_interceptor_transaction_end (&self->current_transaction);
    GUM_INTERCEPTOR_UNLOCK (self);

    return result;
  }
}

/**
 * gum_interceptor_revert:
 * @self: the interceptor
 * @target: (not nullable): address of the function to revert
 *
 * Reverts a previous [method@Gum.Interceptor.replace] of @target, restoring the
 * original function. Has no effect if the function was not replaced.
 *
 * The change takes effect immediately unless a transaction is open.
 */
void
gum_interceptor_revert (GumInterceptor * self,
                        gpointer target)
{
  GumFunctionContext * function_ctx;

  GUM_INTERCEPTOR_LOCK (self);
  gum_interceptor_transaction_begin (&self->current_transaction);
  self->current_transaction.is_dirty = TRUE;

  target = gum_interceptor_resolve (self, target);

  function_ctx = (GumFunctionContext *) g_hash_table_lookup (
      self->function_by_address, target);
  if (function_ctx == NULL)
    goto beach;

  function_ctx->replacement_function = NULL;
  function_ctx->replacement_data = NULL;

  if (gum_function_context_is_empty (function_ctx))
  {
    g_hash_table_remove (self->function_by_address, target);
  }

beach:
  gum_interceptor_transaction_end (&self->current_transaction);
  GUM_INTERCEPTOR_UNLOCK (self);
}

/**
 * gum_interceptor_begin_transaction:
 * @self: the interceptor
 *
 * Begins a transaction, deferring activation of any attach, replace and revert
 * operations until the matching [method@Gum.Interceptor.end_transaction].
 * Batching changes this way is faster and lets a set of modifications be
 * applied as a unit. Transactions nest; only ending the outermost one applies
 * the changes.
 */
void
gum_interceptor_begin_transaction (GumInterceptor * self)
{
  GUM_INTERCEPTOR_LOCK (self);
  gum_interceptor_transaction_begin (&self->current_transaction);
  GUM_INTERCEPTOR_UNLOCK (self);
}

/**
 * gum_interceptor_end_transaction:
 * @self: the interceptor
 *
 * Ends a transaction started with [method@Gum.Interceptor.begin_transaction].
 * Ending the outermost transaction activates all changes made since it began.
 */
void
gum_interceptor_end_transaction (GumInterceptor * self)
{
  GUM_INTERCEPTOR_LOCK (self);
  gum_interceptor_transaction_end (&self->current_transaction);
  GUM_INTERCEPTOR_UNLOCK (self);
}

/**
 * gum_interceptor_flush:
 * @self: the interceptor
 *
 * Completes any teardown still pending from earlier detaches and reverts. When
 * a listener is detached or a replacement reverted the hook stops firing
 * immediately, but the memory backing its instrumentation can only be released
 * once no thread is left executing inside it, so that step is deferred. Call
 * this to force a pass and learn whether it finished. Does nothing while a
 * transaction is open.
 *
 * Returns: %TRUE if no teardown remains pending, %FALSE if some instrumented
 *   code may still be executing
 */
gboolean
gum_interceptor_flush (GumInterceptor * self)
{
  gboolean flushed = FALSE;

  GUM_INTERCEPTOR_LOCK (self);

  if (self->current_transaction.level == 0)
  {
    gum_interceptor_transaction_begin (&self->current_transaction);
    gum_interceptor_transaction_end (&self->current_transaction);

    flushed =
        g_queue_is_empty (self->current_transaction.pending_destroy_tasks);
  }

  GUM_INTERCEPTOR_UNLOCK (self);

  return flushed;
}

/**
 * gum_interceptor_flush_function:
 * @self: the interceptor
 * @function_address: (not nullable): address of the function of interest
 *
 * Like [method@Gum.Interceptor.flush], but reports specifically whether the
 * instrumentation for @function_address is no longer in use, so its memory can
 * be reclaimed.
 *
 * Returns: %TRUE if @function_address has no pending teardown left
 */
gboolean
gum_interceptor_flush_function (GumInterceptor * self,
                                gconstpointer function_address)
{
  gboolean flushed = TRUE;

  GUM_INTERCEPTOR_LOCK (self);

  if (self->current_transaction.level == 0)
  {
    gpointer target;
    GList * cur;

    gum_interceptor_transaction_begin (&self->current_transaction);
    gum_interceptor_transaction_end (&self->current_transaction);

    target = gum_interceptor_resolve (self, (gpointer) function_address);

    for (cur = self->current_transaction.pending_destroy_tasks->head;
        cur != NULL;
        cur = cur->next)
    {
      GumDestroyTask * task = cur->data;

      if (task->ctx->function_address == target)
      {
        flushed = FALSE;
        break;
      }
    }
  }
  else
  {
    flushed = FALSE;
  }

  GUM_INTERCEPTOR_UNLOCK (self);

  return flushed;
}

/**
 * gum_interceptor_flush_listener:
 * @self: the interceptor
 * @listener: (transfer none): the listener of interest
 *
 * Like [method@Gum.Interceptor.flush], but reports specifically whether
 * @listener is no longer referenced by any in-flight invocation, so it is safe
 * to release.
 *
 * Returns: %TRUE if @listener has no pending teardown left
 */
gboolean
gum_interceptor_flush_listener (GumInterceptor * self,
                                GumInvocationListener * listener)
{
  gboolean flushed = TRUE;

  GUM_INTERCEPTOR_LOCK (self);

  if (self->current_transaction.level == 0)
  {
    GList * cur;

    gum_interceptor_transaction_begin (&self->current_transaction);
    gum_interceptor_transaction_end (&self->current_transaction);

    for (cur = self->current_transaction.pending_destroy_tasks->head;
        cur != NULL;
        cur = cur->next)
    {
      GumDestroyTask * task = cur->data;

      if (task->data == listener)
      {
        flushed = FALSE;
        break;
      }
    }
  }
  else
  {
    flushed = FALSE;
  }

  GUM_INTERCEPTOR_UNLOCK (self);

  return flushed;
}

/**
 * gum_interceptor_get_current_invocation:
 *
 * Returns the current invocation context.
 *
 * Returns: (transfer none) (nullable): the invocation context, or
 *   %NULL if not in an intercepted call
 */
GumInvocationContext *
gum_interceptor_get_current_invocation (void)
{
  InterceptorThreadContext * interceptor_ctx;
  GumInvocationStackEntry * entry;

  interceptor_ctx = get_interceptor_thread_context ();
  entry = gum_invocation_stack_peek_top (interceptor_ctx->stack);
  if (entry == NULL)
    return NULL;

  return &entry->invocation_context;
}

/**
 * gum_interceptor_get_live_replacement_invocation:
 * @replacement_function: the replacement function
 *
 * Returns the invocation context for the given replacement
 * function, if currently active.
 *
 * Returns: (transfer none) (nullable): the invocation context, or
 *   %NULL if not in the specified replacement
 */
GumInvocationContext *
gum_interceptor_get_live_replacement_invocation (gpointer replacement_function)
{
  InterceptorThreadContext * interceptor_ctx;
  GumInvocationStackEntry * entry;

  interceptor_ctx = get_interceptor_thread_context ();
  entry = gum_invocation_stack_peek_top (interceptor_ctx->stack);
  if (entry == NULL)
    return NULL;
  if (!entry->calling_replacement)
    return NULL;
  if (replacement_function != entry->function_ctx->replacement_function)
    return NULL;

  return &entry->invocation_context;
}

/**
 * gum_interceptor_get_current_stack:
 *
 * Returns the invocation stack for the current thread.
 *
 * Returns: (transfer none): the invocation stack
 */
GumInvocationStack *
gum_interceptor_get_current_stack (void)
{
  InterceptorThreadContext * context;

  context = g_private_get (&gum_interceptor_context_private);
  if (context == NULL)
    return &_gum_interceptor_empty_stack;

  return context->stack;
}

/**
 * gum_interceptor_ignore_current_thread:
 * @self: the interceptor
 *
 * Temporarily stops the calling thread's calls into hooked code from
 * triggering listeners. The typical use is to bracket work done internally by
 * an injected payload — for example its own worker threads — so that a user's
 * hooks observe only the target process's activity, not the payload's own
 * calls into the functions it has hooked.
 *
 * Listeners marked unignorable still fire (see
 * [enum@Gum.InvocationIgnorability]). Note that re-entrancy from within a
 * listener's own `on_enter`/`on_leave` is already prevented automatically, so
 * this is not needed for that.
 *
 * Nestable, and balanced by [method@Gum.Interceptor.unignore_current_thread].
 */
void
gum_interceptor_ignore_current_thread (GumInterceptor * self)
{
  InterceptorThreadContext * interceptor_ctx;

  interceptor_ctx = get_interceptor_thread_context ();
  interceptor_ctx->ignore_level++;
}

/**
 * gum_interceptor_unignore_current_thread:
 * @self: the interceptor
 *
 * Undoes one [method@Gum.Interceptor.ignore_current_thread] on the calling
 * thread.
 */
void
gum_interceptor_unignore_current_thread (GumInterceptor * self)
{
  InterceptorThreadContext * interceptor_ctx;

  interceptor_ctx = get_interceptor_thread_context ();
  interceptor_ctx->ignore_level--;
}

/**
 * gum_interceptor_maybe_unignore_current_thread:
 * @self: the interceptor
 *
 * Undoes one [method@Gum.Interceptor.ignore_current_thread], but only if the
 * calling thread is currently being ignored.
 *
 * Returns: %TRUE if the thread was being ignored and is now one level less so
 */
gboolean
gum_interceptor_maybe_unignore_current_thread (GumInterceptor * self)
{
  InterceptorThreadContext * interceptor_ctx;

  interceptor_ctx = get_interceptor_thread_context ();
  if (interceptor_ctx->ignore_level <= 0)
    return FALSE;

  interceptor_ctx->ignore_level--;
  return TRUE;
}

/**
 * gum_interceptor_ignore_other_threads:
 * @self: the interceptor
 *
 * Restricts interception to the calling thread: invocations on all other
 * threads stop triggering listeners until
 * [method@Gum.Interceptor.unignore_other_threads] is called.
 */
void
gum_interceptor_ignore_other_threads (GumInterceptor * self)
{
  self->selected_thread_id = gum_process_get_current_thread_id ();
}

/**
 * gum_interceptor_unignore_other_threads:
 * @self: the interceptor
 *
 * Lifts a previous [method@Gum.Interceptor.ignore_other_threads], resuming
 * interception on all threads. Must be called from the same thread that
 * ignored the others.
 */
void
gum_interceptor_unignore_other_threads (GumInterceptor * self)
{
  g_assert (self->selected_thread_id == gum_process_get_current_thread_id ());
  self->selected_thread_id = 0;
}

/**
 * gum_invocation_stack_translate:
 * @self: the invocation stack
 * @return_address: a potentially hijacked return address
 *
 * Translates @return_address back to its real value. While a listener is
 * active the interceptor temporarily replaces on-stack return addresses with
 * its own trampoline; this resolves such an address to the caller's true
 * return address, leaving any unrelated address unchanged.
 *
 * Returns: the real return address
 */
gpointer
gum_invocation_stack_translate (GumInvocationStack * self,
                                gpointer return_address)
{
  guint i;

  for (i = 0; i != self->len; i++)
  {
    GumInvocationStackEntry * entry;

    entry = &g_array_index (self, GumInvocationStackEntry, i);
    if (entry->function_ctx->on_leave_trampoline == return_address)
      return entry->caller_ret_addr;
  }

  return return_address;
}

/**
 * gum_interceptor_save:
 * @state: (out): return location for the saved state
 *
 * Records the calling thread's current invocation depth into @state, to be
 * restored later with [func@Gum.Interceptor.restore]. Use this around a
 * non-local exit such as a `longjmp()` that would otherwise skip the
 * bookkeeping the interceptor does as intercepted calls return.
 */
void
gum_interceptor_save (GumInvocationState * state)
{
  *state = gum_interceptor_get_current_stack ()->len;
}

/**
 * gum_interceptor_restore:
 * @state: (in): the state previously saved with [func@Gum.Interceptor.save]
 *
 * Unwinds the calling thread's invocation stack back to the depth recorded in
 * @state, releasing any entries skipped by a non-local exit.
 */
void
gum_interceptor_restore (GumInvocationState * state)
{
  GumInvocationStack * stack;
  guint old_depth, new_depth, i;

  stack = gum_interceptor_get_current_stack ();

  old_depth = *state;
  new_depth = stack->len;
  if (new_depth == old_depth)
    return;

  for (i = old_depth; i != new_depth; i++)
  {
    GumInvocationStackEntry * entry;

    entry = &g_array_index (stack, GumInvocationStackEntry, i);

    g_atomic_int_dec_and_test (&entry->function_ctx->trampoline_usage_counter);
  }

  g_array_set_size (stack, old_depth);
}

/**
 * gum_interceptor_with_lock_held:
 * @self: the interceptor
 * @func: (scope call): function to call while holding the lock
 * @user_data: data to pass to @func
 *
 * Calls @func while holding the interceptor lock.
 */
void
gum_interceptor_with_lock_held (GumInterceptor * self,
                                GumInterceptorLockedFunc func,
                                gpointer user_data)
{
  GUM_INTERCEPTOR_LOCK (self);
  func (user_data);
  GUM_INTERCEPTOR_UNLOCK (self);
}

/**
 * gum_interceptor_is_locked:
 * @self: the interceptor
 *
 * Checks whether the interceptor lock is currently held, e.g. to decide
 * whether it is safe to make changes from a signal handler.
 *
 * Returns: %TRUE if the lock is held
 */
gboolean
gum_interceptor_is_locked (GumInterceptor * self)
{
  if (!g_rec_mutex_trylock (&self->mutex))
    return TRUE;

  GUM_INTERCEPTOR_UNLOCK (self);
  return FALSE;
}

/**
 * gum_interceptor_detect_hook_size: (skip)
 * @code: code address to analyze
 * @capstone: Capstone handle
 * @insn: Capstone instruction
 *
 * Detects the minimum hook size needed at the given code address.
 *
 * Returns: the hook size in bytes
 */
gsize
gum_interceptor_detect_hook_size (gconstpointer code,
                                  csh capstone,
                                  cs_insn * insn)
{
  return _gum_interceptor_backend_detect_hook_size (code, capstone, insn);
}

void
_gum_interceptor_forget_all_hooks_in_range (const GumMemoryRange * range)
{
  GumInterceptor * interceptor;

  g_mutex_lock (&_gum_interceptor_lock);
  interceptor = (_the_interceptor != NULL)
      ? g_object_ref (_the_interceptor)
      : NULL;
  g_mutex_unlock (&_gum_interceptor_lock);

  if (interceptor == NULL)
    return;

  gum_interceptor_discard_function_contexts_in_range (interceptor, range);

  g_object_unref (interceptor);
}

static void
gum_interceptor_discard_function_contexts_in_range (
    GumInterceptor * self,
    const GumMemoryRange * range)
{
  GHashTableIter iter;
  gpointer value;

  GUM_INTERCEPTOR_LOCK (self);
  gum_interceptor_transaction_begin (&self->current_transaction);

  g_hash_table_iter_init (&iter, self->function_by_address);
  while (g_hash_table_iter_next (&iter, NULL, &value))
  {
    GumFunctionContext * function_ctx = value;
    GumAddress function_address = GUM_ADDRESS (
        _gum_interceptor_backend_get_function_address (function_ctx));

    if (!GUM_MEMORY_RANGE_INCLUDES (range, function_address))
      continue;

    self->current_transaction.is_dirty = TRUE;

    g_hash_table_iter_steal (&iter);
    gum_function_context_discard (function_ctx);
  }

  gum_interceptor_transaction_end (&self->current_transaction);
  GUM_INTERCEPTOR_UNLOCK (self);
}

gpointer
_gum_interceptor_peek_top_caller_return_address (void)
{
  GumInvocationStack * stack;
  GumInvocationStackEntry * entry;

  stack = gum_interceptor_get_current_stack ();
  if (stack->len == 0)
    return NULL;

  entry = &g_array_index (stack, GumInvocationStackEntry, stack->len - 1);

  return entry->caller_ret_addr;
}

gpointer
_gum_interceptor_translate_top_return_address (gpointer return_address)
{
  GumInvocationStack * stack;
  GumInvocationStackEntry * entry;

  stack = gum_interceptor_get_current_stack ();
  if (stack->len == 0)
    goto fallback;

  entry = &g_array_index (stack, GumInvocationStackEntry, stack->len - 1);
  if (entry->function_ctx->on_leave_trampoline != return_address)
    goto fallback;

  return entry->caller_ret_addr;

fallback:
  return return_address;
}

static GumFunctionContext *
gum_interceptor_instrument (GumInterceptor * self,
                            GumInterceptorType type,
                            gpointer function_address,
                            const GumInterceptorOptions * instrumentation,
                            GumInstrumentationError * error)
{
  GumFunctionContext * ctx;
  GumInterceptorOptions effective;
  const GumInterceptorOptions * defaults;
  gboolean force;

  *error = GUM_INSTRUMENTATION_ERROR_NONE;

  ctx = (GumFunctionContext *) g_hash_table_lookup (self->function_by_address,
      function_address);

  if (ctx != NULL)
  {
    if (ctx->type != type)
    {
      *error = GUM_INSTRUMENTATION_ERROR_WRONG_TYPE;
      return NULL;
    }
    return ctx;
  }

  if (self->backend == NULL)
  {
    self->backend =
        _gum_interceptor_backend_create (&self->mutex, &self->allocator);
  }

  ctx = gum_function_context_new (self, function_address, type);
  effective = *instrumentation;
  defaults = &self->options;
  if (effective.scratch_register == 0)
    effective.scratch_register = defaults->scratch_register;
  if (effective.scenario == GUM_INTERCEPTOR_SCENARIO_DEFAULT)
    effective.scenario = defaults->scenario;
  if (effective.relocation_policy == GUM_RELOCATION_DEFAULT)
    effective.relocation_policy = defaults->relocation_policy;
  if (effective.write_redirect == NULL)
  {
    effective.write_redirect = defaults->write_redirect;
    effective.write_redirect_data = defaults->write_redirect_data;
  }
  if (effective.redirect_space_hint == 0)
    effective.redirect_space_hint = defaults->redirect_space_hint;

  ctx->scratch_register = effective.scratch_register;
  ctx->scenario = effective.scenario;
  ctx->relocation_policy = effective.relocation_policy;
  ctx->write_redirect = effective.write_redirect;
  ctx->write_redirect_data = effective.write_redirect_data;
  ctx->redirect_space_hint = effective.redirect_space_hint;

  force = effective.relocation_policy == GUM_RELOCATION_FORCED;

  if (gum_process_get_code_signing_policy () == GUM_CODE_SIGNING_REQUIRED)
  {
    if (!_gum_interceptor_backend_claim_grafted_trampoline (self->backend, ctx))
      goto policy_violation;
  }
  else
  {
    if (!_gum_interceptor_backend_create_trampoline (self->backend, ctx, force))
      goto wrong_signature;
  }

  g_hash_table_insert (self->function_by_address, function_address, ctx);

  gum_interceptor_transaction_schedule_update (&self->current_transaction, ctx,
      gum_interceptor_activate);

  return ctx;

policy_violation:
  {
    *error = GUM_INSTRUMENTATION_ERROR_POLICY_VIOLATION;
    goto propagate_error;
  }
wrong_signature:
  {
    *error = GUM_INSTRUMENTATION_ERROR_WRONG_SIGNATURE;
    goto propagate_error;
  }
propagate_error:
  {
    gum_function_context_finalize (ctx);

    return NULL;
  }
}

static void
gum_interceptor_activate (GumInterceptor * self,
                          GumFunctionContext * ctx,
                          gpointer prologue)
{
  if (ctx->destroyed)
    return;

  g_assert (!ctx->activated);
  ctx->activated = TRUE;

  _gum_interceptor_backend_activate_trampoline (self->backend, ctx,
      prologue);
}

static void
gum_interceptor_deactivate (GumInterceptor * self,
                            GumFunctionContext * ctx,
                            gpointer prologue)
{
  GumInterceptorBackend * backend = self->backend;

  g_assert (ctx->activated);
  ctx->activated = FALSE;

  _gum_interceptor_backend_deactivate_trampoline (backend, ctx, prologue);
}

static void
gum_interceptor_transaction_init (GumInterceptorTransaction * transaction,
                                  GumInterceptor * interceptor)
{
  transaction->is_dirty = FALSE;
  transaction->level = 0;
  transaction->pending_destroy_tasks = g_queue_new ();
  transaction->pending_update_tasks = g_hash_table_new_full (
      NULL, NULL, NULL, (GDestroyNotify) g_array_unref);

  transaction->interceptor = interceptor;
}

static void
gum_interceptor_transaction_destroy (GumInterceptorTransaction * transaction)
{
  GumDestroyTask * task;

  g_hash_table_unref (transaction->pending_update_tasks);

  while ((task = g_queue_pop_head (transaction->pending_destroy_tasks)) != NULL)
  {
    task->notify (task->data);

    g_slice_free (GumDestroyTask, task);
  }
  g_queue_free (transaction->pending_destroy_tasks);
}

static void
gum_interceptor_transaction_begin (GumInterceptorTransaction * self)
{
  self->level++;
}

static void
gum_interceptor_transaction_end (GumInterceptorTransaction * self)
{
  GumInterceptor * interceptor = self->interceptor;
  GumInterceptorTransaction transaction_copy;
  GPtrArray * addresses;
  GHashTableIter iter;
  gpointer address;

  self->level--;
  if (self->level > 0)
    return;

  if (!self->is_dirty)
    return;

  gum_interceptor_ignore_current_thread (interceptor);

  gum_code_allocator_commit (&interceptor->allocator);

  if (g_queue_is_empty (self->pending_destroy_tasks) &&
      g_hash_table_size (self->pending_update_tasks) == 0)
  {
    interceptor->current_transaction.is_dirty = FALSE;
    goto no_changes;
  }

  transaction_copy = interceptor->current_transaction;
  self = &transaction_copy;
  gum_interceptor_transaction_init (&interceptor->current_transaction,
      interceptor);

  addresses =
      g_ptr_array_sized_new (g_hash_table_size (self->pending_update_tasks));
  g_hash_table_iter_init (&iter, self->pending_update_tasks);
  while (g_hash_table_iter_next (&iter, &address, NULL))
    g_ptr_array_add (addresses, address);
  g_ptr_array_sort (addresses, (GCompareFunc) gum_page_address_compare);

  if (gum_process_get_code_signing_policy () == GUM_CODE_SIGNING_REQUIRED)
  {
    guint addr_index;

    for (addr_index = 0; addr_index != addresses->len; addr_index++)
    {
      gpointer target_page;
      GArray * pending;
      guint i;

      target_page = g_ptr_array_index (addresses, addr_index);

      pending = g_hash_table_lookup (self->pending_update_tasks, target_page);
      g_assert (pending != NULL);

      for (i = 0; i != pending->len; i++)
      {
        GumUpdateTask * update;

        update = &g_array_index (pending, GumUpdateTask, i);

        update->func (interceptor, update->ctx,
            _gum_interceptor_backend_get_function_address (update->ctx));
      }
    }
  }
  else if (!gum_memory_patch_code_pages (addresses, FALSE, gum_apply_updates,
        self))
  {
    g_abort ();
  }

  g_ptr_array_unref (addresses);

  {
    GumDestroyTask * task;

    while ((task = g_queue_pop_head (self->pending_destroy_tasks)) != NULL)
    {
      if (task->ctx->trampoline_usage_counter == 0)
      {
        GUM_INTERCEPTOR_UNLOCK (interceptor);
        task->notify (task->data);
        GUM_INTERCEPTOR_LOCK (interceptor);

        g_slice_free (GumDestroyTask, task);
      }
      else
      {
        interceptor->current_transaction.is_dirty = TRUE;
        g_queue_push_tail (
            interceptor->current_transaction.pending_destroy_tasks, task);
      }
    }
  }

  gum_interceptor_transaction_destroy (self);

no_changes:
  gum_interceptor_unignore_current_thread (interceptor);
}

static void
gum_apply_updates (gpointer source_page,
                   gpointer target_page,
                   guint n_pages,
                   gpointer user_data)
{
  GumInterceptorTransaction * self = user_data;
  GArray * pending;
  guint i;

  pending = g_hash_table_lookup (self->pending_update_tasks, target_page);
  g_assert (pending != NULL);

  for (i = 0; i != pending->len; i++)
  {
    GumUpdateTask * update;
    gsize offset;

    update = &g_array_index (pending, GumUpdateTask, i);

    offset = (guint8 *)
        _gum_interceptor_backend_get_function_address (update->ctx) -
        (guint8 *) target_page;

    update->func (self->interceptor, update->ctx,
        (guint8 *) source_page + offset);
  }
}

static void
gum_interceptor_transaction_schedule_destroy (GumInterceptorTransaction * self,
                                              GumFunctionContext * ctx,
                                              GDestroyNotify notify,
                                              gpointer data)
{
  GumDestroyTask * task;

  task = g_slice_new (GumDestroyTask);
  task->ctx = ctx;
  task->notify = notify;
  task->data = data;

  g_queue_push_tail (self->pending_destroy_tasks, task);
}

static void
gum_interceptor_transaction_schedule_update (GumInterceptorTransaction * self,
                                             GumFunctionContext * ctx,
                                             GumUpdateTaskFunc func)
{
  guint8 * function_address;
  gpointer start_page, end_page;
  GArray * pending;
  GumUpdateTask update;

  function_address = _gum_interceptor_backend_get_function_address (ctx);

  start_page = gum_page_address_from_pointer (function_address);
  end_page = gum_page_address_from_pointer (function_address +
      ctx->overwritten_prologue_len - 1);

  pending = g_hash_table_lookup (self->pending_update_tasks, start_page);
  if (pending == NULL)
  {
    pending = g_array_new (FALSE, FALSE, sizeof (GumUpdateTask));
    g_hash_table_insert (self->pending_update_tasks, start_page, pending);
  }

  update.ctx = ctx;
  update.func = func;
  g_array_append_val (pending, update);

  if (end_page != start_page)
  {
    pending = g_hash_table_lookup (self->pending_update_tasks, end_page);
    if (pending == NULL)
    {
      pending = g_array_new (FALSE, FALSE, sizeof (GumUpdateTask));
      g_hash_table_insert (self->pending_update_tasks, end_page, pending);
    }
  }
}

static GumFunctionContext *
gum_function_context_new (GumInterceptor * interceptor,
                          gpointer function_address,
                          GumInterceptorType type)
{
  GumFunctionContext * ctx;

  ctx = g_slice_new0 (GumFunctionContext);
  ctx->function_address = function_address;
  ctx->type = type;
  ctx->listener_entries =
      g_ptr_array_new_full (1, (GDestroyNotify) listener_entry_free);
  ctx->interceptor = interceptor;

  return ctx;
}

static void
gum_function_context_finalize (GumFunctionContext * function_ctx)
{
  g_assert (function_ctx->trampoline_slice == NULL);

  g_ptr_array_unref (
      (GPtrArray *) g_atomic_pointer_get (&function_ctx->listener_entries));

  g_free (function_ctx->overwritten_prologue);
  g_free (function_ctx->redirect_code);

  g_slice_free (GumFunctionContext, function_ctx);
}

static void
gum_function_context_destroy (GumFunctionContext * function_ctx)
{
  GumInterceptorTransaction * transaction =
      &function_ctx->interceptor->current_transaction;

  g_assert (!function_ctx->destroyed);
  function_ctx->destroyed = TRUE;

  if (function_ctx->activated)
  {
    gum_interceptor_transaction_schedule_update (transaction, function_ctx,
        gum_interceptor_deactivate);
  }

  gum_interceptor_transaction_schedule_destroy (transaction, function_ctx,
      (GDestroyNotify) gum_function_context_perform_destroy, function_ctx);
}

static void
gum_function_context_discard (GumFunctionContext * function_ctx)
{
  GumInterceptorTransaction * transaction =
      &function_ctx->interceptor->current_transaction;

  function_ctx->destroyed = TRUE;
  function_ctx->activated = FALSE;

  gum_interceptor_transaction_schedule_destroy (transaction, function_ctx,
      (GDestroyNotify) gum_function_context_perform_destroy, function_ctx);
}

static void
gum_function_context_perform_destroy (GumFunctionContext * function_ctx)
{
  _gum_interceptor_backend_destroy_trampoline (
      function_ctx->interceptor->backend, function_ctx);

  gum_function_context_finalize (function_ctx);
}

static gboolean
gum_function_context_is_empty (GumFunctionContext * function_ctx)
{
  if (function_ctx->replacement_function != NULL)
    return FALSE;

  return gum_function_context_find_taken_listener_slot (function_ctx) == NULL;
}

static void
gum_function_context_add_listener (GumFunctionContext * function_ctx,
                                   GumInvocationListener * listener,
                                   gpointer function_data,
                                   gboolean unignorable)
{
  ListenerEntry * entry;
  GPtrArray * old_entries, * new_entries;
  guint i;

  entry = g_slice_new (ListenerEntry);
  entry->listener_interface = GUM_INVOCATION_LISTENER_GET_IFACE (listener);
  entry->listener_instance = listener;
  entry->function_data = function_data;
  entry->unignorable = unignorable;

  old_entries =
      (GPtrArray *) g_atomic_pointer_get (&function_ctx->listener_entries);
  new_entries = g_ptr_array_new_full (old_entries->len + 1,
      (GDestroyNotify) listener_entry_free);
  for (i = 0; i != old_entries->len; i++)
  {
    ListenerEntry * old_entry = g_ptr_array_index (old_entries, i);
    if (old_entry != NULL)
      g_ptr_array_add (new_entries, g_slice_dup (ListenerEntry, old_entry));
  }
  g_ptr_array_add (new_entries, entry);

  g_atomic_pointer_set (&function_ctx->listener_entries, new_entries);
  gum_interceptor_transaction_schedule_destroy (
      &function_ctx->interceptor->current_transaction, function_ctx,
      (GDestroyNotify) g_ptr_array_unref, old_entries);

  if (entry->listener_interface->on_leave != NULL)
    function_ctx->has_on_leave_listener = TRUE;

  if (unignorable)
    function_ctx->has_unignorable_listener = TRUE;
}

static void
listener_entry_free (ListenerEntry * entry)
{
  g_slice_free (ListenerEntry, entry);
}

static void
gum_function_context_remove_listener (GumFunctionContext * function_ctx,
                                      GumInvocationListener * listener)
{
  ListenerEntry ** slot;
  gboolean has_on_leave_listener, has_unignorable_listener;
  GPtrArray * listener_entries;
  guint i;

  slot = gum_function_context_find_listener (function_ctx, listener);
  g_assert (slot != NULL);
  listener_entry_free (*slot);
  *slot = NULL;

  has_on_leave_listener = FALSE;
  has_unignorable_listener = FALSE;
  listener_entries =
      (GPtrArray *) g_atomic_pointer_get (&function_ctx->listener_entries);
  for (i = 0; i != listener_entries->len; i++)
  {
    ListenerEntry * entry = g_ptr_array_index (listener_entries, i);

    if (entry == NULL)
      continue;

    if (entry->listener_interface->on_leave != NULL)
      has_on_leave_listener = TRUE;

    if (entry->unignorable)
      has_unignorable_listener = TRUE;
  }
  function_ctx->has_on_leave_listener = has_on_leave_listener;
  function_ctx->has_unignorable_listener = has_unignorable_listener;
}

static gboolean
gum_function_context_has_listener (GumFunctionContext * function_ctx,
                                   GumInvocationListener * listener)
{
  return gum_function_context_find_listener (function_ctx, listener) != NULL;
}

static ListenerEntry **
gum_function_context_find_listener (GumFunctionContext * function_ctx,
                                    GumInvocationListener * listener)
{
  GPtrArray * listener_entries;
  guint i;

  listener_entries =
      (GPtrArray *) g_atomic_pointer_get (&function_ctx->listener_entries);
  for (i = 0; i != listener_entries->len; i++)
  {
    ListenerEntry ** slot = (ListenerEntry **)
        &g_ptr_array_index (listener_entries, i);
    if (*slot != NULL && (*slot)->listener_instance == listener)
      return slot;
  }

  return NULL;
}

static ListenerEntry **
gum_function_context_find_taken_listener_slot (
    GumFunctionContext * function_ctx)
{
  GPtrArray * listener_entries;
  guint i;

  listener_entries =
      (GPtrArray *) g_atomic_pointer_get (&function_ctx->listener_entries);
  for (i = 0; i != listener_entries->len; i++)
  {
    ListenerEntry ** slot = (ListenerEntry **)
        &g_ptr_array_index (listener_entries, i);
    if (*slot != NULL)
      return slot;
  }

  return NULL;
}

gboolean
_gum_function_context_begin_invocation (GumFunctionContext * function_ctx,
                                        GumCpuContext * cpu_context,
                                        gpointer * caller_ret_addr,
                                        gpointer * next_hop)
{
  GumInterceptor * interceptor;
  InterceptorThreadContext * interceptor_ctx;
  GumInvocationStack * stack;
  GumInvocationStackEntry * stack_entry;
  GumInvocationContext * invocation_ctx = NULL;
  gpointer stack_address;
  gint system_error;
  gboolean invoke_listeners = TRUE;
  gboolean only_invoke_unignorable_listeners = FALSE;
  gboolean will_trap_on_leave = FALSE;

  g_atomic_int_inc (&function_ctx->trampoline_usage_counter);

  interceptor = function_ctx->interceptor;

#ifdef HAVE_WINDOWS
  system_error = gum_thread_get_system_error ();
#endif

  if (gum_tls_key_get_value (gum_interceptor_guard_key) == interceptor)
  {
    *next_hop = function_ctx->on_invoke_trampoline;
    goto bypass;
  }
  gum_tls_key_set_value (gum_interceptor_guard_key, interceptor);

  interceptor_ctx = get_interceptor_thread_context ();
  stack = interceptor_ctx->stack;

  stack_entry = gum_invocation_stack_peek_top (stack);
  if (stack_entry != NULL &&
      stack_entry->calling_replacement &&
      gum_strip_code_pointer (GUM_FUNCPTR_TO_POINTER (
          stack_entry->invocation_context.function)) ==
          function_ctx->function_address)
  {
    gum_tls_key_set_value (gum_interceptor_guard_key, NULL);
    *next_hop = function_ctx->on_invoke_trampoline;
    goto bypass;
  }

#ifndef HAVE_WINDOWS
  system_error = gum_thread_get_system_error ();
#endif

  if (interceptor->selected_thread_id != 0)
  {
    invoke_listeners =
        gum_process_get_current_thread_id () == interceptor->selected_thread_id;
  }

  if (invoke_listeners)
  {
    invoke_listeners = (interceptor_ctx->ignore_level <= 0);
  }

  if (!invoke_listeners && function_ctx->has_unignorable_listener)
  {
    invoke_listeners = TRUE;
    only_invoke_unignorable_listeners = TRUE;
  }

  stack_address = GUM_INTERCEPTOR_CPU_CONTEXT_SP (cpu_context);
  gum_invocation_stack_reap_unwound (stack, stack_address);

  will_trap_on_leave = function_ctx->replacement_function != NULL ||
      (invoke_listeners && function_ctx->has_on_leave_listener);
  if (will_trap_on_leave)
  {
    stack_entry = gum_invocation_stack_push (stack, function_ctx,
        *caller_ret_addr, stack_address, only_invoke_unignorable_listeners);
    invocation_ctx = &stack_entry->invocation_context;
  }
  else if (invoke_listeners)
  {
    stack_entry = gum_invocation_stack_push (stack, function_ctx,
        function_ctx->function_address, stack_address,
        only_invoke_unignorable_listeners);
    invocation_ctx = &stack_entry->invocation_context;
  }

  if (invocation_ctx != NULL)
    invocation_ctx->system_error = system_error;

  gum_function_context_fixup_cpu_context (function_ctx, cpu_context);

  if (invoke_listeners)
  {
    GPtrArray * listener_entries;
    guint i;

    invocation_ctx->cpu_context = cpu_context;
    invocation_ctx->backend = &interceptor_ctx->listener_backend;

    listener_entries =
        (GPtrArray *) g_atomic_pointer_get (&function_ctx->listener_entries);
    for (i = 0; i != listener_entries->len; i++)
    {
      ListenerEntry * listener_entry;
      ListenerInvocationState state;

      listener_entry = g_ptr_array_index (listener_entries, i);
      if (listener_entry == NULL)
        continue;

      if (only_invoke_unignorable_listeners && !listener_entry->unignorable)
        continue;

      state.point_cut = GUM_POINT_ENTER;
      state.entry = listener_entry;
      state.interceptor_ctx = interceptor_ctx;
      state.invocation_data = stack_entry->listener_invocation_data[i];
      invocation_ctx->backend->data = &state;

      if (listener_entry->listener_interface->on_enter != NULL)
      {
        listener_entry->listener_interface->on_enter (
            listener_entry->listener_instance, invocation_ctx);
      }
    }

    system_error = invocation_ctx->system_error;
  }

  if (!will_trap_on_leave && invoke_listeners)
  {
    gum_invocation_stack_pop (interceptor_ctx->stack);
  }

  gum_thread_set_system_error (system_error);

  gum_tls_key_set_value (gum_interceptor_guard_key, NULL);

  if (will_trap_on_leave)
  {
    *caller_ret_addr = function_ctx->on_leave_trampoline;
  }

  if (function_ctx->replacement_function != NULL)
  {
    stack_entry->calling_replacement = TRUE;
    gum_invocation_stack_entry_snapshot_cpu_context (stack_entry, cpu_context);
    stack_entry->original_system_error = system_error;
    invocation_ctx->cpu_context = &stack_entry->cpu_context;
    invocation_ctx->backend = &interceptor_ctx->replacement_backend;
    invocation_ctx->backend->data = function_ctx->replacement_data;

    *next_hop = function_ctx->replacement_function;
  }
  else
  {
    *next_hop = function_ctx->on_invoke_trampoline;
  }

bypass:
  if (!will_trap_on_leave)
  {
    g_atomic_int_dec_and_test (&function_ctx->trampoline_usage_counter);
  }

  return will_trap_on_leave;
}

void
_gum_function_context_end_invocation (GumFunctionContext * function_ctx,
                                      GumCpuContext * cpu_context,
                                      gpointer * next_hop)
{
  gint system_error;
  InterceptorThreadContext * interceptor_ctx;
  GumInvocationStackEntry * stack_entry;
  GumInvocationContext * invocation_ctx;
  GPtrArray * listener_entries;
  gboolean only_invoke_unignorable_listeners;
  guint i;

#ifdef HAVE_WINDOWS
  system_error = gum_thread_get_system_error ();
#endif

  gum_tls_key_set_value (gum_interceptor_guard_key, function_ctx->interceptor);

#ifndef HAVE_WINDOWS
  system_error = gum_thread_get_system_error ();
#endif

  interceptor_ctx = get_interceptor_thread_context ();

  gum_invocation_stack_reap_unwound_above (interceptor_ctx->stack,
      function_ctx);

  stack_entry = gum_invocation_stack_peek_top (interceptor_ctx->stack);
  *next_hop = gum_sign_code_pointer (stack_entry->caller_ret_addr);

  invocation_ctx = &stack_entry->invocation_context;
  invocation_ctx->cpu_context = cpu_context;
  if (stack_entry->calling_replacement &&
      invocation_ctx->system_error != stack_entry->original_system_error)
  {
    system_error = invocation_ctx->system_error;
  }
  else
  {
    invocation_ctx->system_error = system_error;
  }
  invocation_ctx->backend = &interceptor_ctx->listener_backend;

  gum_function_context_fixup_cpu_context (function_ctx, cpu_context);

  listener_entries =
      (GPtrArray *) g_atomic_pointer_get (&function_ctx->listener_entries);
  only_invoke_unignorable_listeners =
      stack_entry->only_invoke_unignorable_listeners;
  for (i = 0; i != listener_entries->len; i++)
  {
    ListenerEntry * listener_entry;
    ListenerInvocationState state;

    listener_entry = g_ptr_array_index (listener_entries, i);
    if (listener_entry == NULL)
      continue;

    if (only_invoke_unignorable_listeners && !listener_entry->unignorable)
      continue;

    state.point_cut = GUM_POINT_LEAVE;
    state.entry = listener_entry;
    state.interceptor_ctx = interceptor_ctx;
    state.invocation_data = stack_entry->listener_invocation_data[i];
    invocation_ctx->backend->data = &state;

    if (listener_entry->listener_interface->on_leave != NULL)
    {
      listener_entry->listener_interface->on_leave (
          listener_entry->listener_instance, invocation_ctx);
    }
  }

  gum_thread_set_system_error (invocation_ctx->system_error);

  gum_invocation_stack_pop (interceptor_ctx->stack);

  gum_tls_key_set_value (gum_interceptor_guard_key, NULL);

  g_atomic_int_dec_and_test (&function_ctx->trampoline_usage_counter);
}

static void
gum_function_context_fixup_cpu_context (GumFunctionContext * function_ctx,
                                        GumCpuContext * cpu_context)
{
  gsize pc;

  pc = GPOINTER_TO_SIZE (function_ctx->function_address);
#ifdef HAVE_ARM
  pc &= ~1;
#endif

#if defined (HAVE_I386)
# if GLIB_SIZEOF_VOID_P == 4
  cpu_context->eip = pc;
# else
  cpu_context->rip = pc;
# endif
#elif defined (HAVE_ARM)
  cpu_context->pc = pc;
#elif defined (HAVE_ARM64)
  cpu_context->pc = pc;
#elif defined (HAVE_MIPS)
  cpu_context->pc = pc;
#else
# error Unsupported architecture
#endif
}

static InterceptorThreadContext *
get_interceptor_thread_context (void)
{
  InterceptorThreadContext * context;

  context = g_private_get (&gum_interceptor_context_private);
  if (context == NULL)
  {
    context = interceptor_thread_context_new ();

    gum_spinlock_acquire (&gum_interceptor_thread_context_lock);
    g_hash_table_add (gum_interceptor_thread_contexts, context);
    gum_spinlock_release (&gum_interceptor_thread_context_lock);

    g_private_set (&gum_interceptor_context_private, context);
  }

  return context;
}

static void
release_interceptor_thread_context (InterceptorThreadContext * context)
{
  if (gum_interceptor_thread_contexts == NULL)
    return;

  gum_spinlock_acquire (&gum_interceptor_thread_context_lock);
  g_hash_table_remove (gum_interceptor_thread_contexts, context);
  gum_spinlock_release (&gum_interceptor_thread_context_lock);
}

static GumPointCut
gum_interceptor_invocation_get_listener_point_cut (
    GumInvocationContext * context)
{
  return ((ListenerInvocationState *) context->backend->data)->point_cut;
}

static GumPointCut
gum_interceptor_invocation_get_replacement_point_cut (
    GumInvocationContext * context)
{
  return GUM_POINT_ENTER;
}

static GumThreadId
gum_interceptor_invocation_get_thread_id (GumInvocationContext * context)
{
  return gum_process_get_current_thread_id ();
}

static guint
gum_interceptor_invocation_get_depth (GumInvocationContext * context)
{
  InterceptorThreadContext * interceptor_ctx =
      (InterceptorThreadContext *) context->backend->state;

  return interceptor_ctx->stack->len - 1;
}

static gpointer
gum_interceptor_invocation_get_listener_thread_data (
    GumInvocationContext * context,
    gsize required_size)
{
  ListenerInvocationState * data =
      (ListenerInvocationState *) context->backend->data;

  return interceptor_thread_context_get_listener_data (data->interceptor_ctx,
      data->entry->listener_instance, required_size);
}

static gpointer
gum_interceptor_invocation_get_listener_function_data (
    GumInvocationContext * context)
{
  return ((ListenerInvocationState *)
      context->backend->data)->entry->function_data;
}

static gpointer
gum_interceptor_invocation_get_listener_invocation_data (
    GumInvocationContext * context,
    gsize required_size)
{
  ListenerInvocationState * data;

  data = (ListenerInvocationState *) context->backend->data;

  if (required_size > GUM_MAX_LISTENER_DATA)
    return NULL;

  return data->invocation_data;
}

static gpointer
gum_interceptor_invocation_get_replacement_data (GumInvocationContext * context)
{
  return context->backend->data;
}

static const GumInvocationBackend
gum_interceptor_listener_invocation_backend =
{
  gum_interceptor_invocation_get_listener_point_cut,

  gum_interceptor_invocation_get_thread_id,
  gum_interceptor_invocation_get_depth,

  gum_interceptor_invocation_get_listener_thread_data,
  gum_interceptor_invocation_get_listener_function_data,
  gum_interceptor_invocation_get_listener_invocation_data,

  NULL,

  NULL,
  NULL
};

static const GumInvocationBackend
gum_interceptor_replacement_invocation_backend =
{
  gum_interceptor_invocation_get_replacement_point_cut,

  gum_interceptor_invocation_get_thread_id,
  gum_interceptor_invocation_get_depth,

  NULL,
  NULL,
  NULL,

  gum_interceptor_invocation_get_replacement_data,

  NULL,
  NULL
};

static InterceptorThreadContext *
interceptor_thread_context_new (void)
{
  InterceptorThreadContext * context;

  context = g_slice_new0 (InterceptorThreadContext);

  gum_memcpy (&context->listener_backend,
      &gum_interceptor_listener_invocation_backend,
      sizeof (GumInvocationBackend));
  gum_memcpy (&context->replacement_backend,
      &gum_interceptor_replacement_invocation_backend,
      sizeof (GumInvocationBackend));
  context->listener_backend.state = context;
  context->replacement_backend.state = context;

  context->ignore_level = 0;

  context->stack = g_array_sized_new (FALSE, TRUE,
      sizeof (GumInvocationStackEntry), GUM_MAX_CALL_DEPTH);

  context->listener_data_slots = g_array_sized_new (FALSE, TRUE,
      sizeof (ListenerDataSlot), GUM_MAX_LISTENERS_PER_FUNCTION);

  return context;
}

static void
interceptor_thread_context_destroy (InterceptorThreadContext * context)
{
  GumInvocationStack * stack = context->stack;
  guint i;

  g_array_free (context->listener_data_slots, TRUE);

  for (i = 0; i != stack->len; i++)
  {
    gum_invocation_stack_entry_release_trampoline (
        &g_array_index (stack, GumInvocationStackEntry, i));
  }

  g_array_free (stack, TRUE);

  g_slice_free (InterceptorThreadContext, context);
}

static gpointer
interceptor_thread_context_get_listener_data (InterceptorThreadContext * self,
                                              GumInvocationListener * listener,
                                              gsize required_size)
{
  guint i;
  ListenerDataSlot * available_slot = NULL;

  if (required_size > GUM_MAX_LISTENER_DATA)
    return NULL;

  for (i = 0; i != self->listener_data_slots->len; i++)
  {
    ListenerDataSlot * slot;

    slot = &g_array_index (self->listener_data_slots, ListenerDataSlot, i);
    if (slot->owner == listener)
      return slot->data;
    else if (slot->owner == NULL)
      available_slot = slot;
  }

  if (available_slot == NULL)
  {
    g_array_set_size (self->listener_data_slots,
        self->listener_data_slots->len + 1);
    available_slot = &g_array_index (self->listener_data_slots,
        ListenerDataSlot, self->listener_data_slots->len - 1);
  }
  else
  {
    gum_memset (available_slot->data, 0, sizeof (available_slot->data));
  }

  available_slot->owner = listener;

  return available_slot->data;
}

static void
interceptor_thread_context_forget_listener_data (
    InterceptorThreadContext * self,
    GumInvocationListener * listener)
{
  guint i;

  for (i = 0; i != self->listener_data_slots->len; i++)
  {
    ListenerDataSlot * slot;

    slot = &g_array_index (self->listener_data_slots, ListenerDataSlot, i);
    if (slot->owner == listener)
    {
      slot->owner = NULL;
      return;
    }
  }
}

static GumInvocationStackEntry *
gum_invocation_stack_push (GumInvocationStack * stack,
                           GumFunctionContext * function_ctx,
                           gpointer caller_ret_addr,
                           gpointer stack_address,
                           gboolean only_invoke_unignorable_listeners)
{
  GumInvocationStackEntry * entry;
  GumInvocationContext * ctx;

  g_array_set_size (stack, stack->len + 1);
  entry = (GumInvocationStackEntry *)
      &g_array_index (stack, GumInvocationStackEntry, stack->len - 1);
  entry->function_ctx = function_ctx;
  entry->caller_ret_addr = caller_ret_addr;
  entry->stack_address = stack_address;
  entry->only_invoke_unignorable_listeners = only_invoke_unignorable_listeners;

  ctx = &entry->invocation_context;
  ctx->function = gum_sign_code_pointer (function_ctx->function_address);

  ctx->backend = NULL;

  return entry;
}

static gpointer
gum_invocation_stack_pop (GumInvocationStack * stack)
{
  GumInvocationStackEntry * entry;
  gpointer caller_ret_addr;

  entry = (GumInvocationStackEntry *)
      &g_array_index (stack, GumInvocationStackEntry, stack->len - 1);
  caller_ret_addr = entry->caller_ret_addr;
  g_array_set_size (stack, stack->len - 1);

  return caller_ret_addr;
}

static void
gum_invocation_stack_reap_unwound (GumInvocationStack * stack,
                                   gpointer live_stack_address)
{
  while (stack->len != 0)
  {
    GumInvocationStackEntry * entry;

    entry = (GumInvocationStackEntry *)
        &g_array_index (stack, GumInvocationStackEntry, stack->len - 1);
    if (!gum_invocation_stack_entry_was_unwound_past (entry,
        live_stack_address))
      break;

    gum_invocation_stack_entry_release_trampoline (entry);
    g_array_set_size (stack, stack->len - 1);
  }
}

static void
gum_invocation_stack_reap_unwound_above (GumInvocationStack * stack,
                                         GumFunctionContext * returning_ctx)
{
  /*
   * Reap entries sitting above the frame we are about to return from, leaving
   * that frame on top. Calls nest last-in-first-out, and entries that don't
   * trap on leave are popped right away on enter, so any entry still stacked
   * above our frame belongs to a deeper call that was unwound past by a C++
   * exception or longjmp(), skipping its on-leave trampoline.
   *
   * We cannot lean on the leave-time stack pointer the way the on-enter path
   * does: a callee-clean calling convention such as x86 stdcall pops the
   * arguments on return, so the leave-time stack pointer sits above our own
   * recorded stack address, and a frame-pointer-omitting caller and callee
   * may even share one. Matching on the returning function context sidesteps
   * both pitfalls.
   */
  while (stack->len != 0)
  {
    GumInvocationStackEntry * entry;

    entry = (GumInvocationStackEntry *)
        &g_array_index (stack, GumInvocationStackEntry, stack->len - 1);
    if (entry->function_ctx == returning_ctx)
      break;

    gum_invocation_stack_entry_release_trampoline (entry);
    g_array_set_size (stack, stack->len - 1);
  }
}

static void
gum_invocation_stack_entry_snapshot_cpu_context (
    GumInvocationStackEntry * entry,
    const GumCpuContext * cpu_context)
{
  entry->cpu_context = *cpu_context;
#ifdef GUM_CPU_CONTEXT_HAS_OUT_OF_LINE_VECTORS
  gum_memcpy (entry->cpu_context_vectors, cpu_context->xmm,
      sizeof (entry->cpu_context_vectors));
  entry->cpu_context.xmm = entry->cpu_context_vectors;
#endif
}

static gboolean
gum_invocation_stack_entry_was_unwound_past (
    const GumInvocationStackEntry * entry,
    gpointer live_stack_address)
{
  return (guint8 *) entry->stack_address < (guint8 *) live_stack_address;
}

static void
gum_invocation_stack_entry_release_trampoline (
    const GumInvocationStackEntry * entry)
{
  g_atomic_int_dec_and_test (&entry->function_ctx->trampoline_usage_counter);
}

static GumInvocationStackEntry *
gum_invocation_stack_peek_top (GumInvocationStack * stack)
{
  if (stack->len == 0)
    return NULL;

  return &g_array_index (stack, GumInvocationStackEntry, stack->len - 1);
}

static gpointer
gum_interceptor_resolve (GumInterceptor * self,
                         gpointer address)
{
  address = gum_strip_code_pointer (address);

  if (!gum_interceptor_has (self, address))
  {
    const gsize max_redirect_size = 16;
    gpointer target;

    gum_ensure_code_readable (address, max_redirect_size);

    /* Avoid following grafted branches. */
    if (gum_process_get_code_signing_policy () == GUM_CODE_SIGNING_REQUIRED)
      return address;

    target = _gum_interceptor_backend_resolve_redirect (self->backend,
        address);
    if (target != NULL)
      return gum_interceptor_resolve (self, target);
  }

  return address;
}

static gboolean
gum_interceptor_has (GumInterceptor * self,
                     gpointer function_address)
{
  return g_hash_table_lookup (self->function_by_address,
      function_address) != NULL;
}

static gpointer
gum_page_address_from_pointer (gpointer ptr)
{
  return GSIZE_TO_POINTER (
      GPOINTER_TO_SIZE (ptr) & ~((gsize) gum_query_page_size () - 1));
}

static gint
gum_page_address_compare (gconstpointer * a,
                          gconstpointer * b)
{
  gssize diff = (gssize) GPOINTER_TO_SIZE (*a) - (gssize) GPOINTER_TO_SIZE (*b);

  return diff < 0 ? -1 : (diff > 0 ? 1 : 0);
}
