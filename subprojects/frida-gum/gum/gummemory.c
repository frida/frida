/*
 * Copyright (C) 2010-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2021 Abdelrahman Eid <hot3eed@gmail.com>
 * Copyright (C) 2025 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2026 Håvard Sørbø <havard@hsorbo.no>
 * Copyright (C) 2026 Ricardo Marques <marquessricardo@gmail.com>
 * Copyright (C) 2026 IPMegladon <ipmegladon@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummemory.h"

#include "gumcloak-priv.h"
#include "gumcodesegment.h"
#include "gumexceptor.h"
#include "gumlibc.h"
#include "gummemory-priv.h"
#include "gumprocess-priv.h"

#ifdef HAVE_PTRAUTH
# include <ptrauth.h>
#endif
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_ANDROID
# include "gum/gumandroid.h"
#endif
#ifndef GUM_USE_SYSTEM_ALLOC
# ifdef HAVE_DARWIN
#  define DARWIN                   1
# endif
# define MSPACES                   1
# define ONLY_MSPACES              1
# define USE_LOCKS                 1
# define FOOTERS                   0
# define INSECURE                  1
# define NO_MALLINFO               0
# define REALLOC_ZERO_BYTES_FREES  1
# ifdef HAVE_LIBC_MALLINFO
#  include <malloc.h>
#  define STRUCT_MALLINFO_DECLARED 1
# endif
# ifdef _MSC_VER
#  pragma warning (push)
#  pragma warning (disable: 4267 4702)
# endif
# ifdef _GNU_SOURCE
#  undef _GNU_SOURCE
# endif
# include "dlmalloc.c"
# ifdef _MSC_VER
#  pragma warning (pop)
# endif
#endif
#ifdef HAVE_DARWIN
# include "backend-darwin/gumdarwin-priv.h"
# include "gum/gumdarwin.h"
#endif

#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8 && defined (__SSE2__)
# include <emmintrin.h>
# define GUM_HAVE_POINTER_SCAN_SIMD
#elif defined (HAVE_ARM64) && defined (__ARM_NEON)
# include <arm_neon.h>
# define GUM_HAVE_POINTER_SCAN_SIMD
#endif

#ifdef GUM_HAVE_POINTER_SCAN_SIMD
# if defined (HAVE_I386)
typedef __m128i GumScanVec;
#  define GUM_SCAN_VEC_SET1(value) _mm_set1_epi64x (value)
#  define GUM_SCAN_VEC_LOAD(p) _mm_loadu_si128 ((const __m128i *) (p))
#  define GUM_SCAN_VEC_AND(a, b) _mm_and_si128 (a, b)
#  define GUM_SCAN_VEC_OR(a, b) _mm_or_si128 (a, b)
# elif defined (HAVE_ARM64)
typedef uint64x2_t GumScanVec;
#  define GUM_SCAN_VEC_SET1(value) vdupq_n_u64 (value)
#  define GUM_SCAN_VEC_LOAD(p) vld1q_u64 ((const guint64 *) (p))
#  define GUM_SCAN_VEC_AND(a, b) vandq_u64 (a, b)
#  define GUM_SCAN_VEC_OR(a, b) vorrq_u64 (a, b)
# endif
#endif

#define GUM_POINTER_SCAN_TILE_WORDS \
    ((4 * 1024 * 1024) / sizeof (gpointer))
#define GUM_POINTER_SCAN_INLINE_LIMIT GUM_POINTER_SCAN_TILE_WORDS
#define GUM_POINTER_SCAN_MAX_WORKERS 4

typedef struct _GumPatchCodeContext GumPatchCodeContext;
typedef struct _GumPageLump GumPageLump;
typedef struct _GumSuspendOperation GumSuspendOperation;
typedef struct _GumPointerScan GumPointerScan;
typedef struct _GumPointerScanTile GumPointerScanTile;
typedef struct _GumPointerScanTask GumPointerScanTask;

struct _GumMatchPattern
{
  gint ref_count;
  GPtrArray * tokens;
  guint size;
  GRegex * regex;
};

struct _GumPatchCodeContext
{
  gsize page_offset;
  GumMemoryPatchApplyFunc func;
  gpointer user_data;
};

struct _GumPageLump
{
  gpointer start;
  gpointer end;
  gpointer writable_start;
  guint n_pages;
};

struct _GumSuspendOperation
{
  GumThreadId current_thread_id;
  GumMetalArray suspended_threads;
};

struct _GumPointerScan
{
  const gsize * values;
  guint n_values;
  gsize mask;
  GArray * tiles;
  GumExceptor * exceptor;
};

struct _GumPointerScanTile
{
  const gsize * words;
  gsize n_words;
};

struct _GumPointerScanTask
{
  GumPointerScan * scan;
  const GumPointerScanTile * tile;
  GArray * matches;
};

static void gum_apply_patch_code (gpointer mem, gpointer target_page,
    guint n_pages, gpointer user_data);
static gboolean gum_memory_patch_code_pages_via_remap (
    GPtrArray * sorted_addresses, gboolean coalesce, gsize page_size,
    GumMemoryPatchPagesApplyFunc apply, gpointer apply_data);
static gboolean gum_memory_patch_code_pages_via_mprotect (
    GPtrArray * sorted_addresses, gboolean coalesce, gsize page_size,
    gboolean rwx_supported, GumMemoryPatchPagesApplyFunc apply,
    gpointer apply_data);
static gboolean gum_memory_patch_code_pages_via_code_segment (
    GPtrArray * sorted_addresses, gboolean coalesce, gsize page_size,
    GumMemoryPatchPagesApplyFunc apply, gpointer apply_data);
static gboolean gum_maybe_suspend_thread (const GumThreadDetails * details,
    gpointer user_data);

static void gum_memory_scan_raw (const GumMemoryRange * range,
    const GumMatchPattern * pattern, GumMemoryScanMatchFunc func,
    gpointer user_data);
static void gum_memory_scan_regex (const GumMemoryRange * range,
    const GRegex * regex, GumMemoryScanMatchFunc func, gpointer user_data);
static GumMatchPattern * gum_match_pattern_new_from_hexstring (
    const gchar * match_combined_str);
static GumMatchPattern * gum_match_pattern_new_from_regex (
    const gchar * regex_str);
static GumMatchPattern * gum_match_pattern_new (void);
static void gum_match_pattern_update_computed_size (GumMatchPattern * self);
static GumMatchToken * gum_match_pattern_get_longest_token (
    const GumMatchPattern * self, GumMatchType type);
static gboolean gum_match_pattern_try_match_on (const GumMatchPattern * self,
    guint8 * bytes);
static gint gum_memcmp_mask (const guint8 * haystack, const guint8 * needle,
    const guint8 * mask, guint len);
static GumMatchToken * gum_match_pattern_push_token (GumMatchPattern * self,
    GumMatchType type);
static gboolean gum_match_pattern_seal (GumMatchPattern * self);

static GumMatchToken * gum_match_token_new (GumMatchType type);
static void gum_match_token_free (GumMatchToken * token);
static void gum_match_token_append (GumMatchToken * self, guint8 byte);
static void gum_match_token_append_with_mask (GumMatchToken * self,
    guint8 byte, guint8 mask);

static GArray * gum_pointer_scan_tiles_from_ranges (
    const GumMemoryRange * ranges, guint n_ranges);
static gsize gum_pointer_scan_count_words (GArray * tiles);
static void gum_pointer_scan_run_parallel (GumPointerScan * self,
    GArray * matches);
static void gum_pointer_scan_process_task (gpointer data, gpointer user_data);
static void gum_pointer_scan_run_inline (GumPointerScan * self,
    GArray * matches);
static void gum_pointer_scan_process_tile (GumPointerScan * self,
    const GumPointerScanTile * tile, GArray * matches);
#ifdef GUM_HAVE_POINTER_SCAN_SIMD
static gsize gum_pointer_scan_process_vectors (GumPointerScan * self,
    const gsize * words, gsize n_words, GArray * matches);
static void gum_pointer_scan_process_single (GumPointerScan * self,
    const gsize * words, gsize n_vectors, GArray * matches);
static void gum_pointer_scan_process_few (GumPointerScan * self,
    const gsize * words, gsize n_vectors, GArray * matches);
static void gum_pointer_scan_process_many (GumPointerScan * self,
    const gsize * words, gsize n_vectors, GArray * matches);
static GumScanVec gum_pointer_scan_cmpeq (GumScanVec value, GumScanVec masked);
static void gum_pointer_scan_emit (GArray * matches, const gsize * pair,
    GumScanVec cmp);
#endif
static void gum_pointer_scan_check_word (GumPointerScan * self,
    const gsize * word, GArray * matches);
static void gum_pointer_scan_record_match (GArray * matches,
    const gsize * word);
static gint gum_pointer_match_compare (gconstpointer a, gconstpointer b);

static guint gum_heap_ref_count = 0;
#ifndef GUM_USE_SYSTEM_ALLOC
static mspace gum_mspace_main = NULL;
static mspace gum_mspace_internal = NULL;
#endif
static guint gum_cached_page_size;

#ifdef HAVE_ANDROID
G_LOCK_DEFINE_STATIC (gum_softened_code_pages);
static GHashTable * gum_softened_code_pages;
#endif

G_DEFINE_BOXED_TYPE (GumMatchPattern, gum_match_pattern, gum_match_pattern_ref,
                     gum_match_pattern_unref)
G_DEFINE_BOXED_TYPE (GumMemoryRange, gum_memory_range, gum_memory_range_copy,
                     gum_memory_range_free)

void
gum_internal_heap_ref (void)
{
  if (gum_heap_ref_count++ > 0)
    return;

  _gum_memory_backend_init ();

  gum_cached_page_size = _gum_memory_backend_query_page_size ();

  _gum_cloak_init ();

#ifndef GUM_USE_SYSTEM_ALLOC
  gum_mspace_main = create_mspace (0, TRUE);
  gum_mspace_internal = create_mspace (0, TRUE);
#endif
}

void
gum_internal_heap_unref (void)
{
  g_assert (gum_heap_ref_count != 0);
  if (--gum_heap_ref_count > 0)
    return;

#ifndef GUM_USE_SYSTEM_ALLOC
  destroy_mspace (gum_mspace_internal);
  gum_mspace_internal = NULL;

  destroy_mspace (gum_mspace_main);
  gum_mspace_main = NULL;

  (void) DESTROY_LOCK (&malloc_global_mutex);
#endif

  _gum_cloak_deinit ();

  _gum_memory_backend_deinit ();
}

gpointer
gum_sign_code_pointer (gpointer value)
{
#ifdef HAVE_PTRAUTH
  return ptrauth_sign_unauthenticated (value, ptrauth_key_asia, 0);
#else
  return value;
#endif
}

gpointer
gum_strip_code_pointer (gpointer value)
{
#ifdef HAVE_PTRAUTH
  return ptrauth_strip (value, ptrauth_key_asia);
#else
  return value;
#endif
}

GumAddress
gum_sign_code_address (GumAddress value)
{
#ifdef HAVE_PTRAUTH
  return GPOINTER_TO_SIZE (ptrauth_sign_unauthenticated (
      GSIZE_TO_POINTER (value), ptrauth_key_asia, 0));
#else
  return value;
#endif
}

GumAddress
gum_strip_code_address (GumAddress value)
{
#ifdef HAVE_PTRAUTH
  return GPOINTER_TO_SIZE (ptrauth_strip (
      GSIZE_TO_POINTER (value), ptrauth_key_asia));
#else
  return value;
#endif
}

GumPtrauthSupport
gum_query_ptrauth_support (void)
{
#ifdef HAVE_PTRAUTH
  return GUM_PTRAUTH_SUPPORTED;
#else
  return GUM_PTRAUTH_UNSUPPORTED;
#endif
}

guint
gum_query_page_size (void)
{
  return gum_cached_page_size;
}

gboolean
gum_query_is_rwx_supported (void)
{
  return gum_query_rwx_support () == GUM_RWX_FULL;
}

#ifdef G_OS_NONE
G_GNUC_WEAK
#endif
GumRwxSupport
gum_query_rwx_support (void)
{
#if defined (HAVE_DARWIN) && !defined (HAVE_I386)
  return GUM_RWX_NONE;
#else
  return GUM_RWX_FULL;
#endif
}

/**
 * gum_memory_patch_code:
 * @address: address to modify from
 * @size: number of bytes to modify
 * @apply: (scope call): function to apply the modifications
 *
 * Safely modifies @size bytes at @address. The supplied function @apply gets
 * called with a writable pointer where you must write the desired
 * modifications before returning. Do not make any assumptions about this being
 * the same location as @address, as some systems require modifications to be
 * written to a temporary location before being mapped into memory on top of the
 * original memory page (e.g. on iOS, where directly modifying in-memory code
 * may result in the process losing its CS_VALID status).
 *
 * Returns: whether the modifications were successfully applied
 */
