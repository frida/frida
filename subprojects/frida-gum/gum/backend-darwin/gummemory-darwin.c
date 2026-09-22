/*
 * Copyright (C) 2010-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2025-2026 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummemory.h"

#include "gum/gumdarwin.h"
#include "gumdarwin-priv.h"
#include "gummemory-priv.h"

#include <errno.h>
#include <unistd.h>
#include <libkern/OSCacheControl.h>
#include <mach/mach.h>
#include <sys/mman.h>
#include <sys/sysctl.h>

#define GUM_PAGE_BLOCK_BYTES_CAPACITY 8

typedef gboolean (* GumFoundFreeRangeFunc) (const GumMemoryRange * range,
    gpointer user_data);

typedef struct _GumAllocNearContext GumAllocNearContext;

struct _GumAllocNearContext
{
  const GumAddressSpec * spec;
  gsize size;
  gsize alignment;
  gsize page_size;
  GumPageProtection prot;

  gpointer result;
};

extern kern_return_t mach_vm_page_info (vm_map_read_t target_task,
    mach_vm_address_t address, vm_page_info_flavor_t flavor,
    vm_page_info_t info, mach_msg_type_number_t * infoCnt);

static kern_return_t gum_mach_vm_protect (vm_map_t target_task,
    mach_vm_address_t address, mach_vm_size_t size, boolean_t set_maximum,
    vm_prot_t new_protection);
static gpointer gum_allocate_page_aligned (gpointer address, gsize size,
    gint prot);
static gboolean gum_try_alloc_in_range_if_near_enough (
    const GumMemoryRange * range, gpointer user_data);
static gboolean gum_try_suggest_allocation_base (const GumMemoryRange * range,
    const GumAllocNearContext * ctx, gpointer * allocation_base);
static gint gum_page_protection_to_bsd (GumPageProtection prot);
static gboolean gum_page_is_freshly_allocated (gpointer page, gsize size);

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
  return getpagesize ();
}

gboolean
gum_darwin_query_ptrauth_support (mach_port_t task,
                                  GumPtrauthSupport * ptrauth_support)
{
#ifdef HAVE_ARM64
  GumDarwinAllImageInfos infos;
  GumAddress actual_ptr, stripped_ptr;

  if (task == mach_task_self ())
  {
    *ptrauth_support = gum_query_ptrauth_support ();
    return TRUE;
  }

  if (!gum_darwin_query_all_image_infos (task, &infos, NULL))
    return FALSE;

  actual_ptr = infos.notification_address;
  stripped_ptr = actual_ptr & G_GUINT64_CONSTANT (0x7fffffffff);

  *ptrauth_support = (stripped_ptr != actual_ptr)
      ? GUM_PTRAUTH_SUPPORTED
      : GUM_PTRAUTH_UNSUPPORTED;
#else
  *ptrauth_support = GUM_PTRAUTH_UNSUPPORTED;
#endif

  return TRUE;
}

gboolean
gum_darwin_query_page_size (mach_port_t task,
                            guint * page_size)
{
  int pid;
  kern_return_t kr;
  GumCpuType cpu_type;

  if (task == mach_task_self ())
  {
    *page_size = gum_query_page_size ();
    return TRUE;
  }

  /* FIXME: any way we can probe it without access to the task's host port? */
  kr = pid_for_task (task, &pid);
  if (kr != KERN_SUCCESS)
    return FALSE;

  if (!gum_darwin_cpu_type_from_pid (pid, &cpu_type))
    return FALSE;

  switch (cpu_type)
  {
    case GUM_CPU_IA32:
    case GUM_CPU_AMD64:
      *page_size = 4096;
      break;
    case GUM_CPU_ARM:
    {
      if (gum_darwin_check_xnu_version (3216, 0, 0))
      {
        char buf[256];
        size_t size;
        G_GNUC_UNUSED int res;
        guint64 hw_page_size = 0;

        size = sizeof (buf);
        res = sysctlbyname ("hw.pagesize", buf, &size, NULL, 0);
        g_assert (res == 0);

        if (size == 8)
          hw_page_size = *((guint64 *) buf);
        else if (size == 4)
          hw_page_size = *((guint32 *) buf);
        else
          g_assert_not_reached ();

        *page_size = hw_page_size;
      }
      else
      {
        *page_size = 4096;
      }

      break;
    }
    case GUM_CPU_ARM64:
      *page_size = 16384;
      break;
    default:
      g_assert_not_reached ();
  }

  return TRUE;
}

