/*
 * Copyright (C) 2025-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2026 Sam Sun <samsun@nvidia.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummoduleregistry.h"

#include "gum-init.h"
#include "gumcloak.h"
#include "guminterceptor-priv.h"
#include "gummoduleregistry-priv.h"

#define GUM_MODULE_REGISTRY_LOCK(r) g_rec_mutex_lock (&(r)->mutex)
#define GUM_MODULE_REGISTRY_UNLOCK(r) g_rec_mutex_unlock (&(r)->mutex)

typedef enum {
  GUM_MODULE_REGISTRY_CREATED,
  GUM_MODULE_REGISTRY_ACTIVATED,
} GumModuleRegistryState;

struct _GumModuleRegistry
{
  GObject parent;

  GRecMutex mutex;
  GumModuleRegistryState state;
  GPtrArray * modules;
};

enum
{
  MODULE_ADDED,
  MODULE_REMOVED,
  LAST_SIGNAL
};

static void gum_module_registry_dispose (GObject * object);
static void gum_module_registry_finalize (GObject * object);
static void gum_module_registry_activate (GumModuleRegistry * self);

static void gum_deinit_module_registry (void);
static void gum_deinit_rtld_notifier_offsets (void);

static gboolean gum_is_cloaked_module (GumModule * module);

/**
 * GumModuleRegistry:
 *
 * Low-level registry of the process's loaded modules, emitting signals as
 * libraries are loaded and unloaded.
 *
 * Most code should reach for the higher-level Process API, such as
 * `gum_process_enumerate_modules()`, or a [class@Gum.ModuleMap] snapshot. Use
 * the registry directly when you need to know *when* modules come and go:
 * obtain it with [method@Gum.ModuleRegistry.obtain] and connect to its
 * [signal@Gum.ModuleRegistry::module-added] and
 * [signal@Gum.ModuleRegistry::module-removed] signals. Hold
 * [method@Gum.ModuleRegistry.lock] across a sequence of operations that need a
 * stable view.
 */

/**
 * GumModuleRegistry::module-added:
 * @self: the module registry
 * @module: the module that was loaded
 *
 * Emitted when a module is loaded into the process.
 */

/**
 * GumModuleRegistry::module-removed:
 * @self: the module registry
 * @module: the module that was unloaded
 *
 * Emitted when a module is unloaded from the process.
 */

G_DEFINE_TYPE (GumModuleRegistry, gum_module_registry, G_TYPE_OBJECT)

static guint gum_module_registry_signals[LAST_SIGNAL] = { 0, };

static guint * gum_rtld_notifier_offsets = NULL;
static guint gum_rtld_notifier_n_offsets = 0;

static void
gum_module_registry_class_init (GumModuleRegistryClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_module_registry_dispose;
  object_class->finalize = gum_module_registry_finalize;

  gum_module_registry_signals[MODULE_ADDED] = g_signal_new ("module-added",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, GUM_TYPE_MODULE);
  gum_module_registry_signals[MODULE_REMOVED] = g_signal_new ("module-removed",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, GUM_TYPE_MODULE);
}

static void
gum_module_registry_init (GumModuleRegistry * self)
{
  g_rec_mutex_init (&self->mutex);
  self->state = GUM_MODULE_REGISTRY_CREATED;
  self->modules = g_ptr_array_new_full (0, g_object_unref);
}

static void
gum_module_registry_dispose (GObject * object)
{
  GumModuleRegistry * self = GUM_MODULE_REGISTRY (object);

  GUM_MODULE_REGISTRY_LOCK (self);

  _gum_module_registry_deactivate (self);

  g_ptr_array_unref (self->modules);
  self->modules = g_ptr_array_new_full (0, g_object_unref);

  GUM_MODULE_REGISTRY_UNLOCK (self);

  G_OBJECT_CLASS (gum_module_registry_parent_class)->dispose (object);
}

static void
gum_module_registry_finalize (GObject * object)
{
  GumModuleRegistry * self = GUM_MODULE_REGISTRY (object);

  g_ptr_array_unref (self->modules);
  g_rec_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (gum_module_registry_parent_class)->finalize (object);
}

/**
 * gum_module_registry_obtain:
 *
 * Obtains the module registry singleton.
 *
 * Returns: (transfer none): the module registry
 */
GumModuleRegistry *
gum_module_registry_obtain (void)
{
  GumModuleRegistry * registry;
  static gsize cached_result = 0;
  gboolean activate = FALSE;

  if (g_once_init_enter (&cached_result))
  {
    GumModuleRegistry * registry;

    registry = g_object_new (GUM_TYPE_MODULE_REGISTRY, NULL);
    _gum_register_destructor (gum_deinit_module_registry);

    activate = TRUE;

    g_once_init_leave (&cached_result, GPOINTER_TO_SIZE (registry));
  }

  registry = GSIZE_TO_POINTER (cached_result);

  if (activate)
    gum_module_registry_activate (registry);

  return registry;
}

static void
gum_module_registry_activate (GumModuleRegistry * self)
{
  GUM_MODULE_REGISTRY_LOCK (self);

  _gum_module_registry_activate (self);
  self->state = GUM_MODULE_REGISTRY_ACTIVATED;

  GUM_MODULE_REGISTRY_UNLOCK (self);
}

static void
gum_deinit_module_registry (void)
{
  g_object_unref (gum_module_registry_obtain ());
}

/**
 * gum_module_registry_set_rtld_notifier_offsets:
 * @offsets: (array length=n_offsets) (element-type guint): offsets, relative to
 *           the dynamic linker base, of call sites that invoke the rtld notifier
 * @n_offsets: number of entries in @offsets
 *
 * Overrides the dynamic-linker notifier hook location(s) used when the registry
 * is activated. Instead of hooking the well-known notifier stub, Frida hooks the
 * given call sites, each computed as the dynamic linker base plus an offset.
 *
 * Must be called before the first gum_module_registry_obtain(), as activation
 * (and thus notifier hooking) happens on first obtain.
 */
