/*
 * Copyright (C) 2015-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_KERNEL_H__
#define __GUM_KERNEL_H__

#include <gum/gumprocess.h>

G_BEGIN_DECLS

typedef struct _GumKernelModuleRangeDetails GumKernelModuleRangeDetails;
typedef struct _GumKernelModuleDetails GumKernelModuleDetails;

struct _GumKernelModuleRangeDetails
{
  gchar name[48];
  GumAddress address;
  guint64 size;
  GumPageProtection protection;
};

struct _GumKernelModuleDetails
{
  const gchar * name;
  const GumMemoryRange * range;
  const gchar * path;
};

typedef gboolean (* GumFoundKernelModuleRangeFunc) (
    const GumKernelModuleRangeDetails * details, gpointer user_data);
typedef gboolean (* GumFoundKernelModuleFunc) (
    const GumKernelModuleDetails * details, gpointer user_data);

GUM_API gboolean gum_kernel_api_is_available (void);
GUM_API guint gum_kernel_query_page_size (void);
GUM_API GumAddress gum_kernel_alloc_n_pages (guint n_pages);
GUM_API void gum_kernel_free_pages (GumAddress mem);
GUM_API gboolean gum_kernel_try_mprotect (GumAddress address, gsize size,
    GumPageProtection prot);
GUM_API guint8 * gum_kernel_read (GumAddress address, gsize len,
    gsize * n_bytes_read);
GUM_API gboolean gum_kernel_write (GumAddress address, const guint8 * bytes,
    gsize len);
GUM_API void gum_kernel_scan (const GumMemoryRange * range,
    const GumMatchPattern * pattern, GumMemoryScanMatchFunc func,
    gpointer user_data);
GUM_API void gum_kernel_enumerate_ranges (GumPageProtection prot,
    GumFoundRangeFunc func, gpointer user_data);
GUM_API void gum_kernel_enumerate_module_ranges (const gchar * module_name,
    GumPageProtection prot, GumFoundKernelModuleRangeFunc func,
    gpointer user_data);
GUM_API void gum_kernel_enumerate_modules (GumFoundKernelModuleFunc func,
    gpointer user_data);
GUM_API GumAddress gum_kernel_find_base_address (void);
GUM_API void gum_kernel_set_base_address (GumAddress base);

G_END_DECLS

#endif
