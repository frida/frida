/*
 * Copyright (C) 2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumscriptapi.h"

#include "gum-init.h"
#include "gumscriptapi-priv.h"

#define GUM_SCRIPT_API_REGISTRY_LOCK(r) g_mutex_lock (&(r)->mutex)
#define GUM_SCRIPT_API_REGISTRY_UNLOCK(r) g_mutex_unlock (&(r)->mutex)

struct _GumScriptApiRegistry
{
  GObject parent;

  GMutex mutex;
  GPtrArray * apis;
};

struct _GumScriptApi
{
  gint ref_count;

  gchar * name;
  GArray * functions;
  gchar * prelude;
};

static void gum_script_api_registry_finalize (GObject * object);
static void gum_deinit_script_api_registry (void);

static void gum_script_api_free (GumScriptApi * self);
static void gum_script_api_clear_function (GumScriptApiFunction * function);
static GumScriptApiType * gum_script_api_types_from_spec (const gchar * spec,
    guint * length);
static GumScriptApiType gum_script_api_type_from_char (gchar c);

G_DEFINE_TYPE (GumScriptApiRegistry, gum_script_api_registry, G_TYPE_OBJECT)

G_DEFINE_BOXED_TYPE (GumScriptApi, gum_script_api, gum_script_api_ref,
    gum_script_api_unref)

static void
gum_script_api_registry_class_init (GumScriptApiRegistryClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gum_script_api_registry_finalize;
}

static void
gum_script_api_registry_init (GumScriptApiRegistry * self)
{
  g_mutex_init (&self->mutex);
  self->apis = g_ptr_array_new_full (0, (GDestroyNotify) gum_script_api_unref);
}

static void
gum_script_api_registry_finalize (GObject * object)
{
  GumScriptApiRegistry * self = GUM_SCRIPT_API_REGISTRY (object);

  g_ptr_array_unref (self->apis);
  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (gum_script_api_registry_parent_class)->finalize (object);
}

/**
 * gum_script_api_registry_obtain:
 *
 * Obtains the script API registry singleton, which holds the namespaces that
 * every script created from here on gets to see, in every runtime.
 *
 * Returns: (transfer none): the script API registry
 */
GumScriptApiRegistry *
gum_script_api_registry_obtain (void)
{
  static gsize cached_result = 0;

  if (g_once_init_enter (&cached_result))
  {
    GumScriptApiRegistry * registry;

    registry = g_object_new (GUM_TYPE_SCRIPT_API_REGISTRY, NULL);
    _gum_register_destructor (gum_deinit_script_api_registry);

    g_once_init_leave (&cached_result, GPOINTER_TO_SIZE (registry));
  }

  return GSIZE_TO_POINTER (cached_result);
}

static void
gum_deinit_script_api_registry (void)
{
  g_object_unref (gum_script_api_registry_obtain ());
}

/**
 * gum_script_api_registry_add:
 * @self: a registry
 * @api: (transfer none): namespace to expose to scripts
 *
 * Adds a namespace, which scripts created after this point will see.
 */
void
gum_script_api_registry_add (GumScriptApiRegistry * self,
                             GumScriptApi * api)
{
  GUM_SCRIPT_API_REGISTRY_LOCK (self);

  g_ptr_array_add (self->apis, gum_script_api_ref (api));

  GUM_SCRIPT_API_REGISTRY_UNLOCK (self);
}

/**
 * gum_script_api_registry_remove:
 * @self: a registry
 * @api: (transfer none): namespace to stop exposing
 *
 * Removes a namespace. Scripts that already have it keep it.
 */
void
gum_script_api_registry_remove (GumScriptApiRegistry * self,
                                GumScriptApi * api)
{
  GUM_SCRIPT_API_REGISTRY_LOCK (self);

  g_ptr_array_remove (self->apis, api);

  GUM_SCRIPT_API_REGISTRY_UNLOCK (self);
}

GPtrArray *
_gum_script_api_registry_snapshot (GumScriptApiRegistry * self)
{
  GPtrArray * apis;
  guint i;

  apis = g_ptr_array_new_full (0, (GDestroyNotify) gum_script_api_unref);

  GUM_SCRIPT_API_REGISTRY_LOCK (self);

  for (i = 0; i != self->apis->len; i++)
    g_ptr_array_add (apis, gum_script_api_ref (g_ptr_array_index (self->apis, i)));

  GUM_SCRIPT_API_REGISTRY_UNLOCK (self);

  return apis;
}