gboolean
gum_memory_patch_code (gpointer address,
                       gsize size,
                       GumMemoryPatchApplyFunc apply,
                       gpointer apply_data)
{
  gboolean result;
  gsize page_size;
  guint8 * start_page, * end_page;
  gsize page_offset;
  GPtrArray * page_addresses;
  GumPatchCodeContext context;

  address = gum_strip_code_pointer (address);

  page_size = gum_query_page_size ();
  start_page = GSIZE_TO_POINTER (GPOINTER_TO_SIZE (address) & ~(page_size - 1));
  end_page = GSIZE_TO_POINTER (
      (GPOINTER_TO_SIZE (address) + size - 1) & ~(page_size - 1));
  page_offset = ((guint8 *) address) - start_page;

  page_addresses =
      g_ptr_array_sized_new (((end_page - start_page) / page_size) + 1);

  g_ptr_array_add (page_addresses, start_page);

  if (end_page != start_page)
  {
    guint8 * cur;

    for (cur = start_page + page_size;
        cur != end_page + page_size;
        cur += page_size)
    {
      g_ptr_array_add (page_addresses, cur);
    }
  }

  context.page_offset = page_offset;
  context.func = apply;
  context.user_data = apply_data;

  result = gum_memory_patch_code_pages (page_addresses, TRUE,
      gum_apply_patch_code, &context);

  g_ptr_array_unref (page_addresses);

  return result;
}

static void
gum_apply_patch_code (gpointer mem,
                      gpointer target_page,
                      guint n_pages,
                      gpointer user_data)
{
  GumPatchCodeContext * context = user_data;

  context->func ((guint8 *) mem + context->page_offset, context->user_data);
}

/**
 * gum_memory_patch_code_pages: (skip)
 *
 * Safely modifies code pages at the given addresses.
 */