void
gum_module_registry_set_rtld_notifier_offsets (const guint * offsets,
                                               guint n_offsets)
{
  static gsize registered = FALSE;

  if (g_once_init_enter (&registered))
  {
    _gum_register_destructor (gum_deinit_rtld_notifier_offsets);

    g_once_init_leave (&registered, TRUE);
  }

  g_free (gum_rtld_notifier_offsets);

  gum_rtld_notifier_offsets = (n_offsets != 0)
      ? g_memdup2 (offsets, n_offsets * sizeof (guint))
      : NULL;
  gum_rtld_notifier_n_offsets = n_offsets;
}

static void
gum_deinit_rtld_notifier_offsets (void)
{
  g_free (gum_rtld_notifier_offsets);
}

const guint *
_gum_module_registry_get_rtld_notifier_offsets (guint * n_offsets)
{
  *n_offsets = gum_rtld_notifier_n_offsets;
  return gum_rtld_notifier_offsets;
}

GPtrArray *
_gum_module_registry_get_modules (GumModuleRegistry * self)
{
  GPtrArray * result;

  GUM_MODULE_REGISTRY_LOCK (self);

  result = g_ptr_array_ref (self->modules);

  GUM_MODULE_REGISTRY_UNLOCK (self);

  return result;
}

/**
 * gum_module_registry_enumerate_modules:
 * @self: module registry
 * @func: (scope call): function called with each #GumModule
 * @user_data: data to pass to @func
 *
 * Enumerates all registered modules.
 */
void
gum_module_registry_enumerate_modules (GumModuleRegistry * self,
                                       GumFoundModuleFunc func,
                                       gpointer user_data)
{
  guint n, i;

  GUM_MODULE_REGISTRY_LOCK (self);

  n = self->modules->len;
  for (i = 0; i != n; i++)
  {
    GumModule * mod = g_ptr_array_index (self->modules, i);

    if (gum_is_cloaked_module (mod))
      continue;

    if (!func (mod, user_data))
      break;
  }

  GUM_MODULE_REGISTRY_UNLOCK (self);
}

/**
 * gum_module_registry_lock:
 * @self: module registry
 *
 * Acquires the registry lock, preventing concurrent modification while held.
 * Use this to keep the module set stable across several operations, for example
 * while holding on to a #GumModule obtained from it. Balance with
 * [method@Gum.ModuleRegistry.unlock]. Enumeration already takes the lock
 * internally.
 */
void
gum_module_registry_lock (GumModuleRegistry * self)
{
  GUM_MODULE_REGISTRY_LOCK (self);
}

/**
 * gum_module_registry_unlock:
 * @self: module registry
 *
 * Releases the registry lock acquired with [method@Gum.ModuleRegistry.lock].
 */
void
gum_module_registry_unlock (GumModuleRegistry * self)
{
  GUM_MODULE_REGISTRY_UNLOCK (self);
}

void
_gum_module_registry_reset (GumModuleRegistry * self)
{
  GUM_MODULE_REGISTRY_LOCK (self);

  g_ptr_array_remove_range (self->modules, 0, self->modules->len);

  GUM_MODULE_REGISTRY_UNLOCK (self);
}

void
_gum_module_registry_register (GumModuleRegistry * self,
                               GumModule * mod)
{
  gboolean being_observed;
  GPtrArray * modules;

  GUM_MODULE_REGISTRY_LOCK (self);

  being_observed = self->state != GUM_MODULE_REGISTRY_CREATED;

  modules = being_observed
      ? g_ptr_array_copy (self->modules, (GCopyFunc) g_object_ref, NULL)
      : self->modules;
  g_ptr_array_add (modules, g_object_ref (mod));

  if (being_observed)
  {
    g_ptr_array_unref (self->modules);
    self->modules = modules;
  }

  GUM_MODULE_REGISTRY_UNLOCK (self);

  if (being_observed && !gum_is_cloaked_module (mod))
    g_signal_emit (self, gum_module_registry_signals[MODULE_ADDED], 0, mod);
}

void
_gum_module_registry_unregister (GumModuleRegistry * self,
                                 GumAddress base_address)
{
  gboolean being_observed;
  GPtrArray * modules;
  GumModule * mod;
  guint i;

  GUM_MODULE_REGISTRY_LOCK (self);

  being_observed = self->state != GUM_MODULE_REGISTRY_CREATED;

  modules = being_observed
      ? g_ptr_array_copy (self->modules, (GCopyFunc) g_object_ref, NULL)
      : self->modules;

  mod = NULL;
  for (i = 0; i != self->modules->len; i++)
  {
    GumModule * candidate = g_ptr_array_index (self->modules, i);

    if (gum_module_get_range (candidate)->base_address == base_address)
    {
      mod = g_object_ref (candidate);
      g_ptr_array_remove_index (modules, i);
      break;
    }
  }
  g_assert (mod != NULL);

  if (being_observed)
  {
    g_ptr_array_unref (self->modules);
    self->modules = modules;
  }

  GUM_MODULE_REGISTRY_UNLOCK (self);

  _gum_interceptor_forget_all_hooks_in_range (gum_module_get_range (mod));

  if (being_observed && !gum_is_cloaked_module (mod))
    g_signal_emit (self, gum_module_registry_signals[MODULE_REMOVED], 0, mod);

  g_object_unref (mod);
}

static gboolean
gum_is_cloaked_module (GumModule * module)
{
  const GumMemoryRange * range;

  range = gum_module_get_range (module);

  return gum_cloak_has_range_containing (range->base_address);
}