/**
 * gum_script_api_new:
 * @name: name of the namespace to expose, e.g. `Btf`
 *
 * Creates a namespace of native functions that scripts can call. Add functions
 * to it with [method@Gum.ScriptApi.add_function], then hand it to
 * [method@Gum.ScriptApiRegistry.add] to make it visible to scripts.
 *
 * Returns: (transfer full): the newly created namespace
 */
GumScriptApi *
gum_script_api_new (const gchar * name)
{
  GumScriptApi * api;

  api = g_slice_new (GumScriptApi);
  api->ref_count = 1;
  api->name = g_strdup (name);
  api->functions = g_array_new (FALSE, FALSE, sizeof (GumScriptApiFunction));
  g_array_set_clear_func (api->functions,
      (GDestroyNotify) gum_script_api_clear_function);
  api->prelude = NULL;

  return api;
}

GumScriptApi *
gum_script_api_ref (GumScriptApi * api)
{
  g_atomic_int_inc (&api->ref_count);

  return api;
}

void
gum_script_api_unref (GumScriptApi * api)
{
  if (g_atomic_int_dec_and_test (&api->ref_count))
    gum_script_api_free (api);
}

static void
gum_script_api_free (GumScriptApi * self)
{
  g_free (self->prelude);
  g_array_unref (self->functions);
  g_free (self->name);

  g_slice_free (GumScriptApi, self);
}

static void
gum_script_api_clear_function (GumScriptApiFunction * function)
{
  g_free (function->arg_types);
  g_free (function->name);
}

/**
 * gum_script_api_add_function:
 * @self: a namespace
 * @name: name the function is called by, e.g. `fieldOffset`
 * @arg_types: one character per argument: `t` boolean, `i` int, `u` uint,
 *             `q` int64, `Q` uint64, `n` number, `s` string, `p` pointer
 * @retval_type: what the function answers with
 * @func: (scope notified): implementation
 * @user_data: data to pass to @func
 *
 * Adds a function to the namespace.
 */
void
gum_script_api_add_function (GumScriptApi * self,
                             const gchar * name,
                             const gchar * arg_types,
                             GumScriptApiType retval_type,
                             GumScriptApiFunc func,
                             gpointer user_data)
{
  GumScriptApiFunction function;

  function.name = g_strdup (name);
  function.arg_types = gum_script_api_types_from_spec (arg_types,
      &function.arity);
  function.retval_type = retval_type;
  function.func = func;
  function.user_data = user_data;

  g_array_append_val (self->functions, function);
}

/**
 * gum_script_api_set_prelude:
 * @self: a namespace
 * @source: JavaScript run once the namespace is in place
 *
 * Sets JavaScript that runs in every script's global scope right after the
 * namespaces are installed, so an embedder can shape its native functions into
 * whatever reads best. Keep declarations out of the global scope by assigning
 * to the namespace itself.
 */
void
gum_script_api_set_prelude (GumScriptApi * self,
                            const gchar * source)
{
  g_free (self->prelude);
  self->prelude = g_strdup (source);
}

static GumScriptApiType *
gum_script_api_types_from_spec (const gchar * spec,
                                guint * length)
{
  GumScriptApiType * types;
  guint n, i;

  n = (guint) strlen (spec);
  types = g_new (GumScriptApiType, n);

  for (i = 0; i != n; i++)
    types[i] = gum_script_api_type_from_char (spec[i]);

  *length = n;

  return types;
}

static GumScriptApiType
gum_script_api_type_from_char (gchar c)
{
  switch (c)
  {
    case 't': return GUM_SCRIPT_API_BOOLEAN;
    case 'i': return GUM_SCRIPT_API_INT;
    case 'u': return GUM_SCRIPT_API_UINT;
    case 'q': return GUM_SCRIPT_API_INT64;
    case 'Q': return GUM_SCRIPT_API_UINT64;
    case 'n': return GUM_SCRIPT_API_NUMBER;
    case 's': return GUM_SCRIPT_API_STRING;
    case 'p': return GUM_SCRIPT_API_POINTER;
    default:  g_assert_not_reached ();
  }
}

const gchar *
_gum_script_api_get_name (GumScriptApi * self)
{
  return self->name;
}

GArray *
_gum_script_api_peek_functions (GumScriptApi * self)
{
  return self->functions;
}

const gchar *
_gum_script_api_get_prelude (GumScriptApi * self)
{
  return self->prelude;
}