gboolean
gum_memory_patch_code_pages (GPtrArray * sorted_addresses,
                             gboolean coalesce,
                             GumMemoryPatchPagesApplyFunc apply,
                             gpointer apply_data)
{
  gsize page_size;
  gboolean rwx_supported;

  rwx_supported = gum_query_is_rwx_supported ();
  page_size = gum_query_page_size ();

  if (gum_memory_can_remap_writable ())
  {
    return gum_memory_patch_code_pages_via_remap (sorted_addresses, coalesce,
        page_size, apply, apply_data);
  }
  else if (rwx_supported || !gum_code_segment_is_supported ())
  {
    return gum_memory_patch_code_pages_via_mprotect (sorted_addresses,
        coalesce, page_size, rwx_supported, apply, apply_data);
  }
  else
  {
    return gum_memory_patch_code_pages_via_code_segment (sorted_addresses,
        coalesce, page_size, apply, apply_data);
  }
}

static gboolean
gum_memory_patch_code_pages_via_remap (GPtrArray * sorted_addresses,
                                       gboolean coalesce,
                                       gsize page_size,
                                       GumMemoryPatchPagesApplyFunc apply,
                                       gpointer apply_data)
{
  gboolean result = TRUE;
  guint i;
  GArray * plumps;
  GumPageLump * last;

#ifdef HAVE_DARWIN
  if (gum_darwin_is_debugger_mapping_enforced ())
  {
    GumPagePlanBuilder plan;
    gboolean success;

    _gum_page_plan_builder_init (&plan);

    for (i = 0; i != sorted_addresses->len; i++)
    {
      gpointer target_page = g_ptr_array_index (sorted_addresses, i);

      _gum_page_plan_builder_add_page (&plan, target_page);
    }

    success = _gum_page_plan_builder_post (&plan);

    _gum_page_plan_builder_free (&plan);

    if (!success)
      return FALSE;
  }
#endif

  plumps = g_array_new (FALSE, FALSE, sizeof (GumPageLump));
  last = NULL;

  for (i = 0; i != sorted_addresses->len; i++)
  {
    guint8 * target_page = g_ptr_array_index (sorted_addresses, i);

    last = (plumps->len != 0)
        ? &g_array_index (plumps, GumPageLump, plumps->len - 1)
        : NULL;

    if (last == NULL || last->end != target_page)
    {
      GumPageLump lump;

      if (last != NULL)
      {
        gpointer writable;

        writable = gum_memory_try_remap_writable_pages (last->start,
            last->n_pages);
        if (writable == NULL)
        {
          result = FALSE;
          goto cleanup;
        }

        last->writable_start = writable;
      }

      lump.start = target_page;
      lump.end = target_page;
      lump.writable_start = NULL;
      lump.n_pages = 0;

      g_array_append_val (plumps, lump);
    }

    last = &g_array_index (plumps, GumPageLump, plumps->len - 1);
    last->end = target_page + page_size;
    last->n_pages++;
  }

  if (plumps->len == 0)
    goto cleanup;

  last->writable_start =
      gum_memory_try_remap_writable_pages (last->start, last->n_pages);
  if (last->writable_start == NULL)
  {
    result = FALSE;
    goto cleanup;
  }

  if (coalesce)
  {
    for (i = 0; i != plumps->len; i++)
    {
      const GumPageLump * plump = &g_array_index (plumps, GumPageLump, i);

      apply (plump->writable_start, plump->start, plump->n_pages, apply_data);
    }
  }
  else
  {
    guint plump_index = 0;

    for (i = 0; i != sorted_addresses->len; i++)
    {
      guint8 * target_page;
      const GumPageLump * plump;
      gsize offset;

      target_page = g_ptr_array_index (sorted_addresses, i);

      plump = &g_array_index (plumps, GumPageLump, plump_index);

      if (target_page >= (guint8 *) plump->end)
      {
        plump_index++;
        g_assert (plump_index != plumps->len);
        plump = &g_array_index (plumps, GumPageLump, plump_index);
      }

      g_assert (target_page >= (guint8 *) plump->start);
      g_assert (target_page < (guint8 *) plump->end);
      offset = target_page - (guint8 *) plump->start;

      apply ((guint8 *) plump->writable_start + offset, target_page, 1,
          apply_data);
    }
  }

  for (i = 0; i != sorted_addresses->len; i++)
  {
    gpointer target_page = g_ptr_array_index (sorted_addresses, i);

    gum_clear_cache (target_page, page_size);
  }

cleanup:
  for (i = 0; i != plumps->len; i++)
  {
    const GumPageLump * plump = &g_array_index (plumps, GumPageLump, i);

    if (plump->writable_start != NULL)
    {
      gum_memory_dispose_writable_pages (plump->writable_start,
          plump->n_pages);
    }
  }

  g_array_unref (plumps);

  return result;
}

