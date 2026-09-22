/*
 * Copyright (C) 2025-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2025 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummemory.h"

#include "gumexceptor.h"
#include "gummemory-priv.h"
#include "gum/gumbarebone.h"

#include <string.h>

void
_gum_memory_backend_init (void)
{
}

void
_gum_memory_backend_deinit (void)
{
}

guint
_gum_memory_backend_query_page_size (void)
{
  return gum_barebone_query_page_size ();
}

G_GNUC_WEAK guint
gum_barebone_query_page_size (void)
{
  G_PANIC_MISSING_IMPLEMENTATION ();
  return 0;
}

gboolean
gum_memory_is_readable (gconstpointer address,
                        gsize len)
{
  GumPageProtection prot;

  if (!gum_memory_query_protection (address, &prot))
    return FALSE;

  return (prot & GUM_PAGE_READ) != 0;
}

G_GNUC_WEAK gboolean
gum_memory_query_protection (gconstpointer address,
                             GumPageProtection * prot)
{
  return FALSE;
}

G_GNUC_WEAK guint8 *
gum_memory_read (gconstpointer address,
                 gsize len,
                 gsize * n_bytes_read)
{
  guint8 * result;
  gsize n = 0;
  guint page_size;
  GumExceptor * exceptor;
  GumExceptorScope scope;

  result = g_malloc (len);
  page_size = gum_query_page_size ();
  exceptor = gum_exceptor_obtain ();

  while (n != len)
  {
    const guint8 * cursor = (const guint8 *) address + n;
    gsize available = page_size - (GPOINTER_TO_SIZE (cursor) & (page_size - 1));
    gsize chunk = MIN (len - n, available);

    if (gum_exceptor_try (exceptor, &scope))
      memcpy (result + n, cursor, chunk);

    if (gum_exceptor_catch (exceptor, &scope))
      break;

    n += chunk;
  }

  g_object_unref (exceptor);

  if (n == 0)
  {
    g_free (result);
    result = NULL;
  }

  if (n_bytes_read != NULL)
    *n_bytes_read = n;

  return result;
}

G_GNUC_WEAK gboolean
gum_memory_write (gpointer address,
                  const guint8 * bytes,
                  gsize len)
{
  gboolean success = FALSE;
  GumExceptor * exceptor;
  GumExceptorScope scope;

  exceptor = gum_exceptor_obtain ();

  if (gum_exceptor_try (exceptor, &scope))
  {
    memcpy (address, bytes, len);
    success = TRUE;
  }

  if (gum_exceptor_catch (exceptor, &scope))
    success = FALSE;

  g_object_unref (exceptor);

  return success;
}

G_GNUC_WEAK gboolean
gum_memory_can_remap_writable (void)
{
  return FALSE;
}

G_GNUC_WEAK gpointer
gum_memory_try_remap_writable_pages (gpointer first_page,
                                     guint n_pages)
{
  return NULL;
}

G_GNUC_WEAK gpointer
gum_barebone_try_remap_writable_pages (gconstpointer * addrs,
                                       guint n_addrs)
{
  return NULL;
}

G_GNUC_WEAK void
gum_memory_dispose_writable_pages (gpointer first_page,
                                   guint n_pages)
{
}

G_GNUC_WEAK gboolean
gum_try_mprotect (gpointer address,
                  gsize size,
                  GumPageProtection prot)
{
  G_PANIC_MISSING_IMPLEMENTATION ();
  return FALSE;
}

G_GNUC_WEAK void
gum_clear_cache (gpointer address,
                 gsize size)
{
  G_PANIC_MISSING_IMPLEMENTATION ();
}

G_GNUC_WEAK gpointer
gum_memory_allocate (gpointer address,
                     gsize size,
                     gsize alignment,
                     GumPageProtection prot)
{
  G_PANIC_MISSING_IMPLEMENTATION ();
  return NULL;
}

G_GNUC_WEAK gpointer
gum_memory_allocate_near (const GumAddressSpec * spec,
                          gsize size,
                          gsize alignment,
                          GumPageProtection prot)
{
  gpointer result;

  result = gum_memory_allocate (NULL, size, alignment, prot);
  if (result == NULL)
    return NULL;
  if (spec == NULL || gum_address_spec_is_satisfied_by (spec, result))
    return result;
  gum_memory_free (result, size);

  return NULL;
}

G_GNUC_WEAK gpointer
gum_memory_allocate_bookkeeping (gsize size,
                                 gsize alignment)
{
  return gum_memory_allocate (NULL, size, alignment, GUM_PAGE_RW);
}

G_GNUC_WEAK gboolean
gum_memory_free (gpointer address,
                 gsize size)
{
  G_PANIC_MISSING_IMPLEMENTATION ();
  return FALSE;
}

G_GNUC_WEAK gboolean
gum_memory_release (gpointer address,
                    gsize size)
{
  return FALSE;
}

G_GNUC_WEAK gboolean
gum_memory_recommit (gpointer address,
                     gsize size,
                     GumPageProtection prot)
{
  return FALSE;
}

G_GNUC_WEAK gboolean
gum_memory_discard (gpointer address,
                    gsize size)
{
  return FALSE;
}

G_GNUC_WEAK gboolean
gum_memory_decommit (gpointer address,
                     gsize size)
{
  return FALSE;
}
