/*
 * Copyright (C) 2008-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumboundschecker.h"

#include "gumexceptor.h"
#include "guminterceptor.h"
#include "gumlibc.h"
#include "gumpagepool.h"

#include <stdlib.h>
#include <string.h>

#define DEFAULT_POOL_SIZE       4096
#define DEFAULT_FRONT_ALIGNMENT   16

#define GUM_BOUNDS_CHECKER_LOCK() g_mutex_lock (&self->mutex)
#define GUM_BOUNDS_CHECKER_UNLOCK() g_mutex_unlock (&self->mutex)

#define BLOCK_ALLOC_RETADDRS(b) \
    ((GumReturnAddressArray *) (b)->guard)
#define BLOCK_FREE_RETADDRS(b) \
    ((GumReturnAddressArray *) ((guint8 *) (b)->guard + ((b)->guard_size / 2)))

typedef struct _GumBoundsHookGroup GumBoundsHookGroup;

struct _GumBoundsChecker
{
  GObject parent;

  gboolean disposed;

  GMutex mutex;

  GumBacktracerInterface * backtracer_iface;
  GumBacktracer * backtracer_instance;
  GumBoundsOutputFunc output;
  gpointer output_user_data;

  GumInterceptor * interceptor;
  GumExceptor * exceptor;
  GumHeapApiList * heap_apis;
  GumBoundsHookGroup * hook_groups;
  gboolean attached;
  volatile gboolean detaching;
  volatile gboolean handled_invalid_access;

  guint pool_size;
  guint front_alignment;
  GumPagePool * page_pool;
};

struct _GumBoundsHookGroup
{
  GumBoundsChecker * checker;
  const GumHeapApi * api;
};

enum
{
  PROP_0,
  PROP_BACKTRACER,
  PROP_POOL_SIZE,
  PROP_FRONT_ALIGNMENT
};

static void gum_bounds_checker_dispose (GObject * object);
static void gum_bounds_checker_finalize (GObject * object);

static void gum_bounds_checker_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gum_bounds_checker_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);

static gpointer replacement_malloc (gsize size);
static gpointer replacement_calloc (gsize num, gsize size);
static gpointer replacement_realloc (gpointer old_address,
    gsize new_size);
static void replacement_free (gpointer address);

static gpointer gum_bounds_checker_try_alloc (GumBoundsChecker * self,
    guint size, GumInvocationContext * ctx);
static gboolean gum_bounds_checker_try_free (GumBoundsChecker * self,
    gpointer address, GumInvocationContext * ctx);

static gboolean gum_bounds_checker_on_exception (GumExceptionDetails * details,
    gpointer user_data);
static void gum_bounds_checker_append_backtrace (
    const GumReturnAddressArray * arr, GString * s);

G_DEFINE_TYPE (GumBoundsChecker, gum_bounds_checker, G_TYPE_OBJECT)

static void
gum_bounds_checker_class_init (GumBoundsCheckerClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_bounds_checker_dispose;
  object_class->finalize = gum_bounds_checker_finalize;
  object_class->get_property = gum_bounds_checker_get_property;
  object_class->set_property = gum_bounds_checker_set_property;

  g_object_class_install_property (object_class, PROP_BACKTRACER,
      g_param_spec_object ("backtracer", "Backtracer",
      "Backtracer Implementation", GUM_TYPE_BACKTRACER,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_POOL_SIZE,
      g_param_spec_uint ("pool-size", "Pool Size",
      "Pool size in number of pages",
      2, G_MAXUINT, DEFAULT_POOL_SIZE,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_FRONT_ALIGNMENT,
      g_param_spec_uint ("front-alignment", "Front Alignment",
      "Front alignment requirement",
      1, 64, DEFAULT_FRONT_ALIGNMENT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gum_bounds_checker_init (GumBoundsChecker * self)
{
  g_mutex_init (&self->mutex);

  self->interceptor = gum_interceptor_obtain ();
  self->exceptor = gum_exceptor_obtain ();
  self->pool_size = DEFAULT_POOL_SIZE;
  self->front_alignment = DEFAULT_FRONT_ALIGNMENT;

  gum_exceptor_add (self->exceptor, gum_bounds_checker_on_exception, self);
}

static void
gum_bounds_checker_dispose (GObject * object)
{
  GumBoundsChecker * self = GUM_BOUNDS_CHECKER (object);

  if (!self->disposed)
  {
    self->disposed = TRUE;

    gum_bounds_checker_detach (self);

    gum_exceptor_remove (self->exceptor, gum_bounds_checker_on_exception, self);
    g_object_unref (self->exceptor);
    self->exceptor = NULL;

    g_object_unref (self->interceptor);
    self->interceptor = NULL;

    g_clear_object (&self->backtracer_instance);
    self->backtracer_iface = NULL;
  }

  G_OBJECT_CLASS (gum_bounds_checker_parent_class)->dispose (object);
}

static void
gum_bounds_checker_finalize (GObject * object)
{
  GumBoundsChecker * self = GUM_BOUNDS_CHECKER (object);

  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (gum_bounds_checker_parent_class)->finalize (object);
}

static void
gum_bounds_checker_get_property (GObject * object,
                                 guint property_id,
                                 GValue * value,
                                 GParamSpec * pspec)
{
  GumBoundsChecker * self = GUM_BOUNDS_CHECKER (object);

  switch (property_id)
  {
    case PROP_BACKTRACER:
      g_value_set_object (value, self->backtracer_instance);
      break;
    case PROP_POOL_SIZE:
      g_value_set_uint (value, gum_bounds_checker_get_pool_size (self));
      break;
    case PROP_FRONT_ALIGNMENT:
      g_value_set_uint (value, gum_bounds_checker_get_front_alignment (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
gum_bounds_checker_set_property (GObject * object,
                                 guint property_id,
                                 const GValue * value,
                                 GParamSpec * pspec)
{
  GumBoundsChecker * self = GUM_BOUNDS_CHECKER (object);

  switch (property_id)
  {
    case PROP_BACKTRACER:
      if (self->backtracer_instance != NULL)
        g_object_unref (self->backtracer_instance);
      self->backtracer_instance = g_value_dup_object (value);

      if (self->backtracer_instance != NULL)
      {
        self->backtracer_iface =
            GUM_BACKTRACER_GET_IFACE (self->backtracer_instance);
      }
      else
      {
        self->backtracer_iface = NULL;
      }

      break;
    case PROP_POOL_SIZE:
      gum_bounds_checker_set_pool_size (self, g_value_get_uint (value));
      break;
    case PROP_FRONT_ALIGNMENT:
      gum_bounds_checker_set_front_alignment (self, g_value_get_uint (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

GumBoundsChecker *
gum_bounds_checker_new (GumBacktracer * backtracer,
                        GumBoundsOutputFunc func,
                        gpointer user_data)
{
  GumBoundsChecker * checker;

  checker = g_object_new (GUM_TYPE_BOUNDS_CHECKER,
      "backtracer", backtracer,
      NULL);

  checker->output = func;
  checker->output_user_data = user_data;

  return checker;
}

guint
gum_bounds_checker_get_pool_size (GumBoundsChecker * self)
{
  return self->pool_size;
}

void
gum_bounds_checker_set_pool_size (GumBoundsChecker * self,
                                  guint pool_size)
{
  g_assert (self->page_pool == NULL);
  self->pool_size = pool_size;
}

guint
gum_bounds_checker_get_front_alignment (GumBoundsChecker * self)
{
  return self->front_alignment;
}

void
gum_bounds_checker_set_front_alignment (GumBoundsChecker * self,
                                        guint pool_size)
{
  g_assert (self->page_pool == NULL);
  self->front_alignment = pool_size;
}

void
gum_bounds_checker_attach (GumBoundsChecker * self)
{
  GumHeapApiList * apis = gum_process_find_heap_apis ();
  gum_bounds_checker_attach_to_apis (self, apis);
  gum_heap_api_list_free (apis);
}

void
gum_bounds_checker_attach_to_apis (GumBoundsChecker * self,
                                   const GumHeapApiList * apis)
{
  guint i;

  g_assert (self->heap_apis == NULL);
  self->heap_apis = gum_heap_api_list_copy (apis);

  g_assert (self->hook_groups == NULL);
  self->hook_groups = g_new0 (GumBoundsHookGroup, apis->len);

  g_assert (self->page_pool == NULL);
  self->page_pool = gum_page_pool_new (GUM_PROTECT_MODE_ABOVE,
      self->pool_size);
  g_object_set (self->page_pool, "front-alignment", self->front_alignment,
      NULL);

  gum_interceptor_begin_transaction (self->interceptor);

  for (i = 0; i != apis->len; i++)
  {
    const GumHeapApi * api;
    GumBoundsHookGroup * group;
    GumReplaceOptions options = { 0, };

    api = gum_heap_api_list_get_nth (apis, i);

    group = &self->hook_groups[i];
    group->checker = self;
    group->api = api;

    options.replacement_data = group;

#define GUM_REPLACE_API_FUNC(name) \
    gum_interceptor_replace (self->interceptor, \
        GUM_FUNCPTR_TO_POINTER (api->name), \
        GUM_FUNCPTR_TO_POINTER (replacement_##name), NULL, &options)

    GUM_REPLACE_API_FUNC (malloc);
    GUM_REPLACE_API_FUNC (calloc);
    GUM_REPLACE_API_FUNC (realloc);
    GUM_REPLACE_API_FUNC (free);

#undef GUM_REPLACE_API_FUNC
  }

  gum_interceptor_end_transaction (self->interceptor);

  self->attached = TRUE;
}

void
gum_bounds_checker_detach (GumBoundsChecker * self)
{
  if (self->attached)
  {
    guint i;

    self->attached = FALSE;
    self->detaching = TRUE;

    g_assert (gum_page_pool_peek_used (self->page_pool) == 0);

    gum_interceptor_begin_transaction (self->interceptor);

    for (i = 0; i != self->heap_apis->len; i++)
    {
      const GumHeapApi * api = gum_heap_api_list_get_nth (self->heap_apis, i);

#define GUM_REVERT_API_FUNC(name) \
      gum_interceptor_revert (self->interceptor, \
          GUM_FUNCPTR_TO_POINTER (api->name))

      GUM_REVERT_API_FUNC (malloc);
      GUM_REVERT_API_FUNC (calloc);
      GUM_REVERT_API_FUNC (realloc);
      GUM_REVERT_API_FUNC (free);

  #undef GUM_REVERT_API_FUNC
    }

    gum_interceptor_end_transaction (self->interceptor);

    g_object_unref (self->page_pool);
    self->page_pool = NULL;

    g_free (self->hook_groups);
    self->hook_groups = NULL;

    gum_heap_api_list_free (self->heap_apis);
    self->heap_apis = NULL;
  }
}

static gpointer
replacement_malloc (gsize size)
{
  GumInvocationContext * ctx;
  GumBoundsHookGroup * group;
  GumBoundsChecker * self;
  gpointer result;

  ctx = gum_interceptor_get_current_invocation ();
  group = GUM_IC_GET_REPLACEMENT_DATA (ctx, GumBoundsHookGroup *);
  self = group->checker;

  if (self->detaching || self->handled_invalid_access)
    goto fallback;

  GUM_BOUNDS_CHECKER_LOCK ();
  result = gum_bounds_checker_try_alloc (self, MAX (size, 1), ctx);
  GUM_BOUNDS_CHECKER_UNLOCK ();
  if (result == NULL)
    goto fallback;

  return result;

fallback:
  return group->api->malloc (size);
}

static gpointer
replacement_calloc (gsize num,
                    gsize size)
{
  GumInvocationContext * ctx;
  GumBoundsHookGroup * group;
  GumBoundsChecker * self;
  gpointer result;

  ctx = gum_interceptor_get_current_invocation ();
  group = GUM_IC_GET_REPLACEMENT_DATA (ctx, GumBoundsHookGroup *);
  self = group->checker;

  if (self->detaching || self->handled_invalid_access)
    goto fallback;

  GUM_BOUNDS_CHECKER_LOCK ();
  result = gum_bounds_checker_try_alloc (self, MAX (num * size, 1), ctx);
  GUM_BOUNDS_CHECKER_UNLOCK ();
  if (result != NULL)
    gum_memset (result, 0, num * size);
  else
    goto fallback;

  return result;

fallback:
  return group->api->calloc (num, size);
}

static gpointer
replacement_realloc (gpointer old_address,
                     gsize new_size)
{
  GumInvocationContext * ctx;
  GumBoundsHookGroup * group;
  GumBoundsChecker * self;
  gpointer result = NULL;
  GumBlockDetails old_block;

  ctx = gum_interceptor_get_current_invocation ();
  group = GUM_IC_GET_REPLACEMENT_DATA (ctx, GumBoundsHookGroup *);
  self = group->checker;

  if (old_address == NULL)
    return group->api->malloc (new_size);

  if (new_size == 0)
  {
    group->api->free (old_address);
    return NULL;
  }

  if (self->detaching || self->handled_invalid_access)
    goto fallback;

  GUM_BOUNDS_CHECKER_LOCK ();

  if (!gum_page_pool_query_block_details (self->page_pool, old_address,
      &old_block))
  {
    GUM_BOUNDS_CHECKER_UNLOCK ();

    goto fallback;
  }

  result = gum_bounds_checker_try_alloc (self, new_size, ctx);

  GUM_BOUNDS_CHECKER_UNLOCK ();

  if (result == NULL)
    result = group->api->malloc (new_size);

  if (result != NULL)
    gum_memcpy (result, old_address, MIN (old_block.size, new_size));

  GUM_BOUNDS_CHECKER_LOCK ();
  gum_bounds_checker_try_free (self, old_address, ctx);
  GUM_BOUNDS_CHECKER_UNLOCK ();

  return result;

fallback:
  return group->api->realloc (old_address, new_size);
}

static void
replacement_free (gpointer address)
{
  GumInvocationContext * ctx;
  GumBoundsHookGroup * group;
  GumBoundsChecker * self;
  gboolean freed;

  ctx = gum_interceptor_get_current_invocation ();
  group = GUM_IC_GET_REPLACEMENT_DATA (ctx, GumBoundsHookGroup *);
  self = group->checker;

  GUM_BOUNDS_CHECKER_LOCK ();
  freed = gum_bounds_checker_try_free (self, address, ctx);
  GUM_BOUNDS_CHECKER_UNLOCK ();

  if (!freed)
    group->api->free (address);
}

static gpointer
gum_bounds_checker_try_alloc (GumBoundsChecker * self,
                              guint size,
                              GumInvocationContext * ctx)
{
  gpointer result;

  result = gum_page_pool_try_alloc (self->page_pool, size);

  if (result != NULL && self->backtracer_instance != NULL)
  {
    GumBlockDetails block;

    gum_page_pool_query_block_details (self->page_pool, result, &block);

    gum_mprotect (block.guard, block.guard_size, GUM_PAGE_RW);

    g_assert (block.guard_size / 2 >= sizeof (GumReturnAddressArray));
    self->backtracer_iface->generate (self->backtracer_instance,
        ctx->cpu_context, BLOCK_ALLOC_RETADDRS (&block),
        GUM_MAX_BACKTRACE_DEPTH);

    BLOCK_FREE_RETADDRS (&block)->len = 0;

    gum_mprotect (block.guard, block.guard_size, GUM_PAGE_NO_ACCESS);
  }

  return result;
}

static gboolean
gum_bounds_checker_try_free (GumBoundsChecker * self,
                             gpointer address,
                             GumInvocationContext * ctx)
{
  gboolean freed;

  freed = gum_page_pool_try_free (self->page_pool, address);

  if (freed && self->backtracer_instance != NULL)
  {
    GumBlockDetails block;

    gum_page_pool_query_block_details (self->page_pool, address, &block);

    gum_mprotect (block.guard, block.guard_size, GUM_PAGE_RW);

    g_assert (block.guard_size / 2 >= sizeof (GumReturnAddressArray));
    self->backtracer_iface->generate (self->backtracer_instance,
        ctx->cpu_context, BLOCK_FREE_RETADDRS (&block),
        GUM_MAX_BACKTRACE_DEPTH);

    gum_mprotect (block.guard, block.guard_size, GUM_PAGE_NO_ACCESS);
  }

  return freed;
}

static gboolean
gum_bounds_checker_on_exception (GumExceptionDetails * details,
                                 gpointer user_data)
{
  GumBoundsChecker * self;
  GumMemoryOperation op;
  gconstpointer address;
  GumBlockDetails block;
  GString * message;
  GumReturnAddressArray accessed_at = { 0, };

  self = GUM_BOUNDS_CHECKER (user_data);

  if (details->type != GUM_EXCEPTION_ACCESS_VIOLATION)
    return FALSE;

  op = details->memory.operation;
  if (op != GUM_MEMOP_READ && op != GUM_MEMOP_WRITE)
    return FALSE;

  address = details->memory.address;

  if (!gum_page_pool_query_block_details (self->page_pool, address, &block))
    return FALSE;

  if (self->handled_invalid_access)
    return FALSE;
  self->handled_invalid_access = TRUE;

  if (self->output == NULL)
    return TRUE;

  message = g_string_sized_new (300);

  g_string_append_printf (message,
      "Oops! %s block %p of %" G_GSIZE_MODIFIER "d bytes"
      " was accessed at offset %" G_GSIZE_MODIFIER "d",
      block.allocated ? "Heap" : "Freed",
      block.address,
      block.size,
      (gsize) ((guint8 *) address - (guint8 *) block.address));

  if (self->backtracer_instance != NULL)
  {
    self->backtracer_iface->generate (self->backtracer_instance, NULL,
        &accessed_at, GUM_MAX_BACKTRACE_DEPTH);
  }

  if (accessed_at.len > 0)
  {
    g_string_append (message, " from:\n");
    gum_bounds_checker_append_backtrace (&accessed_at, message);
  }
  else
  {
    g_string_append_c (message, '\n');
  }

  if (self->backtracer_instance != NULL)
  {
    GumReturnAddressArray * allocated_at, * freed_at;

    gum_mprotect (block.guard, block.guard_size, GUM_PAGE_READ);

    allocated_at = BLOCK_ALLOC_RETADDRS (&block);
    if (allocated_at->len > 0)
    {
      g_string_append (message, "Allocated at:\n");
      gum_bounds_checker_append_backtrace (allocated_at, message);
    }

    freed_at = BLOCK_FREE_RETADDRS (&block);
    if (freed_at->len > 0)
    {
      g_string_append (message, "Freed at:\n");
      gum_bounds_checker_append_backtrace (freed_at, message);
    }

    gum_mprotect (block.guard, block.guard_size, GUM_PAGE_NO_ACCESS);
  }

  self->output (message->str, self->output_user_data);

  g_string_free (message, TRUE);

  return TRUE;
}

static void
gum_bounds_checker_append_backtrace (const GumReturnAddressArray * arr,
                                     GString * s)
{
  guint i;

  for (i = 0; i != arr->len; i++)
  {
    GumReturnAddress addr = arr->items[i];
    GumReturnAddressDetails rad;

    if (gum_return_address_details_from_address (addr, &rad))
    {
      gchar * file_basename;

      file_basename = g_path_get_basename (rad.file_name);
      g_string_append_printf (s, "\t%p %s!%s %s:%u\n",
          rad.address,
          rad.module_name, rad.function_name,
          file_basename, rad.line_number);
      g_free (file_basename);
    }
    else
    {
      g_string_append_printf (s, "\t%p\n", addr);
    }
  }
}
