/*
 * Copyright (C) 2008-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2025 Kenjiro Ichise <ichise@doranekosystems.com>
 * Copyright (C) 2025 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummemory.h"

#include "gumlinux-priv.h"
#include "gummemory-priv.h"
#include "gum/gumlinux.h"
#include "valgrind.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/uio.h>

#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
# define GUM_SYS_PROCESS_VM_READV   347
# define GUM_SYS_PROCESS_VM_WRITEV  348
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
# define GUM_SYS_PROCESS_VM_READV   310
# define GUM_SYS_PROCESS_VM_WRITEV  311
#elif defined (HAVE_ARM)
# define GUM_SYS_PROCESS_VM_READV   (__NR_SYSCALL_BASE + 376)
# define GUM_SYS_PROCESS_VM_WRITEV  (__NR_SYSCALL_BASE + 377)
#elif defined (HAVE_ARM64)
# define GUM_SYS_PROCESS_VM_READV   270
# define GUM_SYS_PROCESS_VM_WRITEV  271
#elif defined (HAVE_MIPS)
# if _MIPS_SIM == _MIPS_SIM_ABI32
#  define GUM_SYS_PROCESS_VM_READV  (__NR_Linux + 345)
#  define GUM_SYS_PROCESS_VM_WRITEV (__NR_Linux + 346)
# elif _MIPS_SIM == _MIPS_SIM_ABI64
#  define GUM_SYS_PROCESS_VM_READV  (__NR_Linux + 304)
#  define GUM_SYS_PROCESS_VM_WRITEV (__NR_Linux + 305)
# elif _MIPS_SIM == _MIPS_SIM_NABI32
#  define GUM_SYS_PROCESS_VM_READV  (__NR_Linux + 309)
#  define GUM_SYS_PROCESS_VM_WRITEV (__NR_Linux + 310)
# else
#  error Unexpected MIPS ABI
# endif
#else
# error FIXME
#endif

#define GUM_PROCMAP_QUERY \
    _IOWR ('f', 17, GumProcmapQuery)

#define GUM_PROCMAP_QUERY_VMA_READABLE         0x01
#define GUM_PROCMAP_QUERY_VMA_WRITABLE         0x02
#define GUM_PROCMAP_QUERY_VMA_EXECUTABLE       0x04
#define GUM_PROCMAP_QUERY_COVERING_OR_NEXT_VMA 0x10

typedef struct _GumProcmapQuery GumProcmapQuery;

struct _GumProcmapQuery
{
  guint64 size;
  guint64 query_flags;
  guint64 query_addr;
  guint64 vma_start;
  guint64 vma_end;
  guint64 vma_flags;
  guint64 vma_page_size;
  guint64 vma_offset;
  guint64 inode;
  guint32 dev_major;
  guint32 dev_minor;
  guint32 vma_name_size;
  guint32 build_id_size;
  guint64 vma_name_addr;
  guint64 build_id_addr;
};

static gboolean gum_memory_get_protection (gconstpointer address, gsize n,
    gsize * size, GumPageProtection * prot);
static gboolean gum_memory_get_protection_using_procmap_query (
    gconstpointer address, gboolean * success, gsize * size,
    GumPageProtection * prot);
static gboolean gum_memory_query_protections_using_procmap_query (
    GPtrArray * sorted_pages, GumPageProtection * protections);
static gint gum_procmap_query_open (void);
static gboolean gum_query_vma_using_procmap_query (gint fd, gsize address,
    GumProcmapQuery * query);
static GumPageProtection gum_page_protection_from_procmap_query_flags (
    guint64 vma_flags);

static gssize gum_libc_process_vm_readv (pid_t pid, const struct iovec * local,
    gulong num_local, const struct iovec * remote, gulong num_remote,
    gulong flags);
static gssize gum_libc_process_vm_writev (pid_t pid, const struct iovec * local,
    gulong num_local, const struct iovec * remote, gulong num_remote,
    gulong flags);

static gint gum_procmap_query_supported = -1;

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

void
_gum_memory_query_protections (GPtrArray * sorted_pages,
                               GumPageProtection * protections)
{
  guint i;
  GumProcMapsIter iter;
  const gchar * line;

  for (i = 0; i != sorted_pages->len; i++)
    protections[i] = GUM_PAGE_RX;

  if (gum_memory_query_protections_using_procmap_query (sorted_pages,
        protections))
  {
    return;
  }

  gum_proc_maps_iter_init_for_self (&iter);

  i = 0;
  while (i != sorted_pages->len && gum_proc_maps_iter_next (&iter, &line))
  {
    gpointer start, end;
    gchar protection[4 + 1];
    GumPageProtection prot;

    sscanf (line, "%p-%p %s ", &start, &end, protection);

    while (i != sorted_pages->len &&
        g_ptr_array_index (sorted_pages, i) < start)
    {
      i++;
    }

    prot = GUM_PAGE_NO_ACCESS;
    if (protection[0] == 'r')
      prot |= GUM_PAGE_READ;
    if (protection[1] == 'w')
      prot |= GUM_PAGE_WRITE;
    if (protection[2] == 'x')
      prot |= GUM_PAGE_EXECUTE;

    while (i != sorted_pages->len &&
        g_ptr_array_index (sorted_pages, i) < end)
    {
      protections[i] = prot;
      i++;
    }
  }

  gum_proc_maps_iter_destroy (&iter);
}

static gboolean
gum_memory_query_protections_using_procmap_query (
    GPtrArray * sorted_pages,
    GumPageProtection * protections)
{
  gboolean success = FALSE;
  gint fd;
  guint i;

  fd = gum_procmap_query_open ();
  if (fd == -1)
    return FALSE;

  i = 0;
  while (i != sorted_pages->len)
  {
    gpointer page = g_ptr_array_index (sorted_pages, i);
    GumProcmapQuery query = { 0, };
    GumPageProtection prot;

    if (!gum_query_vma_using_procmap_query (fd, GPOINTER_TO_SIZE (page),
        &query))
      goto beach;

    if (query.vma_end == 0)
      break;

    prot = gum_page_protection_from_procmap_query_flags (query.vma_flags);

    while (i != sorted_pages->len &&
        GPOINTER_TO_SIZE (g_ptr_array_index (sorted_pages, i)) <
          query.vma_start)
    {
      i++;
    }

    while (i != sorted_pages->len &&
        GPOINTER_TO_SIZE (g_ptr_array_index (sorted_pages, i)) <
          query.vma_end)
    {
      protections[i] = prot;
      i++;
    }
  }

  success = TRUE;

beach:
  close (fd);

  return success;
}

static gint
gum_procmap_query_open (void)
{
  if (gum_procmap_query_supported == 0)
    return -1;

  return open ("/proc/self/maps", O_RDONLY | O_CLOEXEC);
}

static gboolean
gum_query_vma_using_procmap_query (gint fd,
                                   gsize address,
                                   GumProcmapQuery * query)
{
  query->size = sizeof (GumProcmapQuery);
  query->query_flags = GUM_PROCMAP_QUERY_COVERING_OR_NEXT_VMA;
  query->query_addr = address;

  if (ioctl (fd, GUM_PROCMAP_QUERY, query) == -1)
  {
    if (errno == ENOENT)
    {
      query->vma_start = 0;
      query->vma_end = 0;
    }
    else
    {
      if (gum_procmap_query_supported == -1)
        gum_procmap_query_supported = 0;
      return FALSE;
    }
  }

  gum_procmap_query_supported = 1;

  return TRUE;
}

static GumPageProtection
gum_page_protection_from_procmap_query_flags (guint64 vma_flags)
{
  GumPageProtection prot = GUM_PAGE_NO_ACCESS;

  if ((vma_flags & GUM_PROCMAP_QUERY_VMA_READABLE) != 0)
    prot |= GUM_PAGE_READ;
  if ((vma_flags & GUM_PROCMAP_QUERY_VMA_WRITABLE) != 0)
    prot |= GUM_PAGE_WRITE;
  if ((vma_flags & GUM_PROCMAP_QUERY_VMA_EXECUTABLE) != 0)
    prot |= GUM_PAGE_EXECUTE;

  return prot;
}

guint8 *
gum_memory_read (gconstpointer address,
                 gsize len,
                 gsize * n_bytes_read)
{
  guint8 * result = NULL;
  gsize result_len = 0;
  static gboolean kernel_feature_likely_enabled = TRUE;
  gboolean still_pending = TRUE;

  if (kernel_feature_likely_enabled && gum_linux_check_kernel_version (3, 2, 0))
  {
    gssize n;
    struct iovec local = {
      .iov_base = g_malloc (len),
      .iov_len = len
    };
    struct iovec remote = {
      .iov_base = (void *) address,
      .iov_len = len
    };

    n = gum_libc_process_vm_readv (getpid (), &local, 1, &remote, 1, 0);
    if (n > 0)
    {
      result_len = n;
      result = local.iov_base;
      if (result_len != len)
        result = g_realloc (result, result_len);
    }
    else
    {
      g_free (local.iov_base);
    }

    if (n == -1 && errno == ENOSYS)
      kernel_feature_likely_enabled = FALSE;
    else
      still_pending = FALSE;
  }

  if (still_pending)
  {
    gsize size;
    GumPageProtection prot;

    if (gum_memory_get_protection (address, len, &size, &prot) &&
        (prot & GUM_PAGE_READ) != 0)
    {
      result_len = MIN (len, size);
      result = g_memdup (address, result_len);
    }
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
  static gboolean kernel_feature_likely_enabled = TRUE;
  gboolean still_pending = TRUE;

  if (kernel_feature_likely_enabled && gum_linux_check_kernel_version (3, 2, 0))
  {
    gssize n;
    struct iovec local = {
      .iov_base = (void *) bytes,
      .iov_len = len
    };
    struct iovec remote = {
      .iov_base = address,
      .iov_len = len
    };

    n = gum_libc_process_vm_writev (getpid (), &local, 1, &remote, 1, 0);
    if (n > 0)
      success = n == len;

    if (n == -1 && errno == ENOSYS)
      kernel_feature_likely_enabled = FALSE;
    else
      still_pending = FALSE;
  }

  if (still_pending)
  {
    if (gum_memory_is_writable (address, len))
    {
      memcpy (address, bytes, len);
      success = TRUE;
    }
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
#if defined (HAVE_ANDROID) && defined (HAVE_ARM)
  cacheflush (GPOINTER_TO_SIZE (address), GPOINTER_TO_SIZE (address + size), 0);
#elif defined (HAVE_ARM) || defined (HAVE_ARM64) || defined (HAVE_MIPS)
# if defined (HAVE_CLEAR_CACHE)
  __builtin___clear_cache (address, address + size);
# elif defined (HAVE_ARM) && !defined (__ARM_EABI__)
  register gpointer r0 asm ("r0") = address;
  register gpointer r1 asm ("r1") = address + size;
  register      int r2 asm ("r2") = 0;

  asm volatile (
      "swi %[syscall]\n\t"
      : "+r" (r0)
      : "r" (r1),
        "r" (r2),
        [syscall] "i" (__ARM_NR_cacheflush)
      : "memory"
  );
# else
#  error Please implement for your architecture
# endif
#endif

  VALGRIND_DISCARD_TRANSLATIONS (address, size);
}

static gboolean
gum_memory_get_protection (gconstpointer address,
                           gsize n,
                           gsize * size,
                           GumPageProtection * prot)
{
  gboolean success;
  GumProcMapsIter iter;
  const gchar * line;

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

  success = FALSE;
  *size = 0;
  *prot = GUM_PAGE_NO_ACCESS;

  if (gum_memory_get_protection_using_procmap_query (address, &success, size,
        prot))
  {
    return success;
  }

  gum_proc_maps_iter_init_for_self (&iter);

  while (gum_proc_maps_iter_next (&iter, &line))
  {
    gpointer start, end;
    gchar protection[4 + 1];

    sscanf (line, "%p-%p %s ", &start, &end, protection);

    if (start > address)
      break;
    else if (address >= start && address + n - 1 < end)
    {
      success = TRUE;
      *size = 1;
      if (protection[0] == 'r')
        *prot |= GUM_PAGE_READ;
      if (protection[1] == 'w')
        *prot |= GUM_PAGE_WRITE;
      if (protection[2] == 'x')
        *prot |= GUM_PAGE_EXECUTE;
      break;
    }
  }

  gum_proc_maps_iter_destroy (&iter);

  return success;
}

static gboolean
gum_memory_get_protection_using_procmap_query (gconstpointer address,
                                               gboolean * success,
                                               gsize * size,
                                               GumPageProtection * prot)
{
  gint fd;
  GumProcmapQuery query = { 0, };
  gboolean queried;

  fd = gum_procmap_query_open ();
  if (fd == -1)
    return FALSE;

  queried = gum_query_vma_using_procmap_query (fd, GPOINTER_TO_SIZE (address),
      &query);

  close (fd);

  if (!queried)
    return FALSE;

  if (query.vma_start <= GPOINTER_TO_SIZE (address) &&
      GPOINTER_TO_SIZE (address) < query.vma_end)
  {
    *success = TRUE;
    *size = 1;
    *prot = gum_page_protection_from_procmap_query_flags (query.vma_flags);
  }

  return TRUE;
}

static gssize
gum_libc_process_vm_readv (pid_t pid,
                           const struct iovec * local,
                           gulong num_local,
                           const struct iovec * remote,
                           gulong num_remote,
                           gulong flags)
{
  return syscall (GUM_SYS_PROCESS_VM_READV, pid, local, num_local, remote,
      num_remote, flags);
}

static gssize
gum_libc_process_vm_writev (pid_t pid,
                            const struct iovec * local,
                            gulong num_local,
                            const struct iovec * remote,
                            gulong num_remote,
                            gulong flags)
{
  return syscall (GUM_SYS_PROCESS_VM_WRITEV, pid, local, num_local, remote,
      num_remote, flags);
}