static void
gum_enumerate_free_ranges (GumFoundFreeRangeFunc func,
                           gpointer user_data)
{
  mach_port_t self;
  guint page_size, index;
  mach_vm_address_t address = MACH_VM_MIN_ADDRESS;
  GumAddress prev_end = 0;

  self = mach_task_self ();

  page_size = gum_query_page_size ();

  for (index = 0; TRUE; index++)
  {
    mach_vm_size_t size = 0;
    natural_t depth = 0;
    vm_region_submap_info_data_64_t info;
    mach_msg_type_number_t info_count = VM_REGION_SUBMAP_INFO_COUNT_64;
    kern_return_t kr;

    kr = mach_vm_region_recurse (self, &address, &size, &depth,
        (vm_region_recurse_info_t) &info, &info_count);
    if (kr != KERN_SUCCESS)
    {
      if (prev_end != 0)
      {
        GumAddress max_address;
        GumMemoryRange r;

#if GLIB_SIZEOF_VOID_P == 4
        max_address = 0xffffffff;
#elif defined (HAVE_I386)
        max_address = G_GUINT64_CONSTANT (0x0001000000000000);
#elif defined (HAVE_ARM64)
        max_address = G_GUINT64_CONSTANT (0x0000000200000000);
#endif

        if (max_address > prev_end)
        {
          r.base_address = prev_end;
          r.size = max_address - prev_end;

          func (&r, user_data);
        }
      }

      break;
    }

    if (index == 0 && address > page_size)
    {
      GumMemoryRange r;

      r.base_address = page_size;
      r.size = address - page_size;

      if (!func (&r, user_data))
        break;
    }

    if (prev_end != 0)
    {
      gint64 gap_size;

      gap_size = address - prev_end;

      if (gap_size > 0)
      {
        GumMemoryRange r;

        r.base_address = prev_end;
        r.size = gap_size;

        if (!func (&r, user_data))
          break;
      }
    }

    prev_end = address + size;

    address += size;
  }
}

gboolean
gum_memory_is_readable (gconstpointer address,
                        gsize len)
{
  gboolean is_readable;
  guint8 * bytes;
  gsize n_bytes_read;

  bytes = gum_memory_read (address, len, &n_bytes_read);
  is_readable = bytes != NULL && n_bytes_read == len;
  g_free (bytes);

  return is_readable;
}

gboolean
gum_memory_query_protection (gconstpointer address,
                             GumPageProtection * prot)
{
  return gum_darwin_query_protection (mach_task_self (), GUM_ADDRESS (address),
      prot);
}

guint8 *
gum_memory_read (gconstpointer address,
                 gsize len,
                 gsize * n_bytes_read)
{
  return gum_darwin_read (mach_task_self (), GUM_ADDRESS (address), len,
      n_bytes_read);
}

gboolean
gum_memory_write (gpointer address,
                  const guint8 * bytes,
                  gsize len)
{
  return gum_darwin_write (mach_task_self (), GUM_ADDRESS (address), bytes,
      len);
}

