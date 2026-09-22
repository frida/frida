/*
 * Copyright (C) 2008-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummemory.h"

#include "gummemory-priv.h"
#include "gumprocess-priv.h"

#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>

typedef struct _GumAllocNearContext GumAllocNearContext;
typedef struct _GumEnumerateFreeRangesContext GumEnumerateFreeRangesContext;

struct _GumAllocNearContext
{
  const GumAddressSpec * spec;
  gsize size;
  gsize alignment;
  gsize page_size;
  GumPageProtection prot;

  gpointer result;
};

struct _GumEnumerateFreeRangesContext
{
  GumFoundRangeFunc func;
  gpointer user_data;
  GumAddress prev_end;
};

static gpointer gum_memory_allocate_internal (gpointer address, gsize size,
    gsize alignment, GumPageProtection prot, gint extra_flags);
static gboolean gum_try_alloc_in_range_if_near_enough (
    const GumRangeDetails * details, gpointer user_data);
static gboolean gum_try_suggest_allocation_base (const GumMemoryRange * range,
    const GumAllocNearContext * ctx, gpointer * allocation_base);
static gpointer gum_allocate_page_aligned (gpointer address, gsize size,
    gint prot, gint extra_flags);
static void gum_enumerate_free_ranges (GumFoundRangeFunc func,
    gpointer user_data);
static gboolean gum_emit_free_range (const GumRangeDetails * details,
    gpointer user_data);

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
  return sysconf (_SC_PAGE_SIZE);
}

gpointer
gum_memory_allocate (gpointer address,
                     gsize size,
                     gsize alignment,
                     GumPageProtection prot)
{
  return gum_memory_allocate_internal (address, size, alignment, prot, 0);
}

static gpointer
gum_memory_allocate_internal (gpointer address,
                              gsize size,
                              gsize alignment,
                              GumPageProtection prot,
                              gint extra_flags)
{
  gsize page_size, allocation_size;
  guint8 * base, * aligned_base;

  address = GUM_ALIGN_POINTER (gpointer, address, alignment);

  page_size = gum_query_page_size ();
  allocation_size = size + (alignment - page_size);
  allocation_size = GUM_ALIGN_SIZE (allocation_size, page_size);

  base = gum_allocate_page_aligned (address, allocation_size,
      _gum_page_protection_to_posix (prot), extra_flags);
  if (base == NULL)
    return NULL;

  aligned_base = GUM_ALIGN_POINTER (guint8 *, base, alignment);

  if (aligned_base != base)
  {
    gsize prefix_size = aligned_base - base;
    gum_memory_free (base, prefix_size);
    allocation_size -= prefix_size;
  }

  if (allocation_size != size)
  {
    gsize suffix_size = allocation_size - size;
    gum_memory_free (aligned_base + size, suffix_size);
    allocation_size -= suffix_size;
  }

  g_assert (allocation_size == size);

  return aligned_base;
}

gpointer
gum_memory_allocate_near (const GumAddressSpec * spec,
                          gsize size,
                          gsize alignment,
                          GumPageProtection prot)
{
  gpointer suggested_base, received_base;
  GumAllocNearContext ctx;

  suggested_base = (spec != NULL) ? spec->near_address : NULL;

  received_base = gum_memory_allocate (suggested_base, size, alignment, prot);
  if (received_base == NULL)
    return NULL;
  if (spec == NULL || gum_address_spec_is_satisfied_by (spec, received_base))
    return received_base;
  gum_memory_free (received_base, size);

  ctx.spec = spec;
  ctx.size = size;
  ctx.alignment = alignment;
  ctx.page_size = gum_query_page_size ();
  ctx.prot = prot;
  ctx.result = NULL;

  gum_enumerate_free_ranges (gum_try_alloc_in_range_if_near_enough, &ctx);

  return ctx.result;
}

static gboolean
gum_try_alloc_in_range_if_near_enough (const GumRangeDetails * details,
                                       gpointer user_data)
{
  GumAllocNearContext * ctx = user_data;
  gpointer suggested_base, received_base;

  if (!gum_try_suggest_allocation_base (details->range, ctx, &suggested_base))
    goto keep_looking;

#ifdef HAVE_FREEBSD
  received_base = gum_memory_allocate_internal (suggested_base, ctx->size,
      ctx->alignment, ctx->prot, MAP_FIXED | MAP_EXCL);
  if (received_base != NULL)
  {
    ctx->result = received_base;
    return FALSE;
  }
#endif

  received_base = gum_memory_allocate (suggested_base, ctx->size,
      ctx->alignment, ctx->prot);
  if (received_base == NULL)
    goto keep_looking;

  if (!gum_address_spec_is_satisfied_by (ctx->spec, received_base))
  {
    gum_memory_free (received_base, ctx->size);
    goto keep_looking;
  }

  ctx->result = received_base;
  return FALSE;

keep_looking:
  return TRUE;
}

static gboolean
gum_try_suggest_allocation_base (const GumMemoryRange * range,
                                 const GumAllocNearContext * ctx,
                                 gpointer * allocation_base)
{
  const gsize allocation_size = ctx->size + (ctx->alignment - ctx->page_size);
  gpointer base;
  gsize mask;

  if (range->size < allocation_size)
    return FALSE;

  mask = ~(ctx->alignment - 1);

  base = GSIZE_TO_POINTER ((range->base_address + ctx->alignment - 1) & mask);
  if (!gum_address_spec_is_satisfied_by (ctx->spec, base))
  {
    base = GSIZE_TO_POINTER ((range->base_address + range->size -
        allocation_size) & mask);
    if (!gum_address_spec_is_satisfied_by (ctx->spec, base))
      return FALSE;
  }

  *allocation_base = base;
  return TRUE;
}

static gpointer
gum_allocate_page_aligned (gpointer address,
                           gsize size,
                           gint prot,
                           gint extra_flags)
{
  gpointer result;
  const gint base_flags = MAP_PRIVATE | MAP_ANONYMOUS | extra_flags;
  gint region_flags = 0;

#if defined (HAVE_FREEBSD) && GLIB_SIZEOF_VOID_P == 8
  if (address != NULL &&
      GPOINTER_TO_SIZE (address) + size < G_MAXUINT32)
  {
    region_flags |= MAP_32BIT;
  }
#endif

  result = mmap (address, size, prot, base_flags | region_flags, -1, 0);

#if defined (HAVE_FREEBSD) && GLIB_SIZEOF_VOID_P == 8
  if (result == MAP_FAILED && (region_flags & MAP_32BIT) != 0)
  {
    result = mmap (NULL, size, prot, base_flags | region_flags, -1, 0);
    if (result == MAP_FAILED)
      result = mmap (address, size, prot, base_flags, -1, 0);
  }
#endif

  return (result != MAP_FAILED) ? result : NULL;
}

gpointer
gum_memory_allocate_bookkeeping (gsize size,
                                 gsize alignment)
{
  return gum_memory_allocate (NULL, size, alignment, GUM_PAGE_RW);
}

gboolean
gum_memory_free (gpointer address,
                 gsize size)
{
  return munmap (address, size) == 0;
}

gboolean
gum_memory_release (gpointer address,
                    gsize size)
{
  return gum_memory_free (address, size);
}

gboolean
gum_memory_recommit (gpointer address,
                     gsize size,
                     GumPageProtection prot)
{
  gboolean success;

  success = gum_try_mprotect (address, size, prot);

  if (success && prot == GUM_PAGE_NO_ACCESS)
    gum_memory_discard (address, size);

  return TRUE;
}

gboolean
gum_memory_discard (gpointer address,
                    gsize size)
{
#if defined (HAVE_MADVISE)
  return madvise (address, size, MADV_DONTNEED) == 0;
#elif defined (HAVE_POSIX_MADVISE)
  int advice;

# ifdef POSIX_MADV_DISCARD_NP
  advice = POSIX_MADV_DISCARD_NP;
# else
  advice = POSIX_MADV_DONTNEED;
# endif

  return posix_madvise (address, size, advice) == 0;
#else
# error FIXME
#endif
}

gboolean
gum_memory_decommit (gpointer address,
                     gsize size)
{
  return mmap (address, size, PROT_NONE,
      MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) == address;
}

static void
gum_enumerate_free_ranges (GumFoundRangeFunc func,
                           gpointer user_data)
{
  GumEnumerateFreeRangesContext ctx = { func, user_data, 0 };

  _gum_process_enumerate_ranges (GUM_PAGE_NO_ACCESS, gum_emit_free_range, &ctx);
}

static gboolean
gum_emit_free_range (const GumRangeDetails * details,
                     gpointer user_data)
{
  GumEnumerateFreeRangesContext * ctx =
      (GumEnumerateFreeRangesContext *) user_data;
  const GumMemoryRange * range = details->range;
  GumAddress start = range->base_address;
  GumAddress end = start + range->size;
  gboolean carry_on = TRUE;

  if (ctx->prev_end != 0)
  {
    GumAddress gap_size;

    gap_size = start - ctx->prev_end;

    if (gap_size > 0)
    {
      GumRangeDetails d;
      GumMemoryRange r;

      d.range = &r;
      d.protection = GUM_PAGE_NO_ACCESS;
      d.file = NULL;

      r.base_address = ctx->prev_end;
      r.size = gap_size;

      carry_on = ctx->func (&d, ctx->user_data);
    }
  }

  ctx->prev_end = end;

  return carry_on;
}

gint
_gum_page_protection_to_posix (GumPageProtection prot)
{
  gint posix_prot = PROT_NONE;

  if ((prot & GUM_PAGE_READ) != 0)
    posix_prot |= PROT_READ;
  if ((prot & GUM_PAGE_WRITE) != 0)
    posix_prot |= PROT_WRITE;
  if ((prot & GUM_PAGE_EXECUTE) != 0)
    posix_prot |= PROT_EXEC;

  return posix_prot;
}