static gboolean
gum_memory_patch_code_pages_via_mprotect (GPtrArray * sorted_addresses,
                                          gboolean coalesce,
                                          gsize page_size,
                                          gboolean rwx_supported,
                                          GumMemoryPatchPagesApplyFunc apply,
                                          gpointer apply_data)
{
  gboolean result = TRUE;
  guint i;
  guint8 * apply_start, * apply_target_start;
  guint apply_num_pages;
  GumSuspendOperation suspend_op = { 0, };
  guint num_suspended;
  guint8 * scratch, * source_page, * pristine, * pristine_page;
  GumPageProtection * original_protections;

  if (rwx_supported)
  {
    original_protections = g_newa (GumPageProtection, sorted_addresses->len);

#ifdef HAVE_LINUX
    _gum_memory_query_protections (sorted_addresses, original_protections);
#else
    for (i = 0; i != sorted_addresses->len; i++)
    {
      gpointer target_page = g_ptr_array_index (sorted_addresses, i);

      if (!gum_memory_query_protection (target_page, &original_protections[i]))
        original_protections[i] = GUM_PAGE_RX;
    }
#endif

    for (i = 0; i != sorted_addresses->len; i++)
    {
      gpointer target_page = g_ptr_array_index (sorted_addresses, i);

      if (!gum_try_mprotect (target_page, page_size, GUM_PAGE_RWX))
        return FALSE;
    }

    apply_start = NULL;
    apply_num_pages = 0;
    for (i = 0; i != sorted_addresses->len; i++)
    {
      gpointer target_page = g_ptr_array_index (sorted_addresses, i);

      if (coalesce)
      {
        if (apply_start != 0)
        {
          if (target_page == apply_start + (page_size * apply_num_pages))
          {
            apply_num_pages++;
          }
          else
          {
            apply (apply_start, apply_target_start, apply_num_pages,
                apply_data);
            apply_start = 0;
          }
        }

        if (apply_start == 0)
        {
          apply_start = target_page;
          apply_target_start = target_page;
          apply_num_pages = 1;
        }
      }
      else
      {
        apply (target_page, target_page, 1, apply_data);
      }
    }

    if (apply_num_pages != 0)
      apply (apply_start, apply_target_start, apply_num_pages, apply_data);

    for (i = 0; i != sorted_addresses->len; i++)
    {
      gpointer target_page = g_ptr_array_index (sorted_addresses, i);
      GumPageProtection restored;

      restored = ((original_protections[i] & GUM_PAGE_WRITE) != 0)
          ? GUM_PAGE_RWX
          : GUM_PAGE_RX;

      if (!gum_try_mprotect (target_page, page_size, restored))
        return FALSE;
    }

    for (i = 0; i != sorted_addresses->len; i++)
    {
      gpointer target_page = g_ptr_array_index (sorted_addresses, i);

      gum_clear_cache (target_page, page_size);
    }

    return TRUE;
  }

  scratch = gum_memory_allocate (NULL, sorted_addresses->len * page_size,
      page_size, GUM_PAGE_RW);
  pristine = gum_memory_allocate (NULL, sorted_addresses->len * page_size,
      page_size, GUM_PAGE_RW);

  source_page = scratch;
  for (i = 0; i != sorted_addresses->len; i++)
  {
    gpointer target_page = g_ptr_array_index (sorted_addresses, i);

    memcpy (source_page, target_page, page_size);

    source_page += page_size;
  }

  memcpy (pristine, scratch, sorted_addresses->len * page_size);

  apply_start = NULL;
  apply_num_pages = 0;
  source_page = scratch;
  for (i = 0; i != sorted_addresses->len; i++)
  {
    guint8 * target_page = g_ptr_array_index (sorted_addresses, i);

    if (coalesce)
    {
      if (apply_start != NULL)
      {
        if (target_page == apply_target_start + (page_size * apply_num_pages))
        {
          apply_num_pages++;
        }
        else
        {
          apply (apply_start, apply_target_start, apply_num_pages,
              apply_data);
          apply_start = NULL;
        }
      }

      if (apply_start == NULL)
      {
        apply_start = source_page;
        apply_target_start = target_page;
        apply_num_pages = 1;
      }
    }
    else
    {
      apply (source_page, target_page, 1, apply_data);
    }

    source_page += page_size;
  }

  if (apply_num_pages != 0)
    apply (apply_start, apply_target_start, apply_num_pages, apply_data);

  gum_metal_array_init (&suspend_op.suspended_threads, sizeof (GumThreadId));
  suspend_op.current_thread_id = gum_process_get_current_thread_id ();
  _gum_process_enumerate_threads (gum_maybe_suspend_thread, &suspend_op,
      GUM_THREAD_FLAGS_NONE);

  for (i = 0; i != sorted_addresses->len; i++)
  {
    gpointer target_page = g_ptr_array_index (sorted_addresses, i);

    if (!gum_try_mprotect (target_page, page_size, GUM_PAGE_RW))
    {
      guint j;

      for (j = 0; j != i; j++)
      {
        gum_try_mprotect (g_ptr_array_index (sorted_addresses, j), page_size,
            GUM_PAGE_RX);
      }

      result = FALSE;
      break;
    }
  }

  if (result)
  {
    source_page = scratch;
    pristine_page = pristine;
    for (i = 0; i != sorted_addresses->len; i++)
    {
      guint8 * target_page = g_ptr_array_index (sorted_addresses, i);
      gsize offset;

      for (offset = 0; offset != page_size; offset++)
      {
        if (source_page[offset] != pristine_page[offset])
          target_page[offset] = source_page[offset];
      }

      if (!gum_try_mprotect (target_page, page_size, GUM_PAGE_RX))
      {
        result = FALSE;
        break;
      }

      source_page += page_size;
      pristine_page += page_size;
    }
  }

  if (result)
  {
    for (i = 0; i != sorted_addresses->len; i++)
      gum_clear_cache (g_ptr_array_index (sorted_addresses, i), page_size);
  }

  num_suspended = suspend_op.suspended_threads.length;

  for (i = 0; i != num_suspended; i++)
  {
    GumThreadId * raw_id = gum_metal_array_element_at (
        &suspend_op.suspended_threads, i);

    gum_thread_resume (*raw_id, NULL);
#ifdef HAVE_DARWIN
    mach_port_mod_refs (mach_task_self (), *raw_id, MACH_PORT_RIGHT_SEND, -1);
#endif
  }

  gum_metal_array_free (&suspend_op.suspended_threads);

  gum_memory_free (scratch, sorted_addresses->len * page_size);
  gum_memory_free (pristine, sorted_addresses->len * page_size);

  return result;
}

static gboolean
gum_memory_patch_code_pages_via_code_segment (
    GPtrArray * sorted_addresses,
    gboolean coalesce,
    gsize page_size,
    GumMemoryPatchPagesApplyFunc apply,
    gpointer apply_data)
{
  guint i;
  guint8 * apply_start, * apply_target_start;
  guint apply_num_pages;
  GumCodeSegment * segment;
  guint8 * source_page, * current_page;
  gsize source_offset;

  segment = gum_code_segment_new (sorted_addresses->len * page_size, NULL);

  source_page = gum_code_segment_get_address (segment);

  current_page = source_page;
  for (i = 0; i != sorted_addresses->len; i++)
  {
    guint8 * target_page = g_ptr_array_index (sorted_addresses, i);

    memcpy (current_page, target_page, page_size);

    current_page += page_size;
  }

  apply_start = NULL;
  apply_num_pages = 0;
  for (i = 0; i != sorted_addresses->len; i++)
  {
    guint8 * target_page = g_ptr_array_index (sorted_addresses, i);

    if (coalesce)
    {
      if (apply_start != NULL)
      {
        if (target_page == apply_target_start + (page_size * apply_num_pages))
        {
          apply_num_pages++;
        }
        else
        {
          apply (apply_start, apply_target_start, apply_num_pages,
              apply_data);
          apply_start = NULL;
        }
      }

      if (apply_start == NULL)
      {
        apply_start = source_page;
        apply_target_start = target_page;
        apply_num_pages = 1;
      }
    }
    else
    {
      apply (source_page, target_page, 1, apply_data);
    }

    source_page += page_size;
  }

  if (apply_num_pages != 0)
    apply (apply_start, apply_target_start, apply_num_pages, apply_data);

  gum_code_segment_realize (segment);

  source_offset = 0;
  for (i = 0; i != sorted_addresses->len; i++)
  {
    gpointer target_page = g_ptr_array_index (sorted_addresses, i);

    gum_code_segment_map (segment, source_offset, page_size, target_page);

    gum_clear_cache (target_page, page_size);

    source_offset += page_size;
  }

  gum_code_segment_free (segment);

  return TRUE;
}

static gboolean
gum_maybe_suspend_thread (const GumThreadDetails * details,
                          gpointer user_data)
{
  GumSuspendOperation * op = user_data;
  GumThreadId * suspended_id;

  if (details->id == op->current_thread_id)
    goto skip;

  if (!gum_thread_suspend (details->id, NULL))
    goto skip;

#ifdef HAVE_DARWIN
  mach_port_mod_refs (mach_task_self (), details->id, MACH_PORT_RIGHT_SEND, 1);
#endif
  suspended_id = gum_metal_array_append (&op->suspended_threads);
  *suspended_id = details->id;

skip:
  return TRUE;
}

gboolean
gum_memory_mark_code (gpointer address,
                      gsize size)
{
  gboolean success;

  if (gum_code_segment_is_supported ())
  {
    gsize page_size;
    guint8 * start_page, * end_page;

    page_size = gum_query_page_size ();
    start_page =
        GSIZE_TO_POINTER (GPOINTER_TO_SIZE (address) & ~(page_size - 1));
    end_page = GSIZE_TO_POINTER (
        (GPOINTER_TO_SIZE (address) + size - 1) & ~(page_size - 1));

    success = gum_code_segment_mark (start_page,
        end_page - start_page + page_size, NULL);
  }
  else
  {
    success = gum_try_mprotect (address, size, GUM_PAGE_RX);
  }

  gum_clear_cache (address, size);

  return success;
}

/**
 * gum_memory_scan:
 * @range: the #GumMemoryRange to scan
 * @pattern: the #GumMatchPattern to look for occurrences of
 * @func: (scope call): function to process each match
 * @user_data: data to pass to @func
 *
 * Scans @range for occurrences of @pattern, calling @func with each match.
 */
