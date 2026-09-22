/*
 * Copyright (C) 2008-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2008 Christian Berentsen <jc.berentsen@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumallocatorprobe.h"

#include "gum-init.h"
#include "guminterceptor.h"
#include "gumprocess.h"
#include "gumsymbolutil.h"

#define DEFAULT_ENABLE_COUNTERS FALSE

#define GUM_DBGCRT_UNKNOWN_BLOCK (-1)
#define GUM_DBGCRT_NORMAL_BLOCK (1)

#define GUM_DBGCRT_BLOCK_TYPE(type_bits) ((type_bits) & 0xffff)

typedef struct _FunctionContext      FunctionContext;
typedef struct _HeapHandlers         HeapHandlers;
typedef struct _ThreadContext        ThreadContext;
typedef struct _AllocThreadContext   AllocThreadContext;
typedef struct _ReallocThreadContext ReallocThreadContext;

typedef void (* HeapEnterHandler) (GumAllocatorProbe * self,
    gpointer thread_ctx, GumInvocationContext * invocation_ctx,
    gpointer user_data);
typedef void (* HeapLeaveHandler) (GumAllocatorProbe * self,
    gpointer thread_ctx, GumInvocationContext * invocation_ctx,
    gpointer user_data);

typedef gint (* GumReportBlockTypeFunc) (gpointer block);

struct _GumAllocatorProbe
{
  GObject parent;

  gboolean disposed;

  GumInterceptor * interceptor;
  GPtrArray * function_contexts;
  GumAllocationTracker * allocation_tracker;

  gboolean enable_counters;
  guint malloc_count;
  guint realloc_count;
  guint free_count;
};

enum
{
  PROP_0,
  PROP_ALLOCATION_TRACKER,
  PROP_ENABLE_COUNTERS,
  PROP_MALLOC_COUNT,
  PROP_REALLOC_COUNT,
  PROP_FREE_COUNT
};

struct _ThreadContext
{
  gboolean ignored;
  GumCpuContext cpu_context;
  gpointer function_specific_storage[4];
};

struct _HeapHandlers
{
  HeapEnterHandler enter_handler;
  HeapLeaveHandler leave_handler;
};

struct _FunctionContext
{
  HeapHandlers handlers;
  gpointer handler_data;
  ThreadContext thread_contexts[GUM_MAX_THREADS];
  volatile gint thread_context_count;
};

struct _AllocThreadContext
{
  gboolean ignored;
  GumCpuContext cpu_context;
  gsize size;
};

struct _ReallocThreadContext
{
  gboolean ignored;
  GumCpuContext cpu_context;
  gpointer old_address;
  gsize new_size;
};

struct _FreeThreadContext
{
  gboolean ignored;
  GumCpuContext cpu_context;
  gpointer address;
};

static void gum_allocator_probe_deinit (void);

static void gum_allocator_probe_listener_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_allocator_probe_dispose (GObject * object);
static void gum_allocator_probe_finalize (GObject * object);
static void gum_allocator_probe_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gum_allocator_probe_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);

static void gum_allocator_probe_apply_default_suppressions (
    GumAllocatorProbe * self);

static void gum_allocator_probe_on_enter (GumInvocationListener * listener,
    GumInvocationContext * context);
static void gum_allocator_probe_on_leave (GumInvocationListener * listener,
    GumInvocationContext * context);

static void attach_to_function (GumAllocatorProbe * self,
    gpointer function_address, const HeapHandlers * function_handlers,
    gpointer user_data);

static void gum_allocator_probe_on_malloc (GumAllocatorProbe * self,
    gpointer address, guint size, const GumCpuContext * cpu_context);
static void gum_allocator_probe_on_free (GumAllocatorProbe * self,
    gpointer address, const GumCpuContext * cpu_context);
static void gum_allocator_probe_on_realloc (GumAllocatorProbe * self,
    gpointer old_address, gpointer new_address, guint new_size,
    const GumCpuContext * cpu_context);

static void on_malloc_enter_handler (GumAllocatorProbe * self,
    AllocThreadContext * thread_ctx, GumInvocationContext * invocation_ctx);
static void on_calloc_enter_handler (GumAllocatorProbe * self,
    AllocThreadContext * thread_ctx, GumInvocationContext * invocation_ctx);
static void on_shared_xalloc_leave_handler (GumAllocatorProbe * self,
    AllocThreadContext * thread_ctx, GumInvocationContext * invocation_ctx);
static void on_realloc_enter_handler (GumAllocatorProbe * self,
    ReallocThreadContext * thread_ctx, GumInvocationContext * invocation_ctx);
static void on_realloc_leave_handler (GumAllocatorProbe * self,
    ReallocThreadContext * thread_ctx, GumInvocationContext * invocation_ctx);
static void on_free_enter_handler (GumAllocatorProbe * self,
    gpointer thread_ctx, GumInvocationContext * invocation_ctx);

static void on_malloc_dbg_enter_handler (GumAllocatorProbe * self,
    AllocThreadContext * thread_ctx, GumInvocationContext * invocation_ctx);
static void on_calloc_dbg_enter_handler (GumAllocatorProbe * self,
    AllocThreadContext * thread_ctx, GumInvocationContext * invocation_ctx);
static void on_realloc_dbg_enter_handler (GumAllocatorProbe * self,
    ReallocThreadContext * thread_ctx, GumInvocationContext * invocation_ctx);
static void on_free_dbg_enter_handler (GumAllocatorProbe * self,
    ThreadContext * thread_ctx, GumInvocationContext * invocation_ctx,
    gpointer user_data);

static void decide_ignore_from_block_type (ThreadContext * thread_ctx,
    gint block_type);

G_DEFINE_TYPE_EXTENDED (GumAllocatorProbe,
                        gum_allocator_probe,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_INVOCATION_LISTENER,
                            gum_allocator_probe_listener_iface_init))

G_LOCK_DEFINE (_gum_allocator_probe_ignored_functions);
static GArray * _gum_allocator_probe_ignored_functions = NULL;

static void
gum_allocator_probe_class_init (GumAllocatorProbeClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);
  GParamSpec * pspec;

  object_class->set_property = gum_allocator_probe_set_property;
  object_class->get_property = gum_allocator_probe_get_property;
  object_class->dispose = gum_allocator_probe_dispose;
  object_class->finalize = gum_allocator_probe_finalize;

  pspec = g_param_spec_object ("allocation-tracker", "AllocationTracker",
      "AllocationTracker to use", GUM_TYPE_ALLOCATION_TRACKER,
      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ALLOCATION_TRACKER,
      pspec);

  pspec = g_param_spec_boolean ("enable-counters", "Enable Counters",
      "Enable counters for probed functions", DEFAULT_ENABLE_COUNTERS,
      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ENABLE_COUNTERS,
      pspec);

  pspec = g_param_spec_uint ("malloc-count", "Malloc Count",
      "Number of malloc() calls seen so far", 0, G_MAXUINT, 0,
      (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_MALLOC_COUNT, pspec);

  pspec = g_param_spec_uint ("realloc-count", "Realloc Count",
      "Number of realloc() calls seen so far", 0, G_MAXUINT, 0,
      (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_REALLOC_COUNT, pspec);

  pspec = g_param_spec_uint ("free-count", "Free Count",
      "Number of free() calls seen so far", 0, G_MAXUINT, 0,
      (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FREE_COUNT, pspec);

  _gum_register_destructor (gum_allocator_probe_deinit);
}

static void
gum_allocator_probe_deinit (void)
{
  if (_gum_allocator_probe_ignored_functions != NULL)
  {
    g_array_free (_gum_allocator_probe_ignored_functions, TRUE);
    _gum_allocator_probe_ignored_functions = NULL;
  }
}

static void
gum_allocator_probe_listener_iface_init (gpointer g_iface,
                                         gpointer iface_data)
{
  GumInvocationListenerInterface * iface = g_iface;

  iface->on_enter = gum_allocator_probe_on_enter;
  iface->on_leave = gum_allocator_probe_on_leave;
}

static void
gum_allocator_probe_init (GumAllocatorProbe * self)
{
  self->interceptor = gum_interceptor_obtain ();
  self->function_contexts = g_ptr_array_sized_new (3);

  self->enable_counters = DEFAULT_ENABLE_COUNTERS;
}

static void
gum_allocator_probe_dispose (GObject * object)
{
  GumAllocatorProbe * self = GUM_ALLOCATOR_PROBE (object);

  if (!self->disposed)
  {
    self->disposed = TRUE;

    gum_allocator_probe_detach (self);

    g_clear_object (&self->allocation_tracker);

    g_clear_object (&self->interceptor);
  }

  G_OBJECT_CLASS (gum_allocator_probe_parent_class)->dispose (object);
}

static void
gum_allocator_probe_finalize (GObject * object)
{
  GumAllocatorProbe * self = GUM_ALLOCATOR_PROBE (object);

  g_ptr_array_free (self->function_contexts, TRUE);

  G_OBJECT_CLASS (gum_allocator_probe_parent_class)->finalize (object);
}

static void
gum_allocator_probe_set_property (GObject * object,
                                  guint property_id,
                                  const GValue * value,
                                  GParamSpec * pspec)
{
  GumAllocatorProbe * self = GUM_ALLOCATOR_PROBE (object);

  switch (property_id)
  {
    case PROP_ALLOCATION_TRACKER:
      if (self->allocation_tracker != NULL)
        g_object_unref (self->allocation_tracker);
      self->allocation_tracker = g_value_dup_object (value);
      break;
    case PROP_ENABLE_COUNTERS:
      self->enable_counters = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
gum_allocator_probe_get_property (GObject * object,
                                  guint property_id,
                                  GValue * value,
                                  GParamSpec * pspec)
{
  GumAllocatorProbe * self = GUM_ALLOCATOR_PROBE (object);

  switch (property_id)
  {
    case PROP_ALLOCATION_TRACKER:
      g_value_set_object (value, self->allocation_tracker);
      break;
    case PROP_ENABLE_COUNTERS:
      g_value_set_boolean (value, self->enable_counters);
      break;
    case PROP_MALLOC_COUNT:
      g_value_set_uint (value, self->malloc_count);
      break;
    case PROP_REALLOC_COUNT:
      g_value_set_uint (value, self->realloc_count);
      break;
    case PROP_FREE_COUNT:
      g_value_set_uint (value, self->free_count);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static gboolean
gum_allocator_probe_add_suppression_addresses_if_glib (GumModule * module,
                                                       gpointer user_data)
{
  static const gchar * glib_function_name[] = {
    "g_quark_from_string",
    "g_quark_from_static_string",
    NULL
  };
  static const gchar * gobject_function_name[] = {
    "g_signal_connect_data",
    "g_signal_handlers_destroy",
    "g_type_register_static",
    "g_type_add_interface_static",
    "g_param_spec_pool_insert",
    NULL
  };
  GArray * ignored = (GArray *) user_data;
  gchar * name_lowercase;
  static const gchar ** function_name;

  name_lowercase = g_ascii_strdown (gum_module_get_name (module), -1);

  if (g_strstr_len (name_lowercase, -1, "glib-2.0") != NULL)
    function_name = glib_function_name;
  else if (g_strstr_len (name_lowercase, -1, "gobject-2.0") != NULL)
    function_name = gobject_function_name;
  else
    function_name = NULL;

  if (function_name != NULL)
  {
    guint i;

    for (i = 0; function_name[i] != NULL; i++)
    {
      gpointer address = GSIZE_TO_POINTER (
          gum_module_find_export_by_name (module, function_name[i]));
      g_array_append_val (ignored, address);
    }
  }

  g_free (name_lowercase);

  return TRUE;
}

static void
gum_allocator_probe_apply_default_suppressions (GumAllocatorProbe * self)
{
  GumInterceptor * interceptor = self->interceptor;
  GArray * ignored;
  guint i;

  G_LOCK (_gum_allocator_probe_ignored_functions);

  if (_gum_allocator_probe_ignored_functions == NULL)
  {
    static const gchar * internal_function_name[] = {
        "g_quark_new",
        "instance_real_class_set",
        "instance_real_class_remove",
        "gst_object_set_name_default"
    };

    ignored = g_array_new (FALSE, FALSE, sizeof (gpointer));

    for (i = 0; i != G_N_ELEMENTS (internal_function_name); i++)
    {
      GArray * addrs = gum_find_functions_named (internal_function_name[i]);
      if (addrs->len != 0)
        g_array_append_vals (ignored, addrs->data, addrs->len);
      g_array_free (addrs, TRUE);
    }

    gum_process_enumerate_modules (
        gum_allocator_probe_add_suppression_addresses_if_glib, ignored);

    _gum_allocator_probe_ignored_functions = ignored;
  }
  else
  {
    ignored = _gum_allocator_probe_ignored_functions;
  }

  G_UNLOCK (_gum_allocator_probe_ignored_functions);

  gum_interceptor_begin_transaction (interceptor);

  for (i = 0; i != ignored->len; i++)
    gum_allocator_probe_suppress (self, g_array_index (ignored, gpointer, i));

  gum_allocator_probe_suppress (self,
      GUM_FUNCPTR_TO_POINTER (g_quark_from_string));
  gum_allocator_probe_suppress (self,
      GUM_FUNCPTR_TO_POINTER (g_quark_from_static_string));

  gum_allocator_probe_suppress (self,
      GUM_FUNCPTR_TO_POINTER (g_signal_connect_data));
  gum_allocator_probe_suppress (self,
      GUM_FUNCPTR_TO_POINTER (g_signal_handlers_destroy));
  gum_allocator_probe_suppress (self,
      GUM_FUNCPTR_TO_POINTER (g_type_register_static));
  gum_allocator_probe_suppress (self,
      GUM_FUNCPTR_TO_POINTER (g_type_add_interface_static));
  gum_allocator_probe_suppress (self,
      GUM_FUNCPTR_TO_POINTER (g_param_spec_pool_insert));

  gum_interceptor_end_transaction (interceptor);
}

static void
gum_allocator_probe_on_enter (GumInvocationListener * listener,
                              GumInvocationContext * context)
{
  GumAllocatorProbe * self;
  FunctionContext * function_ctx;

  self = GUM_ALLOCATOR_PROBE (listener);
  function_ctx = GUM_IC_GET_FUNC_DATA (context, FunctionContext *);

  gum_interceptor_ignore_current_thread (self->interceptor);

  if (function_ctx != NULL)
  {
    ThreadContext * base_thread_ctx;

    base_thread_ctx = GUM_IC_GET_INVOCATION_DATA (context, ThreadContext);
    base_thread_ctx->ignored = FALSE;

    function_ctx->handlers.enter_handler (self, base_thread_ctx, context,
        function_ctx->handler_data);
  }
}

static void
gum_allocator_probe_on_leave (GumInvocationListener * listener,
                              GumInvocationContext * context)
{
  GumAllocatorProbe * self;
  FunctionContext * function_ctx;

  self = GUM_ALLOCATOR_PROBE (listener);
  function_ctx = GUM_IC_GET_FUNC_DATA (context, FunctionContext *);

  if (function_ctx != NULL)
  {
    ThreadContext * base_thread_ctx;

    base_thread_ctx = GUM_IC_GET_INVOCATION_DATA (context, ThreadContext);

    if (!base_thread_ctx->ignored)
    {
      if (function_ctx->handlers.leave_handler != NULL)
      {
        function_ctx->handlers.leave_handler (self, base_thread_ctx,
            context, function_ctx->handler_data);
      }
    }
  }

  gum_interceptor_unignore_current_thread (self->interceptor);
}

GumAllocatorProbe *
gum_allocator_probe_new (void)
{
  return g_object_new (GUM_TYPE_ALLOCATOR_PROBE, NULL);
}

static const HeapHandlers gum_malloc_handlers =
{
  (HeapEnterHandler) on_malloc_enter_handler,
  (HeapLeaveHandler) on_shared_xalloc_leave_handler
};

static const HeapHandlers gum_calloc_handlers =
{
  (HeapEnterHandler) on_calloc_enter_handler,
  (HeapLeaveHandler) on_shared_xalloc_leave_handler
};

static const HeapHandlers gum_realloc_handlers =
{
  (HeapEnterHandler) on_realloc_enter_handler,
  (HeapLeaveHandler) on_realloc_leave_handler
};

static const HeapHandlers gum_free_handlers =
{
  (HeapEnterHandler) on_free_enter_handler,
  NULL
};

static const HeapHandlers gum__malloc_dbg_handlers =
{
  (HeapEnterHandler) on_malloc_dbg_enter_handler,
  (HeapLeaveHandler) on_shared_xalloc_leave_handler
};

static const HeapHandlers gum__calloc_dbg_handlers =
{
  (HeapEnterHandler) on_calloc_dbg_enter_handler,
  (HeapLeaveHandler) on_shared_xalloc_leave_handler
};

static const HeapHandlers gum__realloc_dbg_handlers =
{
  (HeapEnterHandler) on_realloc_dbg_enter_handler,
  (HeapLeaveHandler) on_realloc_leave_handler
};

static const HeapHandlers gum__free_dbg_handlers =
{
  (HeapEnterHandler) on_free_dbg_enter_handler,
  NULL
};

void
gum_allocator_probe_attach (GumAllocatorProbe * self)
{
  GumHeapApiList * apis = gum_process_find_heap_apis ();
  gum_allocator_probe_attach_to_apis (self, apis);
  gum_heap_api_list_free (apis);
}

#define GUM_ATTACH_TO_API_FUNC(name) \
    attach_to_function (self, GUM_FUNCPTR_TO_POINTER (api->name), \
        &gum_##name##_handlers, NULL)
#define GUM_ATTACH_TO_API_FUNC_WITH_DATA(name, data) \
    attach_to_function (self, GUM_FUNCPTR_TO_POINTER (api->name), \
        &gum_##name##_handlers, data)

void
gum_allocator_probe_attach_to_apis (GumAllocatorProbe * self,
                                    const GumHeapApiList * apis)
{
  guint i;

  gum_interceptor_ignore_current_thread (self->interceptor);
  gum_interceptor_begin_transaction (self->interceptor);

  for (i = 0; i != apis->len; i++)
  {
    const GumHeapApi * api = gum_heap_api_list_get_nth (apis, i);

    GUM_ATTACH_TO_API_FUNC (malloc);
    GUM_ATTACH_TO_API_FUNC (calloc);
    GUM_ATTACH_TO_API_FUNC (realloc);
    GUM_ATTACH_TO_API_FUNC (free);

    if (api->_malloc_dbg != NULL)
    {
      GUM_ATTACH_TO_API_FUNC (_malloc_dbg);
      GUM_ATTACH_TO_API_FUNC (_calloc_dbg);
      GUM_ATTACH_TO_API_FUNC (_realloc_dbg);
      GUM_ATTACH_TO_API_FUNC_WITH_DATA (_free_dbg,
          GUM_FUNCPTR_TO_POINTER (api->_CrtReportBlockType));
    }
  }

  gum_allocator_probe_apply_default_suppressions (self);

  gum_interceptor_end_transaction (self->interceptor);
  gum_interceptor_unignore_current_thread (self->interceptor);
}

void
gum_allocator_probe_detach (GumAllocatorProbe * self)
{
  guint i;

  gum_interceptor_ignore_current_thread (self->interceptor);

  gum_interceptor_detach (self->interceptor, GUM_INVOCATION_LISTENER (self));

  for (i = 0; i < self->function_contexts->len; i++)
  {
    FunctionContext * function_ctx = (FunctionContext *)
        g_ptr_array_index (self->function_contexts, i);
    g_free (function_ctx);
  }

  g_ptr_array_set_size (self->function_contexts, 0);

  self->malloc_count = 0;
  self->realloc_count = 0;
  self->free_count = 0;

  gum_interceptor_unignore_current_thread (self->interceptor);
}

static void
attach_to_function (GumAllocatorProbe * self,
                    gpointer function_address,
                    const HeapHandlers * function_handlers,
                    gpointer user_data)
{
  GumInvocationListener * listener = GUM_INVOCATION_LISTENER (self);
  FunctionContext * function_ctx;
  GumAttachOptions options = { 0, };

  function_ctx = g_new0 (FunctionContext, 1);
  function_ctx->handlers = *function_handlers;
  function_ctx->handler_data = user_data;
  g_ptr_array_add (self->function_contexts, function_ctx);

  options.listener_function_data = function_ctx;
  gum_interceptor_attach (self->interceptor, function_address, listener,
      &options);
}

void
gum_allocator_probe_suppress (GumAllocatorProbe * self,
                              gpointer function_address)
{
  GumInvocationListener * listener = GUM_INVOCATION_LISTENER (self);

  gum_interceptor_attach (self->interceptor, function_address, listener,
      NULL);
}

static void
gum_allocator_probe_on_malloc (GumAllocatorProbe * self,
                               gpointer address,
                               guint size,
                               const GumCpuContext * cpu_context)
{
  if (self->enable_counters)
    self->malloc_count++;

  if (self->allocation_tracker != NULL)
  {
    gum_allocation_tracker_on_malloc_full (self->allocation_tracker, address,
        size, cpu_context);
  }
}

static void
gum_allocator_probe_on_free (GumAllocatorProbe * self,
                             gpointer address,
                             const GumCpuContext * cpu_context)
{
  if (self->enable_counters)
    self->free_count++;

  if (self->allocation_tracker != NULL)
  {
    gum_allocation_tracker_on_free_full (self->allocation_tracker, address,
        cpu_context);
  }
}

static void
gum_allocator_probe_on_realloc (GumAllocatorProbe * self,
                                gpointer old_address,
                                gpointer new_address,
                                guint new_size,
                                const GumCpuContext * cpu_context)
{
  if (self->enable_counters)
    self->realloc_count++;

  if (self->allocation_tracker != NULL)
  {
    gum_allocation_tracker_on_realloc_full (self->allocation_tracker,
        old_address, new_address, new_size, cpu_context);
  }
}

static void
on_malloc_enter_handler (GumAllocatorProbe * self,
                         AllocThreadContext * thread_ctx,
                         GumInvocationContext * invocation_ctx)
{
  thread_ctx->cpu_context = *invocation_ctx->cpu_context;
  thread_ctx->size =
      (gsize) gum_invocation_context_get_nth_argument (invocation_ctx, 0);
}

static void
on_calloc_enter_handler (GumAllocatorProbe * self,
                         AllocThreadContext * thread_ctx,
                         GumInvocationContext * invocation_ctx)
{
  gsize num, size;

  num = (gsize) gum_invocation_context_get_nth_argument (invocation_ctx, 0);
  size = (gsize) gum_invocation_context_get_nth_argument (invocation_ctx, 1);

  thread_ctx->cpu_context = *invocation_ctx->cpu_context;
  thread_ctx->size = num * size;
}

static void
on_shared_xalloc_leave_handler (GumAllocatorProbe * self,
                                AllocThreadContext * thread_ctx,
                                GumInvocationContext * invocation_ctx)
{
  gpointer return_value;

  return_value = gum_invocation_context_get_return_value (invocation_ctx);

  if (return_value != NULL)
  {
    gum_allocator_probe_on_malloc (self, return_value, thread_ctx->size,
        &thread_ctx->cpu_context);
  }
}

static void
on_realloc_enter_handler (GumAllocatorProbe * self,
                          ReallocThreadContext * thread_ctx,
                          GumInvocationContext * invocation_ctx)
{
  thread_ctx->cpu_context = *invocation_ctx->cpu_context;
  thread_ctx->old_address =
      gum_invocation_context_get_nth_argument (invocation_ctx, 0);
  thread_ctx->new_size =
      (gsize) gum_invocation_context_get_nth_argument (invocation_ctx, 1);
}

static void
on_realloc_leave_handler (GumAllocatorProbe * self,
                          ReallocThreadContext * thread_ctx,
                          GumInvocationContext * invocation_ctx)
{
  gpointer return_value;

  return_value = gum_invocation_context_get_return_value (invocation_ctx);

  if (return_value != NULL)
  {
    gum_allocator_probe_on_realloc (self, thread_ctx->old_address,
        return_value, thread_ctx->new_size, &thread_ctx->cpu_context);
  }
}

static void
on_free_enter_handler (GumAllocatorProbe * self,
                       gpointer thread_ctx,
                       GumInvocationContext * invocation_ctx)
{
  gpointer address;

  address = gum_invocation_context_get_nth_argument (invocation_ctx, 0);

  gum_allocator_probe_on_free (self, address, invocation_ctx->cpu_context);
}

static void
on_malloc_dbg_enter_handler (GumAllocatorProbe * self,
                             AllocThreadContext * thread_ctx,
                             GumInvocationContext * invocation_ctx)
{
  gint block_type;

  block_type = (gint) GPOINTER_TO_SIZE (
      gum_invocation_context_get_nth_argument (invocation_ctx, 1));

  decide_ignore_from_block_type ((ThreadContext *) thread_ctx, block_type);

  if (!thread_ctx->ignored)
    on_malloc_enter_handler (self, thread_ctx, invocation_ctx);
}

static void
on_calloc_dbg_enter_handler (GumAllocatorProbe * self,
                             AllocThreadContext * thread_ctx,
                             GumInvocationContext * invocation_ctx)
{
  gint block_type;

  block_type = (gint) GPOINTER_TO_SIZE (
      gum_invocation_context_get_nth_argument (invocation_ctx, 2));

  decide_ignore_from_block_type ((ThreadContext *) thread_ctx, block_type);

  if (!thread_ctx->ignored)
    on_calloc_enter_handler (self, thread_ctx, invocation_ctx);
}

static void
on_realloc_dbg_enter_handler (GumAllocatorProbe * self,
                              ReallocThreadContext * thread_ctx,
                              GumInvocationContext * invocation_ctx)
{
  gint block_type;

  block_type = (gint) GPOINTER_TO_SIZE (
      gum_invocation_context_get_nth_argument (invocation_ctx, 2));

  decide_ignore_from_block_type ((ThreadContext *) thread_ctx, block_type);

  if (!thread_ctx->ignored)
    on_realloc_enter_handler (self, thread_ctx, invocation_ctx);
}

static void
on_free_dbg_enter_handler (GumAllocatorProbe * self,
                           ThreadContext * thread_ctx,
                           GumInvocationContext * invocation_ctx,
                           gpointer user_data)
{
  gint block_type;

  block_type = (gint) GPOINTER_TO_SIZE (
      gum_invocation_context_get_nth_argument (invocation_ctx, 1));
  if (block_type == GUM_DBGCRT_UNKNOWN_BLOCK)
  {
    gpointer block;
    GumReportBlockTypeFunc report_block_type;

    block = gum_invocation_context_get_nth_argument (invocation_ctx, 0);
    report_block_type =
        GUM_POINTER_TO_FUNCPTR (GumReportBlockTypeFunc, user_data);

    block_type = GUM_DBGCRT_BLOCK_TYPE (report_block_type (block));
  }

  decide_ignore_from_block_type ((ThreadContext *) thread_ctx, block_type);

  if (!thread_ctx->ignored)
    on_free_enter_handler (self, thread_ctx, invocation_ctx);
}

static void
decide_ignore_from_block_type (ThreadContext * thread_ctx,
                               gint block_type)
{
  thread_ctx->ignored = (block_type != GUM_DBGCRT_NORMAL_BLOCK);
}