guint8 *
gum_darwin_read (mach_port_t task,
                 GumAddress address,
                 gsize len,
                 gsize * n_bytes_read)
{
  guint page_size;
  guint8 * result;
  gsize offset;
  kern_return_t kr;

  if (!gum_darwin_query_page_size (task, &page_size))
    return NULL;

  result = g_malloc (len);
  offset = 0;

  while (offset != len)
  {
    GumAddress chunk_address, page_address;
    gsize chunk_size, page_offset;

    chunk_address = address + offset;
    page_address = chunk_address & ~(GumAddress) (page_size - 1);
    page_offset = chunk_address - page_address;
    chunk_size = MIN (len - offset, page_size - page_offset);

#if defined (HAVE_IOS) || defined (HAVE_TVOS)
    mach_vm_size_t n_bytes_read;

    /* mach_vm_read corrupts memory on iOS */
    kr = mach_vm_read_overwrite (task, chunk_address, chunk_size,
        (vm_address_t) (result + offset), &n_bytes_read);
    if (kr != KERN_SUCCESS)
      break;
    g_assert (n_bytes_read == chunk_size);
#else
    vm_offset_t result_data;
    mach_msg_type_number_t result_size;

    /* mach_vm_read_overwrite leaks memory on macOS */
    kr = mach_vm_read (task, page_address, page_size,
        &result_data, &result_size);
    if (kr != KERN_SUCCESS)
      break;
    g_assert (result_size == page_size);
    memcpy (result + offset, (gpointer) (result_data + page_offset),
        chunk_size);
    mach_vm_deallocate (mach_task_self (), result_data, result_size);
#endif

    offset += chunk_size;
  }

  if (offset == 0)
  {
    g_free (result);
    result = NULL;
  }

  if (n_bytes_read != NULL)
    *n_bytes_read = offset;

  return result;
}

gboolean
gum_darwin_write (mach_port_t task,
                  GumAddress address,
                  const guint8 * bytes,
                  gsize len)
{
  kern_return_t kr;

  kr = mach_vm_write (task, address, (vm_offset_t) bytes, len);

  return (kr == KERN_SUCCESS);
}

gboolean
gum_memory_can_remap_writable (void)
{
  return gum_darwin_is_debugger_mapping_enforced ();
}

gpointer
gum_memory_try_remap_writable_pages (gpointer first_page,
                                     guint n_pages)
{
  mach_port_t task;
  gsize size;
  vm_address_t writable_address;
  vm_prot_t cur_prot, max_prot;
  GumMemoryRange range;

  task = mach_task_self ();
  size = n_pages * gum_query_page_size ();

  writable_address = 0;
  /* TODO: Implement shim. */
  if (vm_remap (task, &writable_address, size, 0, VM_FLAGS_ANYWHERE, task,
      GPOINTER_TO_SIZE (first_page), FALSE, &cur_prot, &max_prot,
      VM_INHERIT_NONE) != 0)
  {
    return NULL;
  }

  if (gum_mach_vm_protect (task, writable_address, size, FALSE,
      VM_PROT_READ | VM_PROT_WRITE) != KERN_SUCCESS)
  {
    mach_vm_deallocate (task, writable_address, size);
    return NULL;
  }

  range.base_address = writable_address;
  range.size = size;
  gum_cloak_add_range (&range);

  return GSIZE_TO_POINTER (writable_address);
}

void
gum_memory_dispose_writable_pages (gpointer first_page,
                                   guint n_pages)
{
  mach_port_t task;
  gsize size;
  GumMemoryRange range;

  task = mach_task_self ();
  size = n_pages * gum_query_page_size ();

  range.base_address = GUM_ADDRESS (first_page);
  range.size = size;
  gum_cloak_remove_range (&range);

  mach_vm_deallocate (task, GPOINTER_TO_SIZE (first_page), size);
}

