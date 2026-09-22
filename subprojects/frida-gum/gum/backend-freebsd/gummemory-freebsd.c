/*
 * Copyright (C) 2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2025 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummemory.h"

#include "gummemory-priv.h"
#include "gumprocess-priv.h"
#include "valgrind.h"

#include <sys/mman.h>

typedef struct _GumFindRangeProtContext GumFindRangeProtContext;

struct _GumFindRangeProtContext
{
  GumAddress address;

  gboolean found;
  GumPageProtection protection;
};

static gboolean gum_memory_get_protection (gconstpointer address, gsize n,
    gsize * size, GumPageProtection * prot);
static gboolean gum_store_protection_if_containing_address (
    const GumRangeDetails * details, GumFindRangeProtContext * ctx);

gboolean
gum_memory_is_readable (gconstpointer address,
                        gsize len)
{
  gsize size;
  GumPageProtection prot;

  if (!gum_memory_get_protection (address, len, &size, &prot))
    return FALSE;

  return size >= len && (prot & GUM_PAGE_READ) != 0;
}

static gboolean
gum_memory_is_writable (gconstpointer address,
                        gsize len)
{
  gsize size;
  GumPageProtection prot;

  if (!gum_memory_get_protection (address, len, &size, &prot))
    return FALSE;

  return size >= len && (prot & GUM_PAGE_WRITE) != 0;
}

gboolean
gum_memory_query_protection (gconstpointer address,
                             GumPageProtection * prot)
{
  gsize size;

  if (!gum_memory_get_protection (address, 1, &size, prot))
    return FALSE;

  return size >= 1;
}

guint8 *
gum_memory_read (gconstpointer address,
                 gsize len,
                 gsize * n_bytes_read)
{
  guint8 * result = NULL;
  gsize result_len = 0;
  gsize size;
  GumPageProtection prot;

  if (gum_memory_get_protection (address, len, &size, &prot)
      && (prot & GUM_PAGE_READ) != 0)
  {
    result_len = MIN (len, size);
    result = g_memdup (address, result_len);
  }

  if (n_bytes_read != NULL)
    *n_bytes_read = result_len;

  return result;
}

gboolean
gum_memory_write (gpointer address,
                  const guint8 * bytes,
                  gsize len)
{
  gboolean success = FALSE;

  if (gum_memory_is_writable (address, len))
  {
    memcpy (address, bytes, len);
    success = TRUE;
  }

  return success;
}

gboolean
gum_memory_can_remap_writable (void)
{
  return FALSE;
}

gpointer
gum_memory_try_remap_writable_pages (gpointer first_page,
                                     guint n_pages)
{
  return NULL;
}

void
gum_memory_dispose_writable_pages (gpointer first_page,
                                   guint n_pages)
{
}

gboolean
gum_try_mprotect (gpointer address,
                  gsize size,
                  GumPageProtection prot)
{
  gsize page_size;
  gpointer aligned_address;
  gsize aligned_size;
  gint posix_prot;
  gint result;

  g_assert (size != 0);

  page_size = gum_query_page_size ();
  aligned_address = GSIZE_TO_POINTER (
      GPOINTER_TO_SIZE (address) & ~(page_size - 1));
  aligned_size =
      (1 + ((address + size - 1 - aligned_address) / page_size)) * page_size;
  posix_prot = _gum_page_protection_to_posix (prot);

  result = mprotect (aligned_address, aligned_size, posix_prot);

  return result == 0;
}

void
gum_clear_cache (gpointer address,
                 gsize size)
{
  msync (address, size, MS_INVALIDATE);
  __builtin___clear_cache (address, address + size);

  VALGRIND_DISCARD_TRANSLATIONS (address, size);
}

static gboolean
gum_memory_get_protection (gconstpointer address,
                           gsize n,
                           gsize * size,
                           GumPageProtection * prot)
{
  GumFindRangeProtContext ctx;

  if (size == NULL || prot == NULL)
  {
    gsize ignored_size;
    GumPageProtection ignored_prot;

    return gum_memory_get_protection (address, n,
        (size != NULL) ? size : &ignored_size,
        (prot != NULL) ? prot : &ignored_prot);
  }

  if (n > 1)
  {
    gboolean success;
    gsize page_size, start_page, end_page, cur_page;

    page_size = gum_query_page_size ();

    start_page = GPOINTER_TO_SIZE (address) & ~(page_size - 1);
    end_page = (GPOINTER_TO_SIZE (address) + n - 1) & ~(page_size - 1);

    success = gum_memory_get_protection (GSIZE_TO_POINTER (start_page), 1, NULL,
        prot);
    if (success)
    {
      *size = page_size - (GPOINTER_TO_SIZE (address) - start_page);
      for (cur_page = start_page + page_size;
          cur_page != end_page + page_size;
          cur_page += page_size)
      {
        GumPageProtection cur_prot;

        if (gum_memory_get_protection (GSIZE_TO_POINTER (cur_page), 1, NULL,
            &cur_prot) && (cur_prot != GUM_PAGE_NO_ACCESS ||
            *prot == GUM_PAGE_NO_ACCESS))
        {
          *size += page_size;
          *prot &= cur_prot;
        }
        else
        {
          break;
        }
      }
      *size = MIN (*size, n);
    }

    return success;
  }

  ctx.address = GUM_ADDRESS (address);
  ctx.found = FALSE;

  _gum_process_enumerate_ranges (GUM_PAGE_NO_ACCESS,
      (GumFoundRangeFunc) gum_store_protection_if_containing_address, &ctx);

  if (ctx.found)
  {
    *size = 1;
    *prot = ctx.protection;
  }

  return ctx.found;
}

static gboolean
gum_store_protection_if_containing_address (const GumRangeDetails * details,
                                            GumFindRangeProtContext * ctx)
{
  gboolean proceed = TRUE;

  if (GUM_MEMORY_RANGE_INCLUDES (details->range, ctx->address))
  {
    ctx->found = TRUE;
    ctx->protection = details->protection;

    proceed = FALSE;
  }

  return proceed;
}
