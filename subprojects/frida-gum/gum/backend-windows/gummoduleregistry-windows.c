/*
 * Copyright (C) 2025-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummoduleregistry-priv.h"

#include "gummodule-windows.h"
#include "gum/guminterceptor.h"

#include <psapi.h>

typedef struct _GumLdrLoadDllInvocation GumLdrLoadDllInvocation;
typedef struct _GumLdrUnloadDllInvocation GumLdrUnloadDllInvocation;

struct _GumLdrLoadDllInvocation
{
  HMODULE * module_handle;
};

struct _GumLdrUnloadDllInvocation
{
  HMODULE module_handle;
};

static void gum_register_existing_modules (GumModuleRegistry * registry);
static void gum_hook_loader (GumModuleRegistry * registry);
static void gum_unhook_loader (void);
static void gum_module_registry_load_dll_on_enter (GumInvocationContext * ic,
    gpointer user_data);
static void gum_module_registry_load_dll_on_leave (GumInvocationContext * ic,
    gpointer user_data);
static void gum_module_registry_unload_dll_on_enter (GumInvocationContext * ic,
    gpointer user_data);
static void gum_module_registry_unload_dll_on_leave (GumInvocationContext * ic,
    gpointer user_data);

static GHashTable * gum_current_modules;

static GumInterceptor * gum_ldr_interceptor;
static GumInvocationListener * gum_load_handler;
static GumInvocationListener * gum_unload_handler;

void
_gum_module_registry_activate (GumModuleRegistry * self)
{
  gum_current_modules = g_hash_table_new (NULL, NULL);

  gum_hook_loader (self);
  gum_register_existing_modules (self);
}

void
_gum_module_registry_deactivate (GumModuleRegistry * self)
{
  gum_unhook_loader ();

  g_clear_pointer (&gum_current_modules, g_hash_table_unref);
}

static void
gum_register_existing_modules (GumModuleRegistry * registry)
{
  HANDLE this_process;
  HMODULE first_module;
  DWORD modules_size = 0;
  HMODULE * modules = NULL;
  guint mod_idx;

  this_process = GetCurrentProcess ();

  if (!EnumProcessModules (this_process, &first_module, sizeof (first_module),
      &modules_size))
    goto beach;

  modules = g_malloc (modules_size);

  if (!EnumProcessModules (this_process, modules, modules_size, &modules_size))
    goto beach;

  gum_module_registry_lock (registry);

  for (mod_idx = 0; mod_idx != modules_size / sizeof (HMODULE); mod_idx++)
  {
    GumNativeModule * module;

    g_hash_table_add (gum_current_modules, modules[mod_idx]);

    module = _gum_native_module_make (modules[mod_idx]);

    _gum_module_registry_register (registry, GUM_MODULE (module));

    g_object_unref (module);
  }

  gum_module_registry_unlock (registry);

beach:
  g_free (modules);
}

static void
gum_hook_loader (GumModuleRegistry * registry)
{
  HMODULE ntdll;
  gpointer load_impl, unload_impl;
  GumAttachOptions options = { .ignorability = GUM_INVOCATION_UNIGNORABLE };

  ntdll = GetModuleHandleW (L"ntdll.dll");
  load_impl = GetProcAddress (ntdll, "LdrLoadDll");
  unload_impl = GetProcAddress (ntdll, "LdrUnloadDll");
  if (load_impl == NULL || unload_impl == NULL)
    g_error ("Unsupported Windows version; please file a bug");

  gum_ldr_interceptor = gum_interceptor_obtain ();
  gum_load_handler = gum_make_call_listener (
      gum_module_registry_load_dll_on_enter,
      gum_module_registry_load_dll_on_leave,
      registry, NULL);
  gum_unload_handler = gum_make_call_listener (
      gum_module_registry_unload_dll_on_enter,
      gum_module_registry_unload_dll_on_leave,
      registry, NULL);

  gum_interceptor_begin_transaction (gum_ldr_interceptor);
  gum_interceptor_attach (gum_ldr_interceptor, load_impl, gum_load_handler,
      &options);
  gum_interceptor_attach (gum_ldr_interceptor, unload_impl, gum_unload_handler,
      &options);
  gum_interceptor_end_transaction (gum_ldr_interceptor);
}

static void
gum_unhook_loader (void)
{
  if (gum_load_handler == NULL)
    return;

  gum_interceptor_begin_transaction (gum_ldr_interceptor);
  gum_interceptor_detach (gum_ldr_interceptor, gum_unload_handler);
  gum_interceptor_detach (gum_ldr_interceptor, gum_load_handler);
  gum_interceptor_end_transaction (gum_ldr_interceptor);

  g_object_unref (gum_unload_handler);
  g_object_unref (gum_load_handler);
  g_object_unref (gum_ldr_interceptor);
  gum_unload_handler = NULL;
  gum_load_handler = NULL;
  gum_ldr_interceptor = NULL;
}

static void
gum_module_registry_load_dll_on_enter (GumInvocationContext * ic,
                                       gpointer user_data)
{
  GumLdrLoadDllInvocation * invocation =
      GUM_IC_GET_INVOCATION_DATA (ic, GumLdrLoadDllInvocation);
  invocation->module_handle = gum_invocation_context_get_nth_argument (ic, 3);
}

static void
gum_module_registry_load_dll_on_leave (GumInvocationContext * ic,
                                       gpointer user_data)
{
  GumModuleRegistry * self = user_data;
  NTSTATUS status;
  GumLdrLoadDllInvocation * invocation;
  HMODULE handle;

  status = GPOINTER_TO_SIZE (gum_invocation_context_get_return_value (ic));
  if (status != 0)
    return;

  invocation = GUM_IC_GET_INVOCATION_DATA (ic, GumLdrLoadDllInvocation);

  handle = *invocation->module_handle;

  gum_module_registry_lock (self);

  if (!g_hash_table_contains (gum_current_modules, handle))
  {
    GumNativeModule * module;

    g_hash_table_add (gum_current_modules, handle);

    module = _gum_native_module_make (handle);

    _gum_module_registry_register (self, GUM_MODULE (module));

    g_object_unref (module);
  }

  gum_module_registry_unlock (self);
}

static void
gum_module_registry_unload_dll_on_enter (GumInvocationContext * ic,
                                         gpointer user_data)
{
  GumLdrUnloadDllInvocation * invocation =
      GUM_IC_GET_INVOCATION_DATA (ic, GumLdrUnloadDllInvocation);
  invocation->module_handle = gum_invocation_context_get_nth_argument (ic, 0);
}

static void
gum_module_registry_unload_dll_on_leave (GumInvocationContext * ic,
                                         gpointer user_data)
{
  GumModuleRegistry * self = user_data;
  GumLdrUnloadDllInvocation * invocation;
  HMODULE handle;

  invocation = GUM_IC_GET_INVOCATION_DATA (ic, GumLdrUnloadDllInvocation);

  if (!GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR) invocation->module_handle,
        &handle))
  {
    gum_module_registry_lock (self);

    if (g_hash_table_contains (gum_current_modules, invocation->module_handle))
    {
      g_hash_table_remove (gum_current_modules, invocation->module_handle);
      _gum_module_registry_unregister (self,
          GUM_ADDRESS (invocation->module_handle));
    }

    gum_module_registry_unlock (self);
  }
}
