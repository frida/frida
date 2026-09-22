/*
 * Copyright (C) 2025-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummoduleregistry-elf.h"

#include "guminterceptor.h"
#if defined (HAVE_I386)
# include "gumx86reader.h"
#elif defined (HAVE_ARM64)
# include "gumarm64reader.h"
#endif

static gboolean gum_register_module (GumModule * module, gpointer user_data);
static void gum_hook_rtld_notifier (const GumRtldNotifierDetails * details,
    gpointer user_data);
static void gum_module_registry_on_rtld_notification (GumInvocationContext * ic,
    gpointer user_data);
static void gum_module_registry_synchronize_modules (void);
static gboolean gum_store_module (GumModule * module, gpointer user_data);

static GumModuleRegistry * gum_registry;
static GHashTable * gum_current_modules;
static GumInterceptor * gum_rtld_interceptor;
static GumInvocationListener * gum_rtld_call_handler = NULL;
static GumInvocationListener * gum_rtld_probe_handler = NULL;

void
_gum_module_registry_activate (GumModuleRegistry * self)
{
  gum_registry = self;
  gum_current_modules =
      g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
  gum_rtld_interceptor = gum_interceptor_obtain ();

  _gum_module_registry_enumerate_loaded_modules (gum_register_module, self);

  gum_interceptor_begin_transaction (gum_rtld_interceptor);
  _gum_module_registry_enumerate_rtld_notifiers (gum_hook_rtld_notifier, self);
  gum_interceptor_end_transaction (gum_rtld_interceptor);

  /*
   * Ideally we'd want to hook the notifiers first, and then add the initial
   * modules, given that we're holding the registry lock, so any call to a
   * notifier will end up waiting. However, in the event that there's not enough
   * space in the function prologue, Interceptor/CodeAllocator will have to look
   * for a nearby ELF header it can jump through. And to do that it needs to be
   * able to enumerate the modules, and that's why we need to populate the list
   * first. This means there's a small window where a notifier may have been
   * invoked, but we didn't catch it. So we need to synchronize our modules here
   * just in case that happened.
   */
  gum_module_registry_synchronize_modules ();
}

void
_gum_module_registry_deactivate (GumModuleRegistry * self)
{
  GumInvocationListener ** handlers[] = {
    &gum_rtld_probe_handler,
    &gum_rtld_call_handler
  };
  guint i;

  for (i = 0; i != G_N_ELEMENTS (handlers); i++)
  {
    GumInvocationListener ** handler = handlers[i];

    if (*handler != NULL)
    {
      gum_interceptor_detach (gum_rtld_interceptor, *handler);
      g_object_unref (*handler);
      *handler = NULL;
    }
  }

  g_clear_object (&gum_rtld_interceptor);

  g_clear_pointer (&gum_current_modules, g_hash_table_unref);
}

static gboolean
gum_register_module (GumModule * module,
                     gpointer user_data)
{
  GumModuleRegistry * registry = user_data;

  g_hash_table_insert (gum_current_modules,
      GSIZE_TO_POINTER (gum_module_get_range (module)->base_address),
      g_object_ref (module));
  _gum_module_registry_register (registry, module);

  return TRUE;
}

static void
gum_hook_rtld_notifier (const GumRtldNotifierDetails * details,
                        gpointer user_data)
{
  gpointer impl = details->location;
#if defined (HAVE_I386) || defined (HAVE_ARM64)
  G_GNUC_UNUSED cs_insn * first_instruction;
#endif
  gsize offset = 0;
  GumInvocationListener ** handler;
  GumAttachOptions options = { .ignorability = GUM_INVOCATION_UNIGNORABLE };

#if defined (HAVE_I386)
  first_instruction = gum_x86_reader_disassemble_instruction_at (impl);
  if (first_instruction != NULL && first_instruction->id == X86_INS_INT3)
    offset = first_instruction->size;
#elif defined (HAVE_ARM64)
  first_instruction = gum_arm64_reader_disassemble_instruction_at (impl);
  if (first_instruction != NULL && first_instruction->id == ARM64_INS_BRK)
    offset = first_instruction->size;
#endif

  handler = (details->point_cut == GUM_POINT_LEAVE)
      ? &gum_rtld_call_handler
      : &gum_rtld_probe_handler;
  if (*handler == NULL)
  {
    if (details->point_cut == GUM_POINT_LEAVE)
    {
      *handler = gum_make_call_listener (NULL,
          gum_module_registry_on_rtld_notification, NULL, NULL);
    }
    else
    {
      *handler = gum_make_probe_listener (
          gum_module_registry_on_rtld_notification, NULL, NULL);
    }
  }

  if (gum_interceptor_attach (gum_rtld_interceptor, impl + offset, *handler,
        &options) == GUM_ATTACH_WRONG_SIGNATURE)
  {
    options.instrumentation.relocation_policy = GUM_RELOCATION_FORCED;
    gum_interceptor_attach (gum_rtld_interceptor, impl + offset, *handler,
        &options);
  }
}

static void
gum_module_registry_on_rtld_notification (GumInvocationContext * ic,
                                          gpointer user_data)
{
  _gum_module_registry_handle_rtld_notification (
      gum_module_registry_synchronize_modules, ic);
}

static void
gum_module_registry_synchronize_modules (void)
{
  GHashTable * modules;
  GHashTableIter iter;
  gpointer base_address;
  GumModule * module;
  GQueue added = G_QUEUE_INIT;
  GQueue removed = G_QUEUE_INIT;

  modules = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
  _gum_module_registry_enumerate_loaded_modules (gum_store_module, modules);

  gum_module_registry_lock (gum_registry);

  g_hash_table_iter_init (&iter, modules);
  while (g_hash_table_iter_next (&iter, &base_address, (gpointer *) &module))
  {
    if (!g_hash_table_contains (gum_current_modules, base_address))
      g_queue_push_tail (&added, g_object_ref (module));
  }

  g_hash_table_iter_init (&iter, gum_current_modules);
  while (g_hash_table_iter_next (&iter, &base_address, NULL))
  {
    if (!g_hash_table_contains (modules, base_address))
      g_queue_push_tail (&removed, base_address);
  }

  g_hash_table_unref (gum_current_modules);
  gum_current_modules = modules;

  gum_module_registry_unlock (gum_registry);

  while ((base_address = g_queue_pop_head (&removed)) != NULL)
    _gum_module_registry_unregister (gum_registry, GUM_ADDRESS (base_address));

  while ((module = g_queue_pop_head (&added)) != NULL)
  {
    _gum_module_registry_register (gum_registry, module);
    g_object_unref (module);
  }
}

static gboolean
gum_store_module (GumModule * module,
                  gpointer user_data)
{
  GHashTable * modules = user_data;

  g_hash_table_insert (modules,
      GSIZE_TO_POINTER (gum_module_get_range (module)->base_address),
      g_object_ref (module));

  return TRUE;
}
