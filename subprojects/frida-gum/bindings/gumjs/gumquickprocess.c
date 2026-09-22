/*
 * Copyright (C) 2020-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2020-2023 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2023 Grant Douglas <me@hexplo.it>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumquickprocess.h"

#include "gumquickmacros.h"
#ifdef G_OS_NONE
# include "gum/gumbarebone.h"
#endif

#ifdef HAVE_DARWIN
# include <gum/gumdarwin.h>
#endif

#if defined (HAVE_I386)
# if GLIB_SIZEOF_VOID_P == 4
#  define GUM_SCRIPT_ARCH "ia32"
# else
#  define GUM_SCRIPT_ARCH "x64"
# endif
#elif defined (HAVE_ARM)
# define GUM_SCRIPT_ARCH "arm"
#elif defined (HAVE_ARM64)
# define GUM_SCRIPT_ARCH "arm64"
#elif defined (HAVE_MIPS)
# define GUM_SCRIPT_ARCH "mips"
#endif

#if defined (HAVE_LINUX)
# define GUM_SCRIPT_PLATFORM "linux"
#elif defined (HAVE_DARWIN)
# define GUM_SCRIPT_PLATFORM "darwin"
#elif defined (HAVE_WINDOWS)
# define GUM_SCRIPT_PLATFORM "windows"
#elif defined (HAVE_FREEBSD)
# define GUM_SCRIPT_PLATFORM "freebsd"
#elif defined (HAVE_QNX)
# define GUM_SCRIPT_PLATFORM "qnx"
#elif defined (G_OS_NONE)
# define GUM_SCRIPT_PLATFORM gum_barebone_query_platform ()
#endif

typedef struct _GumQuickEnumerateContext GumQuickEnumerateContext;
typedef struct _GumQuickThreadObserver GumQuickThreadObserver;
typedef struct _GumQuickRunOnThreadContext GumQuickRunOnThreadContext;
typedef struct _GumQuickModuleObserver GumQuickModuleObserver;
typedef struct _GumQuickFindRangeByAddressContext
    GumQuickFindRangeByAddressContext;

struct _GumQuickExceptionHandler
{
  JSValue callback;
  GumQuickCore * core;
};

struct _GumQuickEnumerateContext
{
  JSValue elements;
  guint n;

  JSContext * ctx;
  GumQuickProcess * parent;
};

struct _GumQuickThreadObserver
{
  gint ref_count;

  JSValue wrapper;

  JSValue on_added;
  JSValue on_removed;
  JSValue on_renamed;

  gulong added_handler;
  gulong removed_handler;
  gulong renamed_handler;

  GumQuickProcess * parent;
};

struct _GumQuickRunOnThreadContext
{
  JSValue user_func;
  GumQuickCore * core;
};

struct _GumQuickModuleObserver
{
  gint ref_count;

  JSValue wrapper;

  JSValue on_added;
  JSValue on_removed;

  gulong added_handler;
  gulong removed_handler;

  GumQuickProcess * parent;
};

struct _GumQuickFindRangeByAddressContext
{
  GumAddress address;
  JSValue result;

  JSContext * ctx;
  GumQuickCore * core;
};

static void gumjs_free_main_module_value (GumQuickProcess * self);
GUMJS_DECLARE_GETTER (gumjs_process_get_main_module)
GUMJS_DECLARE_FUNCTION (gumjs_process_get_current_dir)
GUMJS_DECLARE_FUNCTION (gumjs_process_get_home_dir)
GUMJS_DECLARE_FUNCTION (gumjs_process_get_tmp_dir)
GUMJS_DECLARE_FUNCTION (gumjs_process_is_debugger_attached)
GUMJS_DECLARE_FUNCTION (gumjs_process_get_current_thread_id)
GUMJS_DECLARE_FUNCTION (gumjs_process_find_thread_by_id)
GUMJS_DECLARE_FUNCTION (gumjs_process_enumerate_threads)
static gboolean gum_collect_thread (const GumThreadDetails * details,
    GPtrArray * threads);
GUMJS_DECLARE_FUNCTION (gumjs_process_attach_thread_observer)
static GumQuickThreadObserver * gum_quick_thread_observer_ref (
    GumQuickThreadObserver * observer);
static void gum_quick_thread_observer_unref (GumQuickThreadObserver * observer);
static void gum_quick_thread_observer_destroy (GumQuickThreadObserver * self);
static gboolean gum_emit_existing_thread (const GumThreadDetails * thread,
    GumQuickThreadObserver * observer);
static void gum_emit_added_thread (GumThreadRegistry * registry,
    const GumThreadDetails * thread, GumQuickThreadObserver * observer);
static void gum_emit_removed_thread (GumThreadRegistry * registry,
    const GumThreadDetails * thread, GumQuickThreadObserver * observer);
static void gum_emit_renamed_thread (GumThreadRegistry * registry,
    const GumThreadDetails * thread, const gchar * previous_name,
    GumQuickThreadObserver * observer);
static void gum_quick_thread_observer_invoke (GumQuickThreadObserver * self,
    JSValue callback, const GumThreadDetails * thread, guint n_extra_args, ...);
GUMJS_DECLARE_FUNCTION (gumjs_process_run_on_thread)
static void gum_quick_run_on_thread_context_free (
    GumQuickRunOnThreadContext * rc);
static void gum_do_call_on_thread (const GumCpuContext * cpu_context,
    gpointer user_data);
static void gum_quick_process_maybe_start_stalker_gc_timer (
    GumQuickProcess * self, GumQuickScope * scope);
static gboolean gum_quick_process_on_stalker_gc_timer_tick (
    GumQuickProcess * self);
GUMJS_DECLARE_FUNCTION (gumjs_process_find_module_by_name)
GUMJS_DECLARE_FUNCTION (gumjs_process_find_module_by_address)
GUMJS_DECLARE_FUNCTION (gumjs_process_enumerate_modules)
static gboolean gum_emit_module (GumModule * module,
    GumQuickEnumerateContext * ec);
GUMJS_DECLARE_FUNCTION (gumjs_process_attach_module_observer)
static GumQuickModuleObserver * gum_quick_module_observer_ref (
    GumQuickModuleObserver * observer);
static void gum_quick_module_observer_unref (GumQuickModuleObserver * observer);
static void gum_quick_module_observer_destroy (GumQuickModuleObserver * self);
static gboolean gum_emit_existing_module (GumModule * module,
    GumQuickModuleObserver * observer);
static void gum_emit_added_module (GumModuleRegistry * registry,
    GumModule * module, GumQuickModuleObserver * observer);
static void gum_emit_removed_module (GumModuleRegistry * registry,
    GumModule * module, GumQuickModuleObserver * observer);
static void gum_quick_module_observer_invoke (GumQuickModuleObserver * self,
    JSValue callback, GumModule * module);
GUMJS_DECLARE_FUNCTION (gumjs_process_find_range_by_address)
static gboolean gum_store_range_if_containing_address (
    const GumRangeDetails * details, GumQuickFindRangeByAddressContext * fc);
GUMJS_DECLARE_FUNCTION (gumjs_process_find_function_range)
GUMJS_DECLARE_FUNCTION (gumjs_process_enumerate_ranges)
static gboolean gum_emit_range (const GumRangeDetails * details,
    GumQuickEnumerateContext * ec);
GUMJS_DECLARE_FUNCTION (gumjs_process_enumerate_system_ranges)
GUMJS_DECLARE_FUNCTION (gumjs_process_enumerate_malloc_ranges)
GUMJS_DECLARE_FUNCTION (gumjs_process_set_exception_handler)

static void gum_quick_enumerate_context_begin (GumQuickEnumerateContext * ec,
    GumQuickCore * core);
static JSValue gum_quick_enumerate_context_end (GumQuickEnumerateContext * ec);
static gboolean gum_quick_enumerate_context_collect (
    GumQuickEnumerateContext * ec, JSValue element);

static GumQuickExceptionHandler * gum_quick_exception_handler_new (
    JSValue callback, GumQuickCore * core);
static void gum_quick_exception_handler_free (
    GumQuickExceptionHandler * handler);
static gboolean gum_quick_exception_handler_on_exception (
    GumExceptionDetails * details, GumQuickExceptionHandler * handler);

GUMJS_DECLARE_FUNCTION (gumjs_thread_observer_detach)

GUMJS_DECLARE_FUNCTION (gumjs_module_observer_detach)

static const JSCFunctionListEntry gumjs_process_entries[] =
{
  JS_PROP_STRING_DEF ("arch", GUM_SCRIPT_ARCH, JS_PROP_C_W_E),
  JS_PROP_INT32_DEF ("pointerSize", GLIB_SIZEOF_VOID_P, JS_PROP_C_W_E),
  JS_CGETSET_DEF ("mainModule", gumjs_process_get_main_module, NULL),
  JS_CFUNC_DEF ("getCurrentDir", 0, gumjs_process_get_current_dir),
  JS_CFUNC_DEF ("getHomeDir", 0, gumjs_process_get_home_dir),
  JS_CFUNC_DEF ("getTmpDir", 0, gumjs_process_get_tmp_dir),
  JS_CFUNC_DEF ("isDebuggerAttached", 0, gumjs_process_is_debugger_attached),
  JS_CFUNC_DEF ("getCurrentThreadId", 0, gumjs_process_get_current_thread_id),
  JS_CFUNC_DEF ("findThreadById", 0, gumjs_process_find_thread_by_id),
  JS_CFUNC_DEF ("enumerateThreads", 0, gumjs_process_enumerate_threads),
  JS_CFUNC_DEF ("attachThreadObserver", 0,
      gumjs_process_attach_thread_observer),
  JS_CFUNC_DEF ("_runOnThread", 0, gumjs_process_run_on_thread),
  JS_CFUNC_DEF ("findModuleByName", 0, gumjs_process_find_module_by_name),
  JS_CFUNC_DEF ("findModuleByAddress", 0, gumjs_process_find_module_by_address),
  JS_CFUNC_DEF ("enumerateModules", 0, gumjs_process_enumerate_modules),
  JS_CFUNC_DEF ("attachModuleObserver", 0,
      gumjs_process_attach_module_observer),
  JS_CFUNC_DEF ("findRangeByAddress", 0, gumjs_process_find_range_by_address),
  JS_CFUNC_DEF ("findFunctionRange", 0, gumjs_process_find_function_range),
  JS_CFUNC_DEF ("_enumerateRanges", 0, gumjs_process_enumerate_ranges),
  JS_CFUNC_DEF ("enumerateSystemRanges", 0,
      gumjs_process_enumerate_system_ranges),
  JS_CFUNC_DEF ("enumerateMallocRanges", 0,
      gumjs_process_enumerate_malloc_ranges),
  JS_CFUNC_DEF ("setExceptionHandler", 0, gumjs_process_set_exception_handler),
};

static const JSClassDef gumjs_thread_observer_def =
{
  .class_name = "ThreadObserver",
};

static const JSCFunctionListEntry gumjs_thread_observer_entries[] =
{
  JS_CFUNC_DEF ("detach", 0, gumjs_thread_observer_detach),
};

static const JSClassDef gumjs_module_observer_def =
{
  .class_name = "ModuleObserver",
};

static const JSCFunctionListEntry gumjs_module_observer_entries[] =
{
  JS_CFUNC_DEF ("detach", 0, gumjs_module_observer_detach),
};

void
_gum_quick_process_init (GumQuickProcess * self,
                         JSValue ns,
                         GumQuickModule * module,
                         GumQuickThread * thread,
                         GumQuickCore * core)
{
  JSContext * ctx = core->ctx;
  JSValue obj, proto;

  self->module = module;
  self->thread = thread;
  self->core = core;

  self->thread_observers = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_quick_thread_observer_destroy);
  self->module_observers = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_quick_module_observer_destroy);

  self->main_module_value = JS_UNINITIALIZED;

  self->stalker = NULL;
  self->stalker_gc_timer = NULL;

  _gum_quick_core_store_module_data (core, "process", self);

  obj = JS_NewObject (ctx);
  JS_SetPropertyFunctionList (ctx, obj, gumjs_process_entries,
      G_N_ELEMENTS (gumjs_process_entries));
  JS_DefinePropertyValueStr (ctx, obj, "platform",
      JS_NewString (ctx, GUM_SCRIPT_PLATFORM), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr (ctx, obj, "id",
      JS_NewInt32 (ctx, gum_process_get_id ()), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr (ctx, obj, "pageSize",
      JS_NewInt32 (ctx, gum_query_page_size ()), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr (ctx, obj, "codeSigningPolicy",
      JS_NewString (ctx, gum_code_signing_policy_to_string (
          gum_process_get_code_signing_policy ())),
      JS_PROP_C_W_E);
  JS_DefinePropertyValueStr (ctx, ns, "Process", obj, JS_PROP_C_W_E);

  _gum_quick_create_class (ctx, &gumjs_thread_observer_def, core,
      &self->thread_observer_class, &proto);
  JS_SetPropertyFunctionList (ctx, proto, gumjs_thread_observer_entries,
      G_N_ELEMENTS (gumjs_thread_observer_entries));

  _gum_quick_create_class (ctx, &gumjs_module_observer_def, core,
      &self->module_observer_class, &proto);
  JS_SetPropertyFunctionList (ctx, proto, gumjs_module_observer_entries,
      G_N_ELEMENTS (gumjs_module_observer_entries));
}

void
_gum_quick_process_flush (GumQuickProcess * self)
{
  g_clear_pointer (&self->exception_handler, gum_quick_exception_handler_free);

  gumjs_free_main_module_value (self);

  g_hash_table_remove_all (self->module_observers);
  g_hash_table_remove_all (self->thread_observers);
}

void
_gum_quick_process_dispose (GumQuickProcess * self)
{
  g_assert (self->stalker_gc_timer == NULL);

  g_clear_pointer (&self->exception_handler, gum_quick_exception_handler_free);
  gumjs_free_main_module_value (self);
}

static void
gumjs_free_main_module_value (GumQuickProcess * self)
{
  if (JS_IsUninitialized (self->main_module_value))
    return;

  JS_FreeValue (self->core->ctx, self->main_module_value);
  self->main_module_value = JS_UNINITIALIZED;
}

void
_gum_quick_process_finalize (GumQuickProcess * self)
{
  g_clear_object (&self->stalker);

  g_clear_pointer (&self->module_observers, g_hash_table_unref);
  g_clear_pointer (&self->thread_observers, g_hash_table_unref);
}

static GumQuickProcess *
gumjs_get_parent_module (GumQuickCore * core)
{
  return _gum_quick_core_load_module_data (core, "process");
}

GUMJS_DEFINE_GETTER (gumjs_process_get_main_module)
{
  GumQuickProcess * self;

  self = gumjs_get_parent_module (core);

  if (JS_IsUninitialized (self->main_module_value))
  {
    self->main_module_value = _gum_quick_module_new_from_handle (ctx,
        gum_process_get_main_module (), self->module);
  }

  return JS_DupValue (ctx, self->main_module_value);
}

GUMJS_DEFINE_FUNCTION (gumjs_process_get_current_dir)
{
  JSValue result;
  gchar * dir_opsys, * dir_utf8;

  dir_opsys = g_get_current_dir ();
  dir_utf8 = g_filename_display_name (dir_opsys);
  result = JS_NewString (ctx, dir_utf8);
  g_free (dir_utf8);
  g_free (dir_opsys);

  return result;
}

GUMJS_DEFINE_FUNCTION (gumjs_process_get_home_dir)
{
  JSValue result;
  gchar * dir;

  dir = g_filename_display_name (g_get_home_dir ());
  result = JS_NewString (ctx, dir);
  g_free (dir);

  return result;
}

GUMJS_DEFINE_FUNCTION (gumjs_process_get_tmp_dir)
{
  JSValue result;
  gchar * dir;

  dir = g_filename_display_name (g_get_tmp_dir ());
  result = JS_NewString (ctx, dir);
  g_free (dir);

  return result;
}

GUMJS_DEFINE_FUNCTION (gumjs_process_is_debugger_attached)
{
  return JS_NewBool (ctx, gum_process_is_debugger_attached ());
}

GUMJS_DEFINE_FUNCTION (gumjs_process_get_current_thread_id)
{
  return JS_NewInt64 (ctx, gum_process_get_current_thread_id ());
}

GUMJS_DEFINE_FUNCTION (gumjs_process_find_thread_by_id)
{
  GumThreadId thread_id;
  GumQuickScope scope = GUM_QUICK_SCOPE_INIT (core);
  GumThreadDetails * details;
  JSValue result;

  if (!_gum_quick_args_parse (args, "Z", &thread_id))
    return JS_EXCEPTION;

  _gum_quick_scope_suspend (&scope);

  details = gum_process_find_thread_by_id (thread_id, GUM_THREAD_FLAGS_ALL);

  _gum_quick_scope_resume (&scope);

  if (details == NULL)
    return JS_NULL;

  result = _gum_quick_thread_new (ctx, details,
      gumjs_get_parent_module (core)->thread);

  gum_thread_details_free (details);

  return result;
}

GUMJS_DEFINE_FUNCTION (gumjs_process_enumerate_threads)
{
  GumQuickScope scope = GUM_QUICK_SCOPE_INIT (core);
  GPtrArray * threads;
  GumQuickEnumerateContext ec;
  guint i;

  threads = g_ptr_array_new_with_free_func (
      (GDestroyNotify) gum_thread_details_free);

  _gum_quick_scope_suspend (&scope);

  gum_process_enumerate_threads ((GumFoundThreadFunc) gum_collect_thread,
      threads, GUM_THREAD_FLAGS_ALL);

  _gum_quick_scope_resume (&scope);

  gum_quick_enumerate_context_begin (&ec, core);

  for (i = 0; i != threads->len; i++)
  {
    const GumThreadDetails * details = g_ptr_array_index (threads, i);

    if (!gum_quick_enumerate_context_collect (&ec,
        _gum_quick_thread_new (ec.ctx, details, ec.parent->thread)))
      break;
  }

  g_ptr_array_unref (threads);

  return gum_quick_enumerate_context_end (&ec);
}

static gboolean
gum_collect_thread (const GumThreadDetails * details,
                    GPtrArray * threads)
{
  g_ptr_array_add (threads, gum_thread_details_copy (details));

  return TRUE;
}

GUMJS_DEFINE_FUNCTION (gumjs_process_attach_thread_observer)
{
  JSValue cb_val = args->elements[0];
  GumQuickProcess * parent;
  JSValue on_added, on_removed, on_renamed;
  gboolean observe_added, observe_removed, observe_renamed;
  GumQuickThreadObserver * observer;
  GumQuickScope scope = GUM_QUICK_SCOPE_INIT (core);
  GumThreadRegistry * registry;

  parent = gumjs_get_parent_module (core);

  if (!_gum_quick_args_parse (args, "F{onAdded?,onRemoved?,onRenamed?}",
        &on_added, &on_removed, &on_renamed))
    return JS_EXCEPTION;

  observe_added = !JS_IsNull (on_added);
  observe_removed = !JS_IsNull (on_removed);
  observe_renamed = !JS_IsNull (on_renamed);

  if (!observe_added && !observe_removed && !observe_renamed)
    goto missing_callback;

  observer = g_slice_new (GumQuickThreadObserver);
  observer->ref_count = 1;
  observer->on_added = observe_added
      ? JS_DupValue (ctx, on_added)
      : JS_NULL;
  observer->on_removed = observe_removed
      ? JS_DupValue (ctx, on_removed)
      : JS_NULL;
  observer->on_renamed = observe_renamed
      ? JS_DupValue (ctx, on_renamed)
      : JS_NULL;
  observer->added_handler = 0;
  observer->removed_handler = 0;
  observer->renamed_handler = 0;
  observer->parent = parent;

  _gum_quick_scope_suspend (&scope);

  registry = gum_thread_registry_obtain ();

  gum_thread_registry_lock (registry);

  if (observe_added)
  {
    observer->added_handler = g_signal_connect_data (registry,
        "thread-added",
        G_CALLBACK (gum_emit_added_thread),
        gum_quick_thread_observer_ref (observer),
        (GClosureNotify) gum_quick_thread_observer_unref,
        0);
  }

  if (observe_removed)
  {
    observer->removed_handler = g_signal_connect_data (registry,
        "thread-removed",
        G_CALLBACK (gum_emit_removed_thread),
        gum_quick_thread_observer_ref (observer),
        (GClosureNotify) gum_quick_thread_observer_unref,
        0);
  }

  if (observe_renamed)
  {
    observer->renamed_handler = g_signal_connect_data (registry,
        "thread-renamed",
        G_CALLBACK (gum_emit_renamed_thread),
        gum_quick_thread_observer_ref (observer),
        (GClosureNotify) gum_quick_thread_observer_unref,
        0);
  }

  if (observe_added)
  {
    gum_thread_registry_enumerate_threads (registry,
        (GumFoundThreadFunc) gum_emit_existing_thread, observer);
  }

  gum_thread_registry_unlock (registry);

  _gum_quick_scope_resume (&scope);

  observer->wrapper = JS_NewObjectClass (ctx, parent->thread_observer_class);
  JS_SetOpaque (observer->wrapper, observer);
  JS_DefinePropertyValue (ctx, observer->wrapper,
      GUM_QUICK_CORE_ATOM (core, resource),
      JS_DupValue (ctx, cb_val),
      0);

  g_hash_table_add (parent->thread_observers, observer);

  return JS_DupValue (ctx, observer->wrapper);

missing_callback:
  {
    _gum_quick_throw_literal (ctx, "at least one callback must be provided");
    return JS_EXCEPTION;
  }
}

static GumQuickThreadObserver *
gum_quick_thread_observer_ref (GumQuickThreadObserver * observer)
{
  g_atomic_int_inc (&observer->ref_count);

  return observer;
}

static void
gum_quick_thread_observer_unref (GumQuickThreadObserver * observer)
{
  GumQuickProcess * parent = observer->parent;
  JSContext * ctx = parent->core->ctx;
  GumQuickScope scope;

  if (!g_atomic_int_dec_and_test (&observer->ref_count))
    return;

  _gum_quick_scope_enter (&scope, parent->core);

  JS_FreeValue (ctx, observer->on_added);
  JS_FreeValue (ctx, observer->on_removed);
  JS_FreeValue (ctx, observer->on_renamed);

  JS_FreeValue (ctx, observer->wrapper);

  _gum_quick_scope_leave (&scope);

  g_slice_free (GumQuickThreadObserver, observer);
}

static void
gum_quick_thread_observer_destroy (GumQuickThreadObserver * self)
{
  GumThreadRegistry * registry;
  gulong * handlers[] = {
    &self->added_handler,
    &self->removed_handler,
    &self->renamed_handler,
  };
  guint i;

  registry = gum_thread_registry_obtain ();

  for (i = 0; i != G_N_ELEMENTS (handlers); i++)
  {
    gulong * handler = handlers[i];

    if (*handler != 0)
    {
      g_signal_handler_disconnect (registry, *handler);
      *handler = 0;
    }
  }

  JS_SetOpaque (self->wrapper, NULL);

  gum_quick_thread_observer_unref (self);
}

static void
gum_quick_process_detach_thread_observer (GumQuickProcess * self,
                                          GumQuickThreadObserver * observer)
{
  g_hash_table_remove (self->thread_observers, observer);
}

static gboolean
gum_emit_existing_thread (const GumThreadDetails * thread,
                          GumQuickThreadObserver * observer)
{
  gum_quick_thread_observer_invoke (observer, observer->on_added, thread, 0);

  return TRUE;
}

static void
gum_emit_added_thread (GumThreadRegistry * registry,
                       const GumThreadDetails * thread,
                       GumQuickThreadObserver * observer)
{
  gum_quick_thread_observer_invoke (observer, observer->on_added, thread, 0);
}

static void
gum_emit_removed_thread (GumThreadRegistry * registry,
                         const GumThreadDetails * thread,
                         GumQuickThreadObserver * observer)
{
  gum_quick_thread_observer_invoke (observer, observer->on_removed, thread, 0);
}

static void
gum_emit_renamed_thread (GumThreadRegistry * registry,
                         const GumThreadDetails * thread,
                         const gchar * previous_name,
                         GumQuickThreadObserver * observer)
{
  gum_quick_thread_observer_invoke (observer, observer->on_renamed, thread, 1,
      G_TYPE_STRING, previous_name);
}

static void
gum_quick_thread_observer_invoke (GumQuickThreadObserver * self,
                                  JSValue callback,
                                  const GumThreadDetails * thread,
                                  guint n_extra_args,
                                  ...)
{
  GumQuickProcess * parent = self->parent;
  JSContext * ctx = parent->core->ctx;
  GumQuickScope scope;
  JSValue thread_val;
  guint argc;
  JSValue * argv;
  va_list args;
  guint i;

  _gum_quick_scope_enter (&scope, parent->core);

  thread_val = _gum_quick_thread_new (ctx, thread, parent->thread);

  argc = 1 + n_extra_args;

  argv = g_newa (JSValue, argc);
  argv[0] = thread_val;

  va_start (args, n_extra_args);
  for (i = 0; i != n_extra_args; i++)
  {
    GType type;
    JSValue val;

    type = va_arg (args, GType);

    if (type == G_TYPE_STRING)
    {
      const gchar * str = va_arg (args, gchar *);
      val = (str != NULL) ? JS_NewString (ctx, str) : JS_NULL;
    }
    else
    {
      g_assert_not_reached ();
    }

    argv[1 + i] = val;
  }
  va_end (args);

  _gum_quick_scope_call_void (&scope, callback, JS_UNDEFINED, argc, argv);

  for (i = 0; i != 1 + n_extra_args; i++)
    JS_FreeValue (ctx, argv[i]);

  _gum_quick_scope_leave (&scope);
}

GUMJS_DEFINE_FUNCTION (gumjs_process_run_on_thread)
{
  GumQuickProcess * self;
  GumThreadId thread_id;
  JSValue user_func;
  GumQuickScope scope = GUM_QUICK_SCOPE_INIT (core);
  GumQuickRunOnThreadContext * rc;
  gboolean success;

  self = gumjs_get_parent_module (core);

  if (!_gum_quick_args_parse (args, "ZF", &thread_id, &user_func))
    return JS_EXCEPTION;

  if (self->stalker == NULL)
    self->stalker = gum_stalker_new ();

  rc = g_slice_new (GumQuickRunOnThreadContext);
  rc->user_func = JS_DupValue (core->ctx, user_func);
  rc->core = core;

  _gum_quick_scope_suspend (&scope);

  success = gum_stalker_run_on_thread (self->stalker, thread_id,
      gum_do_call_on_thread, rc,
      (GDestroyNotify) gum_quick_run_on_thread_context_free);

  _gum_quick_scope_resume (&scope);

  gum_quick_process_maybe_start_stalker_gc_timer (self, &scope);

  if (!success)
    goto run_failed;

  return JS_UNDEFINED;

run_failed:
  {
    _gum_quick_throw_literal (ctx, "failed to run on thread");

    return JS_EXCEPTION;
  }
}

static void
gum_quick_run_on_thread_context_free (GumQuickRunOnThreadContext * rc)
{
  GumQuickCore * core = rc->core;
  GumQuickScope scope;

  _gum_quick_scope_enter (&scope, core);
  JS_FreeValue (core->ctx, rc->user_func);
  _gum_quick_scope_leave (&scope);

  g_slice_free (GumQuickRunOnThreadContext, rc);
}

static void
gum_do_call_on_thread (const GumCpuContext * cpu_context,
                       gpointer user_data)
{
  GumQuickRunOnThreadContext * rc = user_data;
  GumQuickScope scope;

  _gum_quick_scope_enter (&scope, rc->core);
  _gum_quick_scope_call (&scope, rc->user_func, JS_UNDEFINED, 0, NULL);
  _gum_quick_scope_leave (&scope);
}

static void
gum_quick_process_maybe_start_stalker_gc_timer (GumQuickProcess * self,
                                                GumQuickScope * scope)
{
  GumQuickCore * core = self->core;
  GSource * source;

  if (self->stalker_gc_timer != NULL)
    return;

  if (!gum_stalker_garbage_collect (self->stalker))
    return;

  source = g_timeout_source_new (10);
  g_source_set_callback (source,
      (GSourceFunc) gum_quick_process_on_stalker_gc_timer_tick, self, NULL);
  self->stalker_gc_timer = source;

  _gum_quick_core_pin (core);
  _gum_quick_scope_suspend (scope);

  g_source_attach (source,
      gum_script_scheduler_get_js_context (core->scheduler));
  g_source_unref (source);

  _gum_quick_scope_resume (scope);
}

static gboolean
gum_quick_process_on_stalker_gc_timer_tick (GumQuickProcess * self)
{
  gboolean pending_garbage;

  pending_garbage = gum_stalker_garbage_collect (self->stalker);
  if (!pending_garbage)
  {
    GumQuickCore * core = self->core;
    GumQuickScope scope;

    _gum_quick_scope_enter (&scope, core);

    _gum_quick_core_unpin (core);
    self->stalker_gc_timer = NULL;

    _gum_quick_scope_leave (&scope);
  }

  return pending_garbage ? G_SOURCE_CONTINUE : G_SOURCE_REMOVE;
}

GUMJS_DEFINE_FUNCTION (gumjs_process_find_module_by_name)
{
  const gchar * name;
  GumModule * module;

  if (!_gum_quick_args_parse (args, "s", &name))
    return JS_EXCEPTION;

  module = gum_process_find_module_by_name (name);
  if (module == NULL)
    return JS_NULL;

  return _gum_quick_module_new_take_handle (ctx, module,
      gumjs_get_parent_module (core)->module);
}

GUMJS_DEFINE_FUNCTION (gumjs_process_find_module_by_address)
{
  gpointer address;
  GumModule * module;

  if (!_gum_quick_args_parse (args, "p", &address))
    return JS_EXCEPTION;

  module = gum_process_find_module_by_address (GUM_ADDRESS (address));
  if (module == NULL)
    return JS_NULL;

  return _gum_quick_module_new_take_handle (ctx, module,
      gumjs_get_parent_module (core)->module);
}

GUMJS_DEFINE_FUNCTION (gumjs_process_enumerate_modules)
{
  GumQuickEnumerateContext ec;

  gum_quick_enumerate_context_begin (&ec, core);

  gum_process_enumerate_modules ((GumFoundModuleFunc) gum_emit_module, &ec);

  return gum_quick_enumerate_context_end (&ec);
}

static gboolean
gum_emit_module (GumModule * module,
                 GumQuickEnumerateContext * ec)
{
  return gum_quick_enumerate_context_collect (ec,
      _gum_quick_module_new_from_handle (ec->ctx, module, ec->parent->module));
}

GUMJS_DEFINE_FUNCTION (gumjs_process_attach_module_observer)
{
  JSValue cb_val = args->elements[0];
  GumQuickProcess * parent;
  JSValue on_added, on_removed;
  gboolean observe_added, observe_removed;
  GumQuickModuleObserver * observer;
  GumQuickScope scope = GUM_QUICK_SCOPE_INIT (core);
  GumModuleRegistry * registry;

  parent = gumjs_get_parent_module (core);

  if (!_gum_quick_args_parse (args, "F{onAdded?,onRemoved?}", &on_added,
        &on_removed))
    return JS_EXCEPTION;

  observe_added = !JS_IsNull (on_added);
  observe_removed = !JS_IsNull (on_removed);

  if (!observe_added && !observe_removed)
    goto missing_callback;

  observer = g_slice_new (GumQuickModuleObserver);
  observer->ref_count = 1;
  observer->on_added = observe_added
      ? JS_DupValue (ctx, on_added)
      : JS_NULL;
  observer->on_removed = observe_removed
      ? JS_DupValue (ctx, on_removed)
      : JS_NULL;
  observer->added_handler = 0;
  observer->removed_handler = 0;
  observer->parent = parent;

  _gum_quick_scope_suspend (&scope);

  registry = gum_module_registry_obtain ();

  gum_module_registry_lock (registry);

  if (observe_added)
  {
    observer->added_handler = g_signal_connect_data (registry,
        "module-added",
        G_CALLBACK (gum_emit_added_module),
        gum_quick_module_observer_ref (observer),
        (GClosureNotify) gum_quick_module_observer_unref,
        0);
  }

  if (observe_removed)
  {
    observer->removed_handler = g_signal_connect_data (registry,
        "module-removed",
        G_CALLBACK (gum_emit_removed_module),
        gum_quick_module_observer_ref (observer),
        (GClosureNotify) gum_quick_module_observer_unref,
        0);
  }

  if (observe_added)
  {
    gum_module_registry_enumerate_modules (registry,
        (GumFoundModuleFunc) gum_emit_existing_module, observer);
  }

  gum_module_registry_unlock (registry);

  _gum_quick_scope_resume (&scope);

  observer->wrapper = JS_NewObjectClass (ctx, parent->module_observer_class);
  JS_SetOpaque (observer->wrapper, observer);
  JS_DefinePropertyValue (ctx, observer->wrapper,
      GUM_QUICK_CORE_ATOM (core, resource),
      JS_DupValue (ctx, cb_val),
      0);

  g_hash_table_add (parent->module_observers, observer);

  return JS_DupValue (ctx, observer->wrapper);

missing_callback:
  {
    _gum_quick_throw_literal (ctx, "at least one callback must be provided");
    return JS_EXCEPTION;
  }
}

static GumQuickModuleObserver *
gum_quick_module_observer_ref (GumQuickModuleObserver * observer)
{
  g_atomic_int_inc (&observer->ref_count);

  return observer;
}

static void
gum_quick_module_observer_unref (GumQuickModuleObserver * observer)
{
  GumQuickProcess * parent = observer->parent;
  JSContext * ctx = parent->core->ctx;
  GumQuickScope scope;

  if (!g_atomic_int_dec_and_test (&observer->ref_count))
    return;

  _gum_quick_scope_enter (&scope, parent->core);

  JS_FreeValue (ctx, observer->on_added);
  JS_FreeValue (ctx, observer->on_removed);

  JS_FreeValue (ctx, observer->wrapper);

  _gum_quick_scope_leave (&scope);

  g_slice_free (GumQuickModuleObserver, observer);
}

static void
gum_quick_module_observer_destroy (GumQuickModuleObserver * self)
{
  GumModuleRegistry * registry;
  gulong * handlers[] = {
    &self->added_handler,
    &self->removed_handler,
  };
  guint i;

  registry = gum_module_registry_obtain ();

  for (i = 0; i != G_N_ELEMENTS (handlers); i++)
  {
    gulong * handler = handlers[i];

    if (*handler != 0)
    {
      g_signal_handler_disconnect (registry, *handler);
      *handler = 0;
    }
  }

  JS_SetOpaque (self->wrapper, NULL);

  gum_quick_module_observer_unref (self);
}

static void
gum_quick_process_detach_module_observer (GumQuickProcess * self,
                                          GumQuickModuleObserver * observer)
{
  g_hash_table_remove (self->module_observers, observer);
}

static gboolean
gum_emit_existing_module (GumModule * module,
                          GumQuickModuleObserver * observer)
{
  gum_quick_module_observer_invoke (observer, observer->on_added, module);

  return TRUE;
}

static void
gum_emit_added_module (GumModuleRegistry * registry,
                       GumModule * module,
                       GumQuickModuleObserver * observer)
{
  gum_quick_module_observer_invoke (observer, observer->on_added, module);
}

static void
gum_emit_removed_module (GumModuleRegistry * registry,
                         GumModule * module,
                         GumQuickModuleObserver * observer)
{
  gum_quick_module_observer_invoke (observer, observer->on_removed, module);
}

static void
gum_quick_module_observer_invoke (GumQuickModuleObserver * self,
                                  JSValue callback,
                                  GumModule * module)
{
  GumQuickProcess * parent = self->parent;
  JSContext * ctx = parent->core->ctx;
  GumQuickScope scope;
  JSValue wrapper;

  _gum_quick_scope_enter (&scope, parent->core);

  wrapper = _gum_quick_module_new_from_handle (ctx, module, parent->module);

  _gum_quick_scope_call_void (&scope, callback, JS_UNDEFINED, 1, &wrapper);

  JS_FreeValue (ctx, wrapper);

  _gum_quick_scope_leave (&scope);
}

GUMJS_DEFINE_FUNCTION (gumjs_process_find_range_by_address)
{
  GumQuickFindRangeByAddressContext fc;
  gpointer ptr;

  if (!_gum_quick_args_parse (args, "p", &ptr))
    return JS_EXCEPTION;
  fc.address = GUM_ADDRESS (ptr);
  fc.result = JS_NULL;
  fc.ctx = ctx;
  fc.core = core;

  gum_process_enumerate_ranges (GUM_PAGE_NO_ACCESS,
      (GumFoundRangeFunc) gum_store_range_if_containing_address, &fc);

  return fc.result;
}

static gboolean
gum_store_range_if_containing_address (const GumRangeDetails * details,
                                       GumQuickFindRangeByAddressContext * fc)
{
  gboolean proceed = TRUE;

  if (GUM_MEMORY_RANGE_INCLUDES (details->range, fc->address))
  {
    fc->result = _gum_quick_range_details_new (fc->ctx, details, fc->core);

    proceed = FALSE;
  }

  return proceed;
}

GUMJS_DEFINE_FUNCTION (gumjs_process_find_function_range)
{
  gpointer ptr;
  GumMemoryRange range;

  if (!_gum_quick_args_parse (args, "p", &ptr))
    return JS_EXCEPTION;

  if (!gum_process_find_function_range (ptr, &range))
    return JS_NULL;

  return _gum_quick_memory_range_new (ctx, &range, core);
}

GUMJS_DEFINE_FUNCTION (gumjs_process_enumerate_ranges)
{
  GumPageProtection prot;
  GumQuickEnumerateContext ec;

  if (!_gum_quick_args_parse (args, "m", &prot))
    return JS_EXCEPTION;

  gum_quick_enumerate_context_begin (&ec, core);

  gum_process_enumerate_ranges (prot, (GumFoundRangeFunc) gum_emit_range, &ec);

  return gum_quick_enumerate_context_end (&ec);
}

static gboolean
gum_emit_range (const GumRangeDetails * details,
                GumQuickEnumerateContext * ec)
{
  return gum_quick_enumerate_context_collect (ec,
      _gum_quick_range_details_new (ec->ctx, details, ec->parent->core));
}

GUMJS_DEFINE_FUNCTION (gumjs_process_enumerate_system_ranges)
{
  JSValue ranges = JS_NewObject (ctx);

#ifdef HAVE_DARWIN
  {
    GumMemoryRange dsc;

    if (gum_darwin_query_shared_cache_range (mach_task_self (), &dsc))
    {
      JS_DefinePropertyValueStr (ctx, ranges, "dyldSharedCache",
          _gum_quick_memory_range_new (ctx, &dsc, args->core),
          JS_PROP_C_W_E);
    }
  }
#endif

  return ranges;
}

#if defined (HAVE_WINDOWS) || defined (HAVE_DARWIN)

static gboolean gum_emit_malloc_range (const GumMallocRangeDetails * details,
    GumQuickEnumerateContext * ec);

GUMJS_DEFINE_FUNCTION (gumjs_process_enumerate_malloc_ranges)
{
  GumQuickEnumerateContext ec;

  gum_quick_enumerate_context_begin (&ec, core);

  gum_process_enumerate_malloc_ranges (
      (GumFoundMallocRangeFunc) gum_emit_malloc_range, &ec);

  return gum_quick_enumerate_context_end (&ec);
}

static gboolean
gum_emit_malloc_range (const GumMallocRangeDetails * details,
                       GumQuickEnumerateContext * ec)
{
  JSContext * ctx = ec->ctx;
  GumQuickCore * core = ec->parent->core;
  JSValue range;

  range = JS_NewObject (ctx);

  JS_DefinePropertyValue (ctx, range,
      GUM_QUICK_CORE_ATOM (core, base),
      _gum_quick_native_pointer_new (ctx,
          GSIZE_TO_POINTER (details->range->base_address), core),
      JS_PROP_C_W_E);
  JS_DefinePropertyValue (ctx, range,
      GUM_QUICK_CORE_ATOM (core, size),
      JS_NewInt64 (ctx, details->range->size),
      JS_PROP_C_W_E);

  return gum_quick_enumerate_context_collect (ec, range);
}

#else

GUMJS_DEFINE_FUNCTION (gumjs_process_enumerate_malloc_ranges)
{
  return _gum_quick_throw (ctx,
      "not yet implemented for %s", GUM_SCRIPT_PLATFORM);
}

#endif

GUMJS_DEFINE_FUNCTION (gumjs_process_set_exception_handler)
{
  GumQuickProcess * self;
  JSValue callback;
  GumQuickExceptionHandler * new_handler, * old_handler;

  self = gumjs_get_parent_module (core);

  if (!_gum_quick_args_parse (args, "F?", &callback))
    return JS_EXCEPTION;

  new_handler = !JS_IsNull (callback)
      ? gum_quick_exception_handler_new (callback, self->core)
      : NULL;

  old_handler = self->exception_handler;
  self->exception_handler = new_handler;

  if (old_handler != NULL)
    gum_quick_exception_handler_free (old_handler);

  return JS_UNDEFINED;
}

static void
gum_quick_enumerate_context_begin (GumQuickEnumerateContext * ec,
                                   GumQuickCore * core)
{
  ec->elements = JS_NewArray (core->ctx);
  ec->n = 0;

  ec->ctx = core->ctx;
  ec->parent = gumjs_get_parent_module (core);
}

static JSValue
gum_quick_enumerate_context_end (GumQuickEnumerateContext * ec)
{
  return ec->elements;
}

static gboolean
gum_quick_enumerate_context_collect (GumQuickEnumerateContext * ec,
                                     JSValue element)
{
  JS_DefinePropertyValueUint32 (ec->ctx, ec->elements, ec->n++, element,
      JS_PROP_C_W_E);
  return TRUE;
}

static GumQuickExceptionHandler *
gum_quick_exception_handler_new (JSValue callback,
                                 GumQuickCore * core)
{
  GumQuickExceptionHandler * handler;

  handler = g_slice_new (GumQuickExceptionHandler);
  handler->callback = JS_DupValue (core->ctx, callback);
  handler->core = core;

  gum_exceptor_add (core->exceptor,
      (GumExceptionHandler) gum_quick_exception_handler_on_exception, handler);

  return handler;
}

static void
gum_quick_exception_handler_free (GumQuickExceptionHandler * handler)
{
  GumQuickCore * core = handler->core;

  gum_exceptor_remove (core->exceptor,
      (GumExceptionHandler) gum_quick_exception_handler_on_exception, handler);

  JS_FreeValue (core->ctx, handler->callback);

  g_slice_free (GumQuickExceptionHandler, handler);
}

static gboolean
gum_quick_exception_handler_on_exception (GumExceptionDetails * details,
                                          GumQuickExceptionHandler * handler)
{
  GumQuickCore * core = handler->core;
  JSContext * ctx = core->ctx;
  gboolean handled;
  GumQuickScope scope;
  JSValue d, r;
  GumQuickCpuContext * cpu_context;

  if (gum_quick_script_backend_is_scope_mutex_trapped (core->backend))
    return FALSE;

  _gum_quick_scope_enter (&scope, core);

  d = _gum_quick_exception_details_new (ctx, details, core, &cpu_context);

  r = _gum_quick_scope_call (&scope, handler->callback, JS_UNDEFINED, 1, &d);

  handled = JS_IsBool (r) && JS_VALUE_GET_BOOL (r);

  _gum_quick_cpu_context_make_read_only (cpu_context);

  JS_FreeValue (ctx, r);
  JS_FreeValue (ctx, d);

  _gum_quick_scope_leave (&scope);

  return handled;
}

GUMJS_DEFINE_FUNCTION (gumjs_thread_observer_detach)
{
  GumQuickProcess * parent;
  GumQuickThreadObserver * observer;

  parent = gumjs_get_parent_module (core);

  if (!_gum_quick_unwrap (ctx, this_val, parent->thread_observer_class,
      core, (gpointer *) &observer))
    return JS_EXCEPTION;

  if (observer != NULL)
    gum_quick_process_detach_thread_observer (parent, observer);

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_module_observer_detach)
{
  GumQuickProcess * parent;
  GumQuickModuleObserver * observer;

  parent = gumjs_get_parent_module (core);

  if (!_gum_quick_unwrap (ctx, this_val, parent->module_observer_class,
      core, (gpointer *) &observer))
    return JS_EXCEPTION;

  if (observer != NULL)
    gum_quick_process_detach_module_observer (parent, observer);

  return JS_UNDEFINED;
}
