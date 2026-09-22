/*
 * Copyright (C) 2010-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2020 Matt Oh <oh.jeongwook@gmail.com>
 * Copyright (C) 2024 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumsymbolutil.h"

#include "gum-init.h"
#include "gum/gumdarwinsymbolicator.h"

#include <mach-o/dyld.h>

#include <capstone.h>
#if defined (HAVE_I386)
# include "gumx86reader.h"
#elif defined (HAVE_ARM64)
# include "gumarm64reader.h"
#endif

static void do_deinit (void);

static GArray * gum_pointer_array_new_empty (void);
static GArray * gum_pointer_array_new_take_addresses (GumAddress * addresses,
    gsize len);

static void gum_clear_symbolicator_object (void);

G_LOCK_DEFINE_STATIC (symbolicator);
static GumDarwinSymbolicator * symbolicator = NULL;

static gulong invalidator_added_handler = 0;
static gulong invalidator_removed_handler = 0;

static GumDarwinSymbolicator *
gum_try_obtain_symbolicator (void)
{
  GumDarwinSymbolicator * result = NULL;

  G_LOCK (symbolicator);

  if (symbolicator == NULL)
  {
    symbolicator =
        gum_darwin_symbolicator_new_with_task (mach_task_self (), NULL);
  }

  if (invalidator_added_handler == 0)
  {
    GumModuleRegistry * registry;

    g_assert (invalidator_removed_handler == 0);
    registry = gum_module_registry_obtain ();

    gum_module_registry_lock (registry);

    invalidator_added_handler = g_signal_connect (registry, "module-added",
        G_CALLBACK ((GClosureNotify) gum_clear_symbolicator_object), NULL);
    invalidator_removed_handler = g_signal_connect (registry, "module-removed",
        G_CALLBACK ((GClosureNotify) gum_clear_symbolicator_object), NULL);

    gum_module_registry_unlock (registry);

    _gum_register_early_destructor (do_deinit);
  }

  if (symbolicator != NULL)
    result = g_object_ref (symbolicator);

  G_UNLOCK (symbolicator);

  return result;
}

static void
do_deinit (void)
{
  G_LOCK (symbolicator);

  g_clear_object (&symbolicator);

  if (invalidator_added_handler != 0)
  {
    GumModuleRegistry * registry;

    g_assert (invalidator_removed_handler != 0);
    registry = gum_module_registry_obtain ();

    gum_module_registry_lock (registry);

    g_signal_handler_disconnect (registry, invalidator_added_handler);
    invalidator_added_handler = 0;

    g_signal_handler_disconnect (registry, invalidator_removed_handler);
    invalidator_removed_handler = 0;

    gum_module_registry_unlock (registry);
  }

  G_UNLOCK (symbolicator);
}

gboolean
gum_symbol_details_from_address (gpointer address,
                                 GumDebugSymbolDetails * details)
{
  gboolean success;
  GumDarwinSymbolicator * symbolicator;

  if ((symbolicator = gum_try_obtain_symbolicator ()) == NULL)
    return FALSE;

  success = gum_darwin_symbolicator_details_from_address (symbolicator,
      GUM_ADDRESS (address), details);

  g_object_unref (symbolicator);

  return success;
}

gchar *
gum_symbol_name_from_address (gpointer address)
{
  gchar * name;
  GumDarwinSymbolicator * symbolicator;

  if ((symbolicator = gum_try_obtain_symbolicator ()) == NULL)
    return NULL;

  name = gum_darwin_symbolicator_name_from_address (symbolicator,
      GUM_ADDRESS (address));

  g_object_unref (symbolicator);

  return name;
}

gpointer
gum_find_function (const gchar * name)
{
  gpointer address;
  GumDarwinSymbolicator * symbolicator;

  if ((symbolicator = gum_try_obtain_symbolicator ()) == NULL)
    return NULL;

  address = GSIZE_TO_POINTER (
      gum_darwin_symbolicator_find_function (symbolicator, name));

  g_object_unref (symbolicator);

  return address;
}

GArray *
gum_find_functions_named (const gchar * name)
{
  GumDarwinSymbolicator * symbolicator;
  GumAddress * addresses;
  gsize len;

  if ((symbolicator = gum_try_obtain_symbolicator ()) == NULL)
    return gum_pointer_array_new_empty ();

  addresses =
      gum_darwin_symbolicator_find_functions_named (symbolicator, name, &len);

  g_object_unref (symbolicator);

  return gum_pointer_array_new_take_addresses (addresses, len);
}

GArray *
gum_find_functions_matching (const gchar * str)
{
  GumDarwinSymbolicator * symbolicator;
  GumAddress * addresses;
  gsize len;

  if ((symbolicator = gum_try_obtain_symbolicator ()) == NULL)
    return gum_pointer_array_new_empty ();

  addresses =
      gum_darwin_symbolicator_find_functions_matching (symbolicator, str, &len);

  g_object_unref (symbolicator);

  return gum_pointer_array_new_take_addresses (addresses, len);
}

gboolean
gum_load_symbols (const gchar * path)
{
  return FALSE;
}

static GArray *
gum_pointer_array_new_empty (void)
{
  return g_array_new (FALSE, FALSE, sizeof (gpointer));
}

static GArray *
gum_pointer_array_new_take_addresses (GumAddress * addresses,
                                      gsize len)
{
  GArray * result;
  gsize i;

  result = g_array_sized_new (FALSE, FALSE, sizeof (gpointer), len);

  for (i = 0; i != len; i++)
  {
    gpointer address = GSIZE_TO_POINTER (addresses[i]);
    g_array_append_val (result, address);
  }

  g_free (addresses);

  return result;
}

static void
gum_clear_symbolicator_object (void)
{
  G_LOCK (symbolicator);

  g_clear_object (&symbolicator);

  G_UNLOCK (symbolicator);
}