void
gum_memory_scan (const GumMemoryRange * range,
                 const GumMatchPattern * pattern,
                 GumMemoryScanMatchFunc func,
                 gpointer user_data)
{
  if (pattern->regex == NULL)
    gum_memory_scan_raw (range, pattern, func, user_data);
  else
    gum_memory_scan_regex (range, pattern->regex, func, user_data);
}

static void
gum_memory_scan_raw (const GumMemoryRange * range,
                     const GumMatchPattern * pattern,
                     GumMemoryScanMatchFunc func,
                     gpointer user_data)
{
  GumMatchToken * needle;
  guint8 * needle_data, * mask_data = NULL;
  guint needle_len, pattern_size;
  guint8 * cur, * end_address;

  needle = gum_match_pattern_get_longest_token (pattern, GUM_MATCH_EXACT);
  if (needle == NULL)
  {
    needle = gum_match_pattern_get_longest_token (pattern, GUM_MATCH_MASK);
    mask_data = (guint8 *) needle->masks->data;
  }

  needle_data = (guint8 *) needle->bytes->data;
  needle_len = needle->bytes->len;
  pattern_size = gum_match_pattern_get_size (pattern);

  cur = (guint8 *) GSIZE_TO_POINTER (range->base_address) + needle->offset;
  end_address = cur + range->size - pattern_size + 1;

  for (; cur < end_address; cur++)
  {
    guint8 * start;

    if (mask_data == NULL)
    {
      if (cur[0] != needle_data[0] ||
          memcmp (cur, needle_data, needle_len) != 0)
      {
        continue;
      }
    }
    else
    {
      if ((cur[0] & mask_data[0]) != (needle_data[0] & mask_data[0]) ||
          gum_memcmp_mask ((guint8 *) cur, (guint8 *) needle_data,
              (guint8 *) mask_data, needle_len) != 0)
      {
        continue;
      }
    }

    start = cur - needle->offset;

    if (gum_match_pattern_try_match_on (pattern, start))
    {
      if (!func (GUM_ADDRESS (start), pattern_size, user_data))
        return;

      cur = start + pattern_size + needle->offset - 1;
    }
  }
}

static void
gum_memory_scan_regex (const GumMemoryRange * range,
                       const GRegex * regex,
                       GumMemoryScanMatchFunc func,
                       gpointer user_data)
{
  GMatchInfo * info;

  g_regex_match_full (regex, GSIZE_TO_POINTER (range->base_address),
      range->size, 0, 0, &info, NULL);

  while (g_match_info_matches (info))
  {
    gint start_pos, end_pos;

    if (!g_match_info_fetch_pos (info, 0, &start_pos, &end_pos) ||
        (gsize) end_pos > range->size ||
        !func (GUM_ADDRESS (range->base_address + start_pos),
            end_pos - start_pos, user_data))
    {
      break;
    }

    g_match_info_next (info, NULL);
  }

  g_match_info_free (info);
}

GumMatchPattern *
gum_match_pattern_new_from_string (const gchar * pattern_str)
{
  GumMatchPattern * result;

  if (g_str_has_prefix (pattern_str, "/") &&
      g_str_has_suffix (pattern_str, "/"))
  {
    gchar * regex_str = g_strndup (pattern_str + 1, strlen (pattern_str) - 2);
    result = gum_match_pattern_new_from_regex (regex_str);
    g_free (regex_str);
  }
  else
  {
    result = gum_match_pattern_new_from_hexstring (pattern_str);
  }

  return result;
}

static GumMatchPattern *
gum_match_pattern_new_from_hexstring (const gchar * match_combined_str)
{
  GumMatchPattern * pattern = NULL;
  gchar ** parts;
  const gchar * match_str, * mask_str;
  gboolean has_mask = FALSE;
  GumMatchToken * token = NULL;
  const gchar * ch, * mh;

  parts = g_strsplit (match_combined_str, ":", 2);
  match_str = parts[0];
  if (match_str == NULL)
    goto parse_error;

  mask_str = parts[1];
  has_mask = mask_str != NULL;
  if (has_mask && strlen (mask_str) != strlen (match_str))
    goto parse_error;

  pattern = gum_match_pattern_new ();

  for (ch = match_str, mh = mask_str;
       *ch != '\0' && (!has_mask || *mh != '\0');
       ch++, mh++)
  {
    gint upper, lower;
    gint mask = 0xff;
    guint8 value;

    if (ch[0] == ' ')
      continue;

    if (has_mask)
    {
      while (mh[0] == ' ')
        mh++;
      if ((upper = g_ascii_xdigit_value (mh[0])) == -1)
        goto parse_error;
      if ((lower = g_ascii_xdigit_value (mh[1])) == -1)
        goto parse_error;
      mask = (upper << 4) | lower;
    }

    if (ch[0] == '?')
    {
      upper = 4;
      mask &= 0x0f;
    }
    else if ((upper = g_ascii_xdigit_value (ch[0])) == -1)
    {
      goto parse_error;
    }

    if (ch[1] == '?')
    {
      lower = 2;
      mask &= 0xf0;
    }
    else if ((lower = g_ascii_xdigit_value (ch[1])) == -1)
    {
      goto parse_error;
    }

    value = (upper << 4) | lower;

    if (mask == 0xff)
    {
      if (token == NULL || token->type != GUM_MATCH_EXACT)
        token = gum_match_pattern_push_token (pattern, GUM_MATCH_EXACT);
      gum_match_token_append (token, value);
    }
    else if (mask == 0x00)
    {
      if (token == NULL || token->type != GUM_MATCH_WILDCARD)
        token = gum_match_pattern_push_token (pattern, GUM_MATCH_WILDCARD);
      gum_match_token_append (token, 0x42);
    }
    else
    {
      if (token == NULL || token->type != GUM_MATCH_MASK)
        token = gum_match_pattern_push_token (pattern, GUM_MATCH_MASK);
      gum_match_token_append_with_mask (token, value, mask);
    }

    ch++;
    mh++;
  }

  if (!gum_match_pattern_seal (pattern))
    goto parse_error;

  g_strfreev (parts);

  return pattern;

  /* ERRORS */
parse_error:
  {
    g_strfreev (parts);
    if (pattern != NULL)
      gum_match_pattern_unref (pattern);

    return NULL;
  }
}

static GumMatchPattern *
gum_match_pattern_new_from_regex (const gchar * regex_str)
{
  GumMatchPattern * pattern;
  GRegex * regex;

  regex = g_regex_new (regex_str, G_REGEX_OPTIMIZE | G_REGEX_RAW,
      G_REGEX_MATCH_NOTEMPTY, NULL);
  if (regex == NULL)
    return NULL;

  pattern = gum_match_pattern_new ();
  pattern->regex = regex;

  return pattern;
}

static GumMatchPattern *
gum_match_pattern_new (void)
{
  GumMatchPattern * pattern;

  pattern = g_slice_new (GumMatchPattern);
  pattern->ref_count = 1;
  pattern->tokens =
      g_ptr_array_new_with_free_func ((GDestroyNotify) gum_match_token_free);
  pattern->size = 0;
  pattern->regex = NULL;

  return pattern;
}

GumMatchPattern *
gum_match_pattern_ref (GumMatchPattern * pattern)
{
  g_atomic_int_inc (&pattern->ref_count);

  return pattern;
}

void
gum_match_pattern_unref (GumMatchPattern * pattern)
{
  if (g_atomic_int_dec_and_test (&pattern->ref_count))
  {
    if (pattern->regex != NULL)
      g_regex_unref (pattern->regex);

    g_ptr_array_free (pattern->tokens, TRUE);

    g_slice_free (GumMatchPattern, pattern);
  }
}