static kern_return_t
gum_mach_vm_protect (vm_map_t target_task,
                     mach_vm_address_t address,
                     mach_vm_size_t size,
                     boolean_t set_maximum,
                     vm_prot_t new_protection)
{
#if defined (HAVE_ARM)
  kern_return_t result;
  guint32 args[] = {
    target_task,
    address & 0xffffffff,
    (address >> 32) & 0xffffffff,
    size & 0xffffffff,
    (size >> 32) & 0xffffffff,
    set_maximum,
    new_protection,
    0
  };

  /* FIXME: Should avoid clobbering R7, which is reserved. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winline-asm"

  asm volatile (
      "push {r0, r1, r2, r3, r4, r5, r6, r7, r12}\n\t"
      "ldmdb %1!, {r0, r1, r2, r3, r4, r5, r6, r7}\n\t"
      "mvn r12, 0xd\n\t"
      "svc 0x80\n\t"
      "mov %0, r0\n\t"
      "pop {r0, r1, r2, r3, r4, r5, r6, r7, r12}\n\t"
      : "=r" (result)
      : "r" (args + G_N_ELEMENTS (args))
      : "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r12"
  );

#pragma clang diagnostic pop

  return result;
#elif defined (HAVE_ARM64)
  kern_return_t result;

  asm volatile (
      "sub sp, sp, #16 * 3\n\t"
      "stp x0, x1, [sp, #16 * 0]\n\t"
      "stp x2, x3, [sp, #16 * 1]\n\t"
      "stp x4, x16, [sp, #16 * 2]\n\t"
      "mov x0, %1\n\t"
      "mov x1, %2\n\t"
      "mov x2, %3\n\t"
      "mov x3, %4\n\t"
      "mov x4, %5\n\t"
      "movn x16, 0xd\n\t"
      "svc 0x80\n\t"
      "mov %w0, w0\n\t"
      "ldp x0, x1, [sp, #16 * 0]\n\t"
      "ldp x2, x3, [sp, #16 * 1]\n\t"
      "ldp x4, x16, [sp, #16 * 2]\n\t"
      "add sp, sp, #16 * 3\n\t"
      : "=r" (result)
      : "r" ((gsize) target_task),
        "r" (address),
        "r" (size),
        "r" ((gsize) set_maximum),
        "r" ((gsize) new_protection)
      : "x0", "x1", "x2", "x3", "x4", "x16"
  );

  return result;
#else
  return mach_vm_protect (target_task, address, size, set_maximum,
      new_protection);
#endif
}

gboolean
gum_try_mprotect (gpointer address,
                  gsize size,
                  GumPageProtection prot)
{
  gsize page_size;
  gpointer aligned_address;
  gsize aligned_size;
  vm_prot_t mach_prot;
  kern_return_t kr;

  g_assert (size != 0);

  page_size = gum_query_page_size ();
  aligned_address = GSIZE_TO_POINTER (
      GPOINTER_TO_SIZE (address) & ~(page_size - 1));
  aligned_size =
      (1 + ((address + size - 1 - aligned_address) / page_size)) * page_size;
  mach_prot = gum_page_protection_to_mach (prot);

  kr = gum_mach_vm_protect (mach_task_self (),
      GPOINTER_TO_SIZE (aligned_address), aligned_size, FALSE, mach_prot);

  return kr == KERN_SUCCESS;
}

void
gum_clear_cache (gpointer address,
                 gsize size)
{
  sys_icache_invalidate (address, size);
  sys_dcache_flush (address, size);
}

gpointer
gum_memory_allocate (gpointer address,
                     gsize size,
                     gsize alignment,
                     GumPageProtection prot)
{
  gsize page_size, allocation_size;
  guint8 * base, * aligned_base;

  address = GUM_ALIGN_POINTER (gpointer, address, alignment);

  page_size = gum_query_page_size ();
  allocation_size = size + (alignment - page_size);
  allocation_size = GUM_ALIGN_SIZE (allocation_size, page_size);

  base = gum_allocate_page_aligned (address, allocation_size,
      gum_page_protection_to_bsd (prot));
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

static gpointer
gum_allocate_page_aligned (gpointer address,
                           gsize size,
                           gint prot)
{
  gpointer result;

  result = mmap (address, size, prot, MAP_PRIVATE | MAP_ANONYMOUS,
      VM_MAKE_TAG (255), 0);
  if (result == MAP_FAILED)
    return NULL;

#if (defined (HAVE_IOS) || defined (HAVE_TVOS)) && !defined (HAVE_I386)
  {
    gboolean need_checkra1n_quirk;

    need_checkra1n_quirk = prot == (PROT_READ | PROT_WRITE | PROT_EXEC) &&
        gum_query_rwx_support () == GUM_RWX_ALLOCATIONS_ONLY;
    if (need_checkra1n_quirk)
    {
      gum_mach_vm_protect (mach_task_self (), GPOINTER_TO_SIZE (result), size,
          FALSE, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE);
    }
  }
#endif

  return result;
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
gum_try_alloc_in_range_if_near_enough (const GumMemoryRange * range,
                                       gpointer user_data)
{
  GumAllocNearContext * ctx = user_data;
  gpointer suggested_base, received_base;

  if (!gum_try_suggest_allocation_base (range, ctx, &suggested_base))
    goto keep_looking;

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
  int res;

  do
    res = madvise (address, size, MADV_FREE_REUSE);
  while (res == -1 && errno == EAGAIN);

  return TRUE;
}

gboolean
gum_memory_discard (gpointer address,
                    gsize size)
{
  int res;

  do
    res = madvise (address, size, MADV_FREE_REUSABLE);
  while (res == -1 && errno == EAGAIN);

  if (res == -1)
    res = madvise (address, size, MADV_DONTNEED);

  return res == 0;
}

gboolean
gum_memory_decommit (gpointer address,
                     gsize size)
{
  return mmap (address, size, PROT_NONE,
      MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) == address;
}

GumPageProtection
gum_page_protection_from_mach (vm_prot_t native_prot)
{
  GumPageProtection prot = 0;

  if ((native_prot & VM_PROT_READ) == VM_PROT_READ)
    prot |= GUM_PAGE_READ;
  if ((native_prot & VM_PROT_WRITE) == VM_PROT_WRITE)
    prot |= GUM_PAGE_WRITE;
  if ((native_prot & VM_PROT_EXECUTE) == VM_PROT_EXECUTE)
    prot |= GUM_PAGE_EXECUTE;

  return prot;
}

vm_prot_t
gum_page_protection_to_mach (GumPageProtection prot)
{
  vm_prot_t mach_prot = VM_PROT_NONE;

  if ((prot & GUM_PAGE_READ) != 0)
    mach_prot |= VM_PROT_READ;
  if ((prot & GUM_PAGE_WRITE) != 0)
    mach_prot |= VM_PROT_WRITE | VM_PROT_COPY;
  if ((prot & GUM_PAGE_EXECUTE) != 0)
    mach_prot |= VM_PROT_EXECUTE;

  return mach_prot;
}

static gint
gum_page_protection_to_bsd (GumPageProtection prot)
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

static gboolean
gum_memory_is_codesign_out_of_the_way (gpointer page)
{
  gboolean result;
  mach_port_t task;
  struct vm_page_info_basic page_info;
  mach_msg_type_number_t count = VM_PAGE_INFO_BASIC_COUNT;
  gsize size;
  vm_address_t writable_address;
  vm_prot_t cur_prot, max_prot;

  size = gum_query_page_size ();

  if (gum_page_is_freshly_allocated (page, size))
    return FALSE;

  task = mach_task_self ();

  /* TODO: Implement shim. */
  mach_vm_page_info (task, GPOINTER_TO_SIZE (page), VM_PAGE_INFO_BASIC,
      (int *) &page_info, &count);

  if ((page_info.disposition &
      (VM_PAGE_QUERY_PAGE_CS_VALIDATED |
      VM_PAGE_QUERY_PAGE_CS_TAINTED |
      VM_PAGE_QUERY_PAGE_CS_NX)) != 0)
  {
    return FALSE;
  }

  writable_address = 0;
  if (vm_remap (task, &writable_address, size, 0, VM_FLAGS_ANYWHERE, task,
      GPOINTER_TO_SIZE (page), FALSE, &cur_prot, &max_prot,
      VM_INHERIT_NONE) != 0)
  {
    return FALSE;
  }

  result = gum_mach_vm_protect (task, writable_address, size, FALSE,
      VM_PROT_READ | VM_PROT_WRITE) == KERN_SUCCESS;

  mach_vm_deallocate (task, writable_address, size);

  return result;
}

