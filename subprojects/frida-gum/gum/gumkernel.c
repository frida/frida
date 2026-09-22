/*
 * Copyright (C) 2015-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumkernel.h"

/**
 * gum_kernel_scan:
 * @range: the #GumMemoryRange to scan
 * @pattern: the #GumMatchPattern to look for occurrences of
 * @func: (scope call): function to process each match
 * @user_data: data to pass to @func
 *
 * Scans the specified kernel memory @range for occurrences of @pattern,
 * calling @func with each match.
 */

/**
 * gum_kernel_enumerate_ranges:
 * @prot: bitfield specifying the minimum protection
 * @func: (scope call): function called with #GumRangeDetails
 * @user_data: data to pass to @func
 *
 * Enumerates kernel memory ranges satisfying @prot, calling @func with
 * #GumRangeDetails about each such range found.
 */

/**
 * gum_kernel_enumerate_module_ranges:
 * @module_name: (nullable): name of module, or %NULL for the kernel itself
 * @prot: bitfield specifying the minimum protection
 * @func: (scope call): function called with #GumKernelModuleRangeDetails
 * @user_data: data to pass to @func
 *
 * Enumerates kernel memory ranges of the specified module that satisfy @prot,
 * calling @func with #GumKernelModuleRangeDetails about each such range found.
 */

/**
 * gum_kernel_enumerate_modules:
 * @func: (scope call): function called with #GumModuleDetails
 * @user_data: data to pass to @func
 *
 * Enumerates kernel modules loaded right now, calling @func with
 * #GumModuleDetails about each module found.
 */

#ifndef HAVE_DARWIN

gboolean
gum_kernel_api_is_available (void)
{
  return FALSE;
}

guint
gum_kernel_query_page_size (void)
{
  return 0;
}

GumAddress
gum_kernel_alloc_n_pages (guint n_pages)
{
  return 0;
}

void
gum_kernel_free_pages (GumAddress mem)
{
}

gboolean
gum_kernel_try_mprotect (GumAddress address,
                         gsize size,
                         GumPageProtection prot)
{
  return FALSE;
}

guint8 *
gum_kernel_read (GumAddress address,
                 gsize len,
                 gsize * n_bytes_read)
{
  return NULL;
}

gboolean
gum_kernel_write (GumAddress address,
                  const guint8 * bytes,
                  gsize len)
{
  return FALSE;
}

void
gum_kernel_scan (const GumMemoryRange * range,
                 const GumMatchPattern * pattern,
                 GumMemoryScanMatchFunc func,
                 gpointer user_data)
{
}

void
gum_kernel_enumerate_ranges (GumPageProtection prot,
                             GumFoundRangeFunc func,
                             gpointer user_data)
{
}

void
gum_kernel_enumerate_module_ranges (const gchar * module_name,
                                    GumPageProtection prot,
                                    GumFoundKernelModuleRangeFunc func,
                                    gpointer user_data)
{
}

void
gum_kernel_enumerate_modules (GumFoundKernelModuleFunc func,
                              gpointer user_data)
{
}

GumAddress
gum_kernel_find_base_address (void)
{
  return 0;
}

void
gum_kernel_set_base_address (GumAddress base)
{
}

#endif