guint
gum_match_pattern_get_size (const GumMatchPattern * pattern)
{
  return pattern->size;
}

/**
 * gum_match_pattern_get_tokens: (skip)
 */
GPtrArray *
gum_match_pattern_get_tokens (const GumMatchPattern * pattern)
{
  return pattern->tokens;
}

static void
gum_match_pattern_update_computed_size (GumMatchPattern * self)
{
  guint i;

  self->size = 0;

  for (i = 0; i != self->tokens->len; i++)
  {
    GumMatchToken * token;

    token = (GumMatchToken *) g_ptr_array_index (self->tokens, i);
    self->size += token->bytes->len;
  }
}

static GumMatchToken *
gum_match_pattern_get_longest_token (const GumMatchPattern * self,
                                     GumMatchType type)
{
  GumMatchToken * longest = NULL;
  guint i;

  for (i = 0; i != self->tokens->len; i++)
  {
    GumMatchToken * token;

    token = (GumMatchToken *) g_ptr_array_index (self->tokens, i);
    if (token->type == type && (longest == NULL
        || token->bytes->len > longest->bytes->len))
    {
      longest = token;
    }
  }

  return longest;
}

static gboolean
gum_match_pattern_try_match_on (const GumMatchPattern * self,
                                guint8 * bytes)
{
  guint i;
  gboolean no_masks = TRUE;

  for (i = 0; i != self->tokens->len; i++)
  {
    GumMatchToken * token;

    token = (GumMatchToken *) g_ptr_array_index (self->tokens, i);
    if (token->type == GUM_MATCH_EXACT)
    {
      gchar * p;

      p = (gchar *) bytes + token->offset;
      if (p == token->bytes->data ||
          memcmp (p, token->bytes->data, token->bytes->len) != 0)
      {
        return FALSE;
      }
    }
    else if (token->type == GUM_MATCH_MASK)
    {
      no_masks = FALSE;
    }
  }

  if (no_masks)
    return TRUE;

  for (i = 0; i != self->tokens->len; i++)
  {
    GumMatchToken * token;

    token = (GumMatchToken *) g_ptr_array_index (self->tokens, i);
    if (token->type == GUM_MATCH_MASK)
    {
      gchar * p;

      p = (gchar *) bytes + token->offset;
      if (gum_memcmp_mask ((guint8 *) p, (guint8 *) token->bytes->data,
          (guint8 *) token->masks->data, token->masks->len) != 0)
      {
        return FALSE;
      }
    }
  }

  return TRUE;
}

static gint
gum_memcmp_mask (const guint8 * haystack,
                 const guint8 * needle,
                 const guint8 * mask,
                 guint len)
{
  guint i;

  for (i = 0; i != len; i++)
  {
    guint8 value = *(haystack++) & mask[i];
    guint8 test_value = needle[i] & mask[i];
    if (value != test_value)
      return value - test_value;
  }

  return 0;
}

static GumMatchToken *
gum_match_pattern_push_token (GumMatchPattern * self,
                              GumMatchType type)
{
  GumMatchToken * token;

  gum_match_pattern_update_computed_size (self);

  token = gum_match_token_new (type);
  token->offset = self->size;
  g_ptr_array_add (self->tokens, token);

  return token;
}

static gboolean
gum_match_pattern_seal (GumMatchPattern * self)
{
  gum_match_pattern_update_computed_size (self);

  if (self->size == 0)
    return FALSE;

  return gum_match_pattern_get_longest_token (self, GUM_MATCH_EXACT) != NULL ||
      gum_match_pattern_get_longest_token (self, GUM_MATCH_MASK) != NULL;
}

static GumMatchToken *
gum_match_token_new (GumMatchType type)
{
  GumMatchToken * token;

  token = g_slice_new (GumMatchToken);
  token->type = type;
  token->bytes = g_array_new (FALSE, FALSE, sizeof (guint8));
  token->masks = NULL;
  token->offset = 0;

  return token;
}

static void
gum_match_token_free (GumMatchToken * token)
{
  g_array_free (token->bytes, TRUE);
  if (token->masks != NULL)
    g_array_free (token->masks, TRUE);
  g_slice_free (GumMatchToken, token);
}

static void
gum_match_token_append (GumMatchToken * self,
                        guint8 byte)
{
  g_array_append_val (self->bytes, byte);
}

static void
gum_match_token_append_with_mask (GumMatchToken * self,
                                  guint8 byte,
                                  guint8 mask)
{
  g_array_append_val (self->bytes, byte);

  if (self->masks == NULL)
    self->masks = g_array_new (FALSE, FALSE, sizeof (guint8));

  g_array_append_val (self->masks, mask);
}

/**
 * gum_memory_find_pointers:
 * @ranges: (array length=n_ranges): the #GumMemoryRange instances to scan
 * @n_ranges: the number of @ranges
 * @values: (array length=n_values): the pointer-width values to look for
 * @n_values: the number of @values
 * @mask: bitmask applied to each scanned word and each value before comparing
 *
 * Scans @ranges for pointer-aligned words matching any of @values, comparing
 * under @mask. Use %G_MAXSIZE for an exact match, or e.g.
 * 0x00007ffffffffff8 to strip arm64e PAC and non-pointer-isa bits. Memory that
 * is or becomes inaccessible while scanning is skipped.
 *
 * Returns: (element-type GumPointerMatch) (transfer full): the matches, sorted
 *          by address
 */
GArray *
gum_memory_find_pointers (const GumMemoryRange * ranges,
                          guint n_ranges,
                          const gsize * values,
                          guint n_values,
                          gsize mask)
{
  GArray * matches;
  gsize * masked_values;
  GumPointerScan scan;
  guint i;

  matches = g_array_new (FALSE, FALSE, sizeof (GumPointerMatch));

  masked_values = g_newa (gsize, n_values);
  for (i = 0; i != n_values; i++)
    masked_values[i] = values[i] & mask;

  scan.values = masked_values;
  scan.n_values = n_values;
  scan.mask = mask;
  scan.tiles = gum_pointer_scan_tiles_from_ranges (ranges, n_ranges);
  scan.exceptor = gum_exceptor_obtain ();

  if (gum_pointer_scan_count_words (scan.tiles) < GUM_POINTER_SCAN_INLINE_LIMIT)
    gum_pointer_scan_run_inline (&scan, matches);
  else
    gum_pointer_scan_run_parallel (&scan, matches);

  g_array_sort (matches, gum_pointer_match_compare);

  g_object_unref (scan.exceptor);
  g_array_free (scan.tiles, TRUE);

  return matches;
}

static GArray *
gum_pointer_scan_tiles_from_ranges (const GumMemoryRange * ranges,
                                    guint n_ranges)
{
  GArray * tiles;
  guint range_index;

  tiles = g_array_new (FALSE, FALSE, sizeof (GumPointerScanTile));

  for (range_index = 0; range_index != n_ranges; range_index++)
  {
    const GumMemoryRange * range = &ranges[range_index];
    gsize start, end;
    const gsize * words;
    gsize n_words, offset;

    start = GUM_ALIGN_SIZE (range->base_address, sizeof (gpointer));
    end = (range->base_address + range->size) & ~(sizeof (gpointer) - 1);
    if (end <= start)
      continue;

    words = GSIZE_TO_POINTER (start);
    n_words = (end - start) / sizeof (gpointer);

    for (offset = 0; offset < n_words; offset += GUM_POINTER_SCAN_TILE_WORDS)
    {
      GumPointerScanTile tile;

      tile.words = words + offset;
      tile.n_words = MIN (GUM_POINTER_SCAN_TILE_WORDS, n_words - offset);

      g_array_append_val (tiles, tile);
    }
  }

  return tiles;
}