static gboolean
gum_page_is_freshly_allocated (gpointer page,
                               gsize size)
{
  guint8 vec = 0;

  /* TODO: Implement shim. */
  if (mincore (page, size, (char *) &vec) != 0)
    return FALSE;

  return (vec & MINCORE_ANONYMOUS) &&
      ((vec & (MINCORE_INCORE | MINCORE_REFERENCED |
          MINCORE_MODIFIED | MINCORE_PAGED_OUT)) == 0);
}

void
_gum_page_plan_builder_init (GumPagePlanBuilder * self)
{
  self->page_blocks = g_array_new (FALSE, FALSE, sizeof (GumPageBlock));
}

void
_gum_page_plan_builder_free (GumPagePlanBuilder * self)
{
  guint i;

  if (self == NULL)
    return;

  for (i = 0; i != self->page_blocks->len; i++)
  {
    GumPageBlock * block = &g_array_index (self->page_blocks, GumPageBlock, i);

    g_byte_array_unref (block->bytes);
  }

  g_array_unref (self->page_blocks);
}

void
_gum_page_plan_builder_add_page (GumPagePlanBuilder * self,
                                 gpointer target_page)
{
  _gum_page_plan_builder_add_pages (self, target_page, 1);
}

void
_gum_page_plan_builder_add_pages (GumPagePlanBuilder * self,
                                  gpointer base,
                                  gsize n_pages)
{
  GArray * blocks = self->page_blocks;
  gsize page_size;
  gpointer end, page;

  page_size = gum_query_page_size ();

  end = base + n_pages * page_size;
  for (page = base; page != end; page += page_size)
  {
    GumPageBlock * current;
    guint8 byte;

    if (gum_memory_is_codesign_out_of_the_way (page))
      continue;

    current = (blocks->len != 0)
        ? &g_array_index (blocks, GumPageBlock, blocks->len - 1)
        : NULL;

    if (current == NULL || current->end != page)
    {
      GumPageBlock b;

      b.start = page;
      b.end = page;
      b.bytes = g_byte_array_sized_new (GUM_PAGE_BLOCK_BYTES_CAPACITY);

      g_array_append_val (blocks, b);

      current = &g_array_index (blocks, GumPageBlock, blocks->len - 1);
    }

    current->end = page + page_size;

    if ((current->bytes->len % 2) == 0)
      byte = *(((guint8 *) page) + page_size - 1);
    else
      byte = *(guint8 *) page;
    g_byte_array_append (current->bytes, &byte, sizeof (byte));
  }

  g_assert (blocks->len == 0 ||
      g_array_index (blocks, GumPageBlock, blocks->len - 1).bytes->len != 0);
}

