/*
 * Copyright (C) 2008-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2008 Christian Berentsen <jc.berentsen@gmail.com>
 * Copyright (C) 2025 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_MEMORY_H__
#define __GUM_MEMORY_H__

#include <gum/gumdefs.h>

#define GUM_TYPE_MATCH_PATTERN (gum_match_pattern_get_type ())
#define GUM_TYPE_MEMORY_RANGE (gum_memory_range_get_type ())
#define GUM_MEMORY_RANGE_INCLUDES(r, a) ((a) >= (r)->base_address && \
    (a) < ((r)->base_address + (r)->size))

#define GUM_PAGE_RW ((GumPageProtection) (GUM_PAGE_READ | GUM_PAGE_WRITE))
#define GUM_PAGE_RX ((GumPageProtection) (GUM_PAGE_READ | GUM_PAGE_EXECUTE))
#define GUM_PAGE_RWX ((GumPageProtection) (GUM_PAGE_READ | GUM_PAGE_WRITE | \
    GUM_PAGE_EXECUTE))

G_BEGIN_DECLS

typedef guint GumPtrauthSupport;
typedef guint GumRwxSupport;
typedef guint GumMemoryOperation;
typedef guint GumPageProtection;
typedef struct _GumAddressSpec GumAddressSpec;
typedef struct _GumRangeDetails GumRangeDetails;
typedef struct _GumMemoryRange GumMemoryRange;
typedef struct _GumFileMapping GumFileMapping;
typedef struct _GumMatchPattern GumMatchPattern;
typedef struct _GumPointerMatch GumPointerMatch;

typedef gboolean (* GumMemoryIsNearFunc) (gpointer memory, gpointer address);

enum _GumPtrauthSupport
{
  GUM_PTRAUTH_INVALID,
  GUM_PTRAUTH_UNSUPPORTED,
  GUM_PTRAUTH_SUPPORTED
};

enum _GumRwxSupport
{
  GUM_RWX_NONE,
  GUM_RWX_ALLOCATIONS_ONLY,
  GUM_RWX_FULL
};

enum _GumMemoryOperation
{
  GUM_MEMOP_INVALID,
  GUM_MEMOP_READ,
  GUM_MEMOP_WRITE,
  GUM_MEMOP_EXECUTE
};

enum _GumPageProtection
{
  GUM_PAGE_NO_ACCESS = 0,
  GUM_PAGE_READ      = (1 << 0),
  GUM_PAGE_WRITE     = (1 << 1),
  GUM_PAGE_EXECUTE   = (1 << 2),
};

struct _GumAddressSpec
{
  gpointer near_address;
  gsize max_distance;
};

struct _GumRangeDetails
{
  const GumMemoryRange * range;
  GumPageProtection protection;
  const GumFileMapping * file;
};

struct _GumMemoryRange
{
  GumAddress base_address;
  gsize size;
};

struct _GumFileMapping
{
  const gchar * path;
  guint64 offset;
  gsize size;
};

struct _GumPointerMatch
{
  GumAddress address;
  gsize value;
};

typedef gboolean (* GumFoundRangeFunc) (const GumRangeDetails * details,
    gpointer user_data);
typedef void (* GumMemoryPatchApplyFunc) (gpointer mem, gpointer user_data);
typedef void (* GumMemoryPatchPagesApplyFunc) (gpointer mem,
    gpointer target_page, guint n_pages, gpointer user_data);
typedef gboolean (* GumMemoryScanMatchFunc) (GumAddress address, gsize size,
    gpointer user_data);

GUM_API void gum_internal_heap_ref (void);
GUM_API void gum_internal_heap_unref (void);

GUM_API gpointer gum_sign_code_pointer (gpointer value);
GUM_API gpointer gum_strip_code_pointer (gpointer value);
GUM_API GumAddress gum_sign_code_address (GumAddress value);
GUM_API GumAddress gum_strip_code_address (GumAddress value);
GUM_API GumPtrauthSupport gum_query_ptrauth_support (void);
GUM_API guint gum_query_page_size (void);
GUM_API gboolean gum_query_is_rwx_supported (void);
GUM_API GumRwxSupport gum_query_rwx_support (void);
GUM_API gboolean gum_memory_is_readable (gconstpointer address, gsize len);
GUM_API gboolean gum_memory_query_protection (gconstpointer address,
    GumPageProtection * prot);
GUM_API guint8 * gum_memory_read (gconstpointer address, gsize len,
    gsize * n_bytes_read);
GUM_API gboolean gum_memory_write (gpointer address, const guint8 * bytes,
    gsize len);
GUM_API gboolean gum_memory_patch_code (gpointer address, gsize size,
    GumMemoryPatchApplyFunc apply, gpointer apply_data);
GUM_API gboolean gum_memory_patch_code_pages (GPtrArray * sorted_addresses,
    gboolean coalesce, GumMemoryPatchPagesApplyFunc apply,
    gpointer apply_data);
GUM_API gboolean gum_memory_can_remap_writable (void);
GUM_API gpointer gum_memory_try_remap_writable_pages (gpointer first_page,
    guint n_pages);
GUM_API void gum_memory_dispose_writable_pages (gpointer first_page,
    guint n_pages);
GUM_API gboolean gum_memory_mark_code (gpointer address, gsize size);

GUM_API void gum_memory_scan (const GumMemoryRange * range,
    const GumMatchPattern * pattern, GumMemoryScanMatchFunc func,
    gpointer user_data);
GUM_API GArray * gum_memory_find_pointers (const GumMemoryRange * ranges,
    guint n_ranges, const gsize * values, guint n_values, gsize mask);

GUM_API GType gum_match_pattern_get_type (void) G_GNUC_CONST;
GUM_API GumMatchPattern * gum_match_pattern_new_from_string (
    const gchar * pattern_str);
GUM_API GumMatchPattern * gum_match_pattern_ref (GumMatchPattern * pattern);
GUM_API void gum_match_pattern_unref (GumMatchPattern * pattern);
GUM_API guint gum_match_pattern_get_size (const GumMatchPattern * pattern);
GUM_API GPtrArray * gum_match_pattern_get_tokens (
    const GumMatchPattern * pattern);

GUM_API void gum_ensure_code_readable (gconstpointer address, gsize size);

GUM_API void gum_mprotect (gpointer address, gsize size,
    GumPageProtection prot);
GUM_API gboolean gum_try_mprotect (gpointer address, gsize size,
    GumPageProtection prot);

GUM_API void gum_clear_cache (gpointer address, gsize size);

#define gum_new(struct_type, n_structs) \
    ((struct_type *) gum_malloc (n_structs * sizeof (struct_type)))
#define gum_new0(struct_type, n_structs) \
    ((struct_type *) gum_malloc0 (n_structs * sizeof (struct_type)))

GUM_API guint gum_peek_private_memory_usage (void);

GUM_API gpointer gum_malloc (gsize size);
GUM_API gpointer gum_malloc0 (gsize size);
GUM_API gsize gum_malloc_usable_size (gconstpointer mem);
GUM_API gpointer gum_calloc (gsize count, gsize size);
GUM_API gpointer gum_realloc (gpointer mem, gsize size);
GUM_API gpointer gum_memalign (gsize alignment, gsize size);
GUM_API gpointer gum_memdup (gconstpointer mem, gsize byte_size);
GUM_API void gum_free (gpointer mem);

GUM_API gpointer gum_memory_allocate (gpointer address, gsize size,
    gsize alignment, GumPageProtection prot);
GUM_API gpointer gum_memory_allocate_near (const GumAddressSpec * spec,
    gsize size, gsize alignment, GumPageProtection prot);
GUM_API gpointer gum_memory_allocate_bookkeeping (gsize size, gsize alignment);
GUM_API gboolean gum_memory_free (gpointer address, gsize size);
GUM_API gboolean gum_memory_release (gpointer address, gsize size);
GUM_API gboolean gum_memory_recommit (gpointer address, gsize size,
    GumPageProtection prot);
GUM_API gboolean gum_memory_discard (gpointer address, gsize size);
GUM_API gboolean gum_memory_decommit (gpointer address, gsize size);

GUM_API gboolean gum_address_spec_is_satisfied_by (const GumAddressSpec * spec,
    gconstpointer address);

GUM_API GType gum_memory_range_get_type (void) G_GNUC_CONST;
GUM_API GumMemoryRange * gum_memory_range_copy (const GumMemoryRange * range);
GUM_API void gum_memory_range_free (GumMemoryRange * range);

G_END_DECLS

#endif