static gsize
gum_pointer_scan_count_words (GArray * tiles)
{
  gsize n_words;
  guint i;

  n_words = 0;
  for (i = 0; i != tiles->len; i++)
    n_words += g_array_index (tiles, GumPointerScanTile, i).n_words;

  return n_words;
}

static void
gum_pointer_scan_run_parallel (GumPointerScan * self,
                               GArray * matches)
{
  guint max_threads, i;
  GThreadPool * pool;
  GArray * tasks;

  max_threads = MIN (g_get_num_processors (), GUM_POINTER_SCAN_MAX_WORKERS);
  pool = g_thread_pool_new (gum_pointer_scan_process_task, NULL, max_threads,
      FALSE, NULL);

  tasks = g_array_sized_new (FALSE, FALSE, sizeof (GumPointerScanTask),
      self->tiles->len);
  g_array_set_size (tasks, self->tiles->len);

  for (i = 0; i != self->tiles->len; i++)
  {
    GumPointerScanTask * task = &g_array_index (tasks, GumPointerScanTask, i);

    task->scan = self;
    task->tile = &g_array_index (self->tiles, GumPointerScanTile, i);
    task->matches = g_array_new (FALSE, FALSE, sizeof (GumPointerMatch));

    g_thread_pool_push (pool, task, NULL);
  }

  g_thread_pool_free (pool, FALSE, TRUE);

  for (i = 0; i != tasks->len; i++)
  {
    GArray * task_matches =
        g_array_index (tasks, GumPointerScanTask, i).matches;

    g_array_append_vals (matches, task_matches->data, task_matches->len);

    g_array_free (task_matches, TRUE);
  }

  g_array_free (tasks, TRUE);
}

static void
gum_pointer_scan_process_task (gpointer data,
                               gpointer user_data)
{
  GumPointerScanTask * task = data;

  gum_pointer_scan_process_tile (task->scan, task->tile, task->matches);
}

static void
gum_pointer_scan_run_inline (GumPointerScan * self,
                             GArray * matches)
{
  guint i;

  for (i = 0; i != self->tiles->len; i++)
  {
    gum_pointer_scan_process_tile (self,
        &g_array_index (self->tiles, GumPointerScanTile, i), matches);
  }
}

static void
gum_pointer_scan_process_tile (GumPointerScan * self,
                               const GumPointerScanTile * tile,
                               GArray * matches)
{
  GumExceptorScope scope;

  if (gum_exceptor_try (self->exceptor, &scope))
  {
    const gsize * words = tile->words;
    gsize n_words = tile->n_words;
    gsize i = 0;

#ifdef GUM_HAVE_POINTER_SCAN_SIMD
    i = gum_pointer_scan_process_vectors (self, words, n_words, matches);
#endif

    for (; i != n_words; i++)
      gum_pointer_scan_check_word (self, words + i, matches);
  }

  gum_exceptor_catch (self->exceptor, &scope);
}

#ifdef GUM_HAVE_POINTER_SCAN_SIMD

static gsize
gum_pointer_scan_process_vectors (GumPointerScan * self,
                                  const gsize * words,
                                  gsize n_words,
                                  GArray * matches)
{
  gsize n_vectors = n_words / 2;
  guint n_values = self->n_values;

  if (n_values == 1)
    gum_pointer_scan_process_single (self, words, n_vectors, matches);
  else if (n_values >= 2 && n_values <= 4)
    gum_pointer_scan_process_few (self, words, n_vectors, matches);
  else if (n_values > 4)
    gum_pointer_scan_process_many (self, words, n_vectors, matches);

  return n_vectors * 2;
}

static void
gum_pointer_scan_process_single (GumPointerScan * self,
                                 const gsize * words,
                                 gsize n_vectors,
                                 GArray * matches)
{
  GumScanVec mask = GUM_SCAN_VEC_SET1 (self->mask);
  GumScanVec value = GUM_SCAN_VEC_SET1 (self->values[0]);
  gsize i;

  for (i = 0; i != n_vectors; i++)
  {
    const gsize * pair = words + i * 2;
    GumScanVec masked = GUM_SCAN_VEC_AND (GUM_SCAN_VEC_LOAD (pair), mask);

    gum_pointer_scan_emit (matches, pair,
        gum_pointer_scan_cmpeq (value, masked));
  }
}

static void
gum_pointer_scan_process_few (GumPointerScan * self,
                              const gsize * words,
                              gsize n_vectors,
                              GArray * matches)
{
  const gsize * values = self->values;
  guint n = self->n_values;
  GumScanVec mask = GUM_SCAN_VEC_SET1 (self->mask);
  GumScanVec v0 = GUM_SCAN_VEC_SET1 (values[0]);
  GumScanVec v1 = GUM_SCAN_VEC_SET1 (values[1]);
  GumScanVec v2 = GUM_SCAN_VEC_SET1 (values[(n > 2) ? 2 : 0]);
  GumScanVec v3 = GUM_SCAN_VEC_SET1 (values[(n > 3) ? 3 : 0]);
  gsize i;

  for (i = 0; i != n_vectors; i++)
  {
    const gsize * pair = words + i * 2;
    GumScanVec masked = GUM_SCAN_VEC_AND (GUM_SCAN_VEC_LOAD (pair), mask);
    GumScanVec cmp = GUM_SCAN_VEC_OR (
        GUM_SCAN_VEC_OR (gum_pointer_scan_cmpeq (v0, masked),
            gum_pointer_scan_cmpeq (v1, masked)),
        GUM_SCAN_VEC_OR (gum_pointer_scan_cmpeq (v2, masked),
            gum_pointer_scan_cmpeq (v3, masked)));

    gum_pointer_scan_emit (matches, pair, cmp);
  }
}

static void
gum_pointer_scan_process_many (GumPointerScan * self,
                               const gsize * words,
                               gsize n_vectors,
                               GArray * matches)
{
  const gsize * values = self->values;
  guint n_values = self->n_values;
  GumScanVec mask = GUM_SCAN_VEC_SET1 (self->mask);
  GumScanVec * value_vecs = g_newa (GumScanVec, n_values);
  guint v;
  gsize i;

  for (v = 0; v != n_values; v++)
    value_vecs[v] = GUM_SCAN_VEC_SET1 (values[v]);

  for (i = 0; i != n_vectors; i++)
  {
    const gsize * pair = words + i * 2;
    GumScanVec masked = GUM_SCAN_VEC_AND (GUM_SCAN_VEC_LOAD (pair), mask);
    GumScanVec cmp = gum_pointer_scan_cmpeq (value_vecs[0], masked);

    for (v = 1; v != n_values; v++)
    {
      cmp = GUM_SCAN_VEC_OR (cmp,
          gum_pointer_scan_cmpeq (value_vecs[v], masked));
    }

    gum_pointer_scan_emit (matches, pair, cmp);
  }
}

# if defined (HAVE_I386)

static GumScanVec
gum_pointer_scan_cmpeq (GumScanVec value,
                        GumScanVec masked)
{
  GumScanVec eq = _mm_cmpeq_epi32 (masked, value);

  return _mm_and_si128 (eq, _mm_shuffle_epi32 (eq, _MM_SHUFFLE (2, 3, 0, 1)));
}