gboolean
_gum_page_plan_builder_post (GumPagePlanBuilder * self)
{
  GArray * blocks = self->page_blocks;
  guint32 n_blocks;
  GByteArray * buffer;
  guint32 i;
  gsize raw_result;

  /*
   * Plan packet structure:
   *
   * (guint32) n_blocks
   *    (guint64) start
   *    (guint32) n_pages
   *      (guint8) byte
   *      ...
   *    ...
   */

  n_blocks = blocks->len;
  if (n_blocks == 0)
    return TRUE;

  buffer = g_byte_array_sized_new (4 +
      (n_blocks * (8 + 4 + GUM_PAGE_BLOCK_BYTES_CAPACITY)));

  g_byte_array_append (buffer, (guint8 *) &n_blocks, sizeof (n_blocks));

  for (i = 0; i != n_blocks; i++)
  {
    GumPageBlock * block;
    guint32 n_pages;

    block = &g_array_index (blocks, GumPageBlock, i);

    g_byte_array_append (buffer, (guint8 *) &block->start,
        sizeof (block->start));

    n_pages = block->bytes->len;
    g_byte_array_append (buffer, (guint8 *) &n_pages, sizeof (n_pages));

    g_byte_array_append (buffer, block->bytes->data, block->bytes->len);
  }

#ifdef HAVE_ARM64
  asm volatile (
      "mov x0, #0\n\t"
      "mov x1, #1337\n\t"
      "mov x2, #1337\n\t"
      "mov x3, #3\n\t"
      "mov x4, %1\n\t"
      "mov x5, %2\n\t"
      "brk #1337\n\t"
      "mov %0, x0\n\t"
      : "=r" (raw_result)
      : "r" ((gsize) buffer->len),
        "r" (GPOINTER_TO_SIZE (buffer->data))
      : "x0", "x1", "x2", "x3", "x4", "x5"
  );
#else
  g_abort ();
#endif

  g_byte_array_unref (buffer);

  return raw_result == 0x1337;
}