static void
gum_pointer_scan_emit (GArray * matches,
                       const gsize * pair,
                       GumScanVec cmp)
{
  int lanes = _mm_movemask_epi8 (cmp);

  if ((lanes & 0x00ff) != 0)
    gum_pointer_scan_record_match (matches, pair);
  if ((lanes & 0xff00) != 0)
    gum_pointer_scan_record_match (matches, pair + 1);
}

# elif defined (HAVE_ARM64)

static GumScanVec
gum_pointer_scan_cmpeq (GumScanVec value,
                        GumScanVec masked)
{
  return vceqq_u64 (masked, value);
}

static void
gum_pointer_scan_emit (GArray * matches,
                       const gsize * pair,
                       GumScanVec cmp)
{
  if (vgetq_lane_u64 (cmp, 0) != 0)
    gum_pointer_scan_record_match (matches, pair);
  if (vgetq_lane_u64 (cmp, 1) != 0)
    gum_pointer_scan_record_match (matches, pair + 1);
}

# endif

#endif

static void
gum_pointer_scan_check_word (GumPointerScan * self,
                             const gsize * word,
                             GArray * matches)
{
  gsize masked = *word & self->mask;
  guint v;

  for (v = 0; v != self->n_values; v++)
  {
    if (masked == self->values[v])
    {
      gum_pointer_scan_record_match (matches, word);
      break;
    }
  }
}

static void
gum_pointer_scan_record_match (GArray * matches,
                               const gsize * word)
{
  GumPointerMatch match;

  match.address = GUM_ADDRESS (word);
  match.value = *word;

  g_array_append_val (matches, match);
}

static gint
gum_pointer_match_compare (gconstpointer a,
                           gconstpointer b)
{
  const GumPointerMatch * ma = a;
  const GumPointerMatch * mb = b;

  if (ma->address < mb->address)
    return -1;

  if (ma->address > mb->address)
    return 1;

  return 0;
}

void
gum_ensure_code_readable (gconstpointer address,
                          gsize size)
{
  /*
   * We will make this more generic once it's needed on other OSes.
   */
#ifdef HAVE_ANDROID
  gsize page_size;
  gconstpointer start_page, end_page, cur_page;

  if (gum_android_get_api_level () < 29)
    return;

  page_size = gum_query_page_size ();
  start_page = GSIZE_TO_POINTER (
      GPOINTER_TO_SIZE (address) & ~(page_size - 1));
  end_page = GSIZE_TO_POINTER (
      GPOINTER_TO_SIZE (address + size - 1) & ~(page_size - 1)) + page_size;

  G_LOCK (gum_softened_code_pages);

  if (gum_softened_code_pages == NULL)
    gum_softened_code_pages = g_hash_table_new (NULL, NULL);

  for (cur_page = start_page; cur_page != end_page; cur_page += page_size)
  {
    GumPageProtection prot;

    if (g_hash_table_contains (gum_softened_code_pages, cur_page))
      continue;

    if (!gum_memory_query_protection (cur_page, &prot))
      continue;

    if ((prot & GUM_PAGE_READ) != 0)
    {
      g_hash_table_add (gum_softened_code_pages, (gpointer) cur_page);
      continue;
    }

    if (gum_try_mprotect ((gpointer) cur_page, page_size,
        prot | GUM_PAGE_READ))
      g_hash_table_add (gum_softened_code_pages, (gpointer) cur_page);
  }

  G_UNLOCK (gum_softened_code_pages);
#endif
}

void
gum_mprotect (gpointer address,
              gsize size,
              GumPageProtection prot)
{
  gboolean success;

  success = gum_try_mprotect (address, size, prot);
  if (!success)
    g_abort ();
}

#ifndef GUM_USE_SYSTEM_ALLOC

guint
gum_peek_private_memory_usage (void)
{
  guint total = 0;
  struct mallinfo info;

  info = mspace_mallinfo (gum_mspace_main);
  total += (guint) info.uordblks;

  info = mspace_mallinfo (gum_mspace_internal);
  total += (guint) info.uordblks;

  return total;
}

gpointer
gum_malloc (gsize size)
{
  return mspace_malloc (gum_mspace_main, size);
}

gpointer
gum_malloc0 (gsize size)
{
  return mspace_calloc (gum_mspace_main, 1, size);
}

gsize
gum_malloc_usable_size (gconstpointer mem)
{
  return mspace_usable_size (mem);
}

gpointer
gum_calloc (gsize count,
            gsize size)
{
  return mspace_calloc (gum_mspace_main, count, size);
}

gpointer
gum_realloc (gpointer mem,
             gsize size)
{
  return mspace_realloc (gum_mspace_main, mem, size);
}

gpointer
gum_memalign (gsize alignment,
              gsize size)
{
  return mspace_memalign (gum_mspace_main, alignment, size);
}

gpointer
gum_memdup (gconstpointer mem,
            gsize byte_size)
{
  gpointer result;

  result = mspace_malloc (gum_mspace_main, byte_size);
  memcpy (result, mem, byte_size);

  return result;
}

void
gum_free (gpointer mem)
{
  mspace_free (gum_mspace_main, mem);
}

gpointer
gum_internal_malloc (size_t size)
{
  return mspace_malloc (gum_mspace_internal, size);
}

gpointer
gum_internal_calloc (size_t count,
                     size_t size)
{
  return mspace_calloc (gum_mspace_internal, count, size);
}

gpointer
gum_internal_realloc (gpointer mem,
                      size_t size)
{
  return mspace_realloc (gum_mspace_internal, mem, size);
}

void
gum_internal_free (gpointer mem)
{
  mspace_free (gum_mspace_internal, mem);
}

#else

guint
gum_peek_private_memory_usage (void)
{
  return 0;
}

gpointer
gum_malloc (gsize size)
{
  return malloc (size);
}

gpointer
gum_malloc0 (gsize size)
{
  return calloc (1, size);
}

gsize
gum_malloc_usable_size (gconstpointer mem)
{
  return 0;
}

gpointer
gum_calloc (gsize count,
            gsize size)
{
  return calloc (count, size);
}

gpointer
gum_realloc (gpointer mem,
             gsize size)
{
  return realloc (mem, size);
}

gpointer
gum_memalign (gsize alignment,
              gsize size)
{
  /* TODO: Implement this. */
  g_assert_not_reached ();

  return NULL;
}

gpointer
gum_memdup (gconstpointer mem,
            gsize byte_size)
{
  gpointer result;

  result = malloc (byte_size);
  memcpy (result, mem, byte_size);

  return result;
}

void
gum_free (gpointer mem)
{
  free (mem);
}

gpointer
gum_internal_malloc (size_t size)
{
  return gum_malloc (size);
}

gpointer
gum_internal_calloc (size_t count,
                     size_t size)
{
  return gum_calloc (count, size);
}

gpointer
gum_internal_realloc (gpointer mem,
                      size_t size)
{
  return gum_realloc (mem, size);
}

void
gum_internal_free (gpointer mem)
{
  gum_free (mem);
}

#endif

gboolean
gum_address_spec_is_satisfied_by (const GumAddressSpec * spec,
                                  gconstpointer address)
{
  gsize distance;

  distance =
      ABS ((const guint8 *) spec->near_address - (const guint8 *) address);

  return distance <= spec->max_distance;
}

GumMemoryRange *
gum_memory_range_copy (const GumMemoryRange * range)
{
  return g_slice_dup (GumMemoryRange, range);
}

void
gum_memory_range_free (GumMemoryRange * range)
{
  g_slice_free (GumMemoryRange, range);
}
