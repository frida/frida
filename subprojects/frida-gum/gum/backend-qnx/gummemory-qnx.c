/*
 * Copyright (C) 2015-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2025 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummemory.h"

#include "gummemory-priv.h"
#include "gumqnx-priv.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <sys/procfs.h>

static gboolean gum_memory_get_protection (gconstpointer address, gsize n,
    gsize * size, GumPageProtection * prot);

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
  int fd = -1;
  guint8 * buffer = NULL;
  gint num_read = 0;
  gint res G_GNUC_UNUSED;

  fd = open ("/proc/self/as", O_RDONLY);
  g_assert (fd != -1);
  res = lseek (fd, GPOINTER_TO_SIZE (address), SEEK_SET);
  g_assert (GINT_TO_POINTER (res) == address);

  buffer = g_malloc (len);
  num_read = read (fd, buffer, len);
  if (num_read == -1)
  {
    g_free (buffer);
    buffer = NULL;
  }
  if (n_bytes_read != NULL)
    *n_bytes_read = num_read;

  close (fd);

  return buffer;
}

gboolean
gum_memory_write (gpointer address,
                  const guint8 * bytes,
                  gsize len)
{
  gboolean success = FALSE;
  int fd = -1;
  gint res G_GNUC_UNUSED;
  gint num_written = 0;

  if (!gum_memory_is_writable (address, len))
    return success;

  fd = open ("/proc/self/as", O_RDWR);
  g_assert (fd != -1);
  res = lseek (fd, GPOINTER_TO_SIZE (address), SEEK_SET);
  g_assert (GINT_TO_POINTER (res) == address);

  num_written = write (fd, bytes, len);
  if (num_written == len)
    success = TRUE;

  close (fd);

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
  if (result == -1 && errno == EACCES &&
      (prot & GUM_PAGE_WRITE) == GUM_PAGE_WRITE)
  {
    int fd = -1;
    char * buffer;
    gpointer address_mmapped G_GNUC_UNUSED;
    gint total_read_count = 0;

    fd = open ("/proc/self/as", O_RDONLY);
    g_assert (fd != -1);

    buffer = g_alloca (aligned_size);
    g_assert (buffer != NULL);

    lseek (fd, GPOINTER_TO_SIZE (aligned_address), SEEK_SET);

    while (total_read_count < aligned_size)
    {
      gint read_count = read (fd, &buffer[total_read_count],
          aligned_size - total_read_count);
      total_read_count += read_count;
    }

    ThreadCtl (_NTO_TCTL_THREADS_HOLD, 0);

    address_mmapped = mmap (aligned_address, aligned_size,
        PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_FIXED, NOFD, 0);
    g_assert (address_mmapped == aligned_address);

    memcpy (aligned_address, buffer, aligned_size);

    result = mprotect (aligned_address, aligned_size, posix_prot);

    ThreadCtl (_NTO_TCTL_THREADS_CONT, 0);

    close (fd);
  }

  return result == 0;
}

void
gum_clear_cache (gpointer address,
                 gsize size)
{
  msync (address, size, MS_INVALIDATE_ICACHE);
}

static gboolean
gum_memory_get_protection (gconstpointer address,
                           gsize n,
                           gsize * size,
                           GumPageProtection * prot)
{
  gboolean success;
  gint fd, res G_GNUC_UNUSED;
  procfs_mapinfo * mapinfos;
  gint num_mapinfos;
  gpointer start, end;
  gint i;

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

  fd = open ("/proc/self/as", O_RDONLY);
  g_assert (fd != -1);

  res = devctl (fd, DCMD_PROC_PAGEDATA, 0, 0, &num_mapinfos);
  g_assert (res == 0);

  mapinfos = g_malloc (num_mapinfos * sizeof (procfs_mapinfo));

  res = devctl (fd, DCMD_PROC_PAGEDATA, mapinfos,
      sizeof (procfs_mapinfo) * num_mapinfos, &num_mapinfos);
  g_assert (res == 0);

  for (i = 0; i != num_mapinfos; i++)
  {
    start = GSIZE_TO_POINTER (mapinfos[i].vaddr & 0xffffffff);
    end = start + mapinfos[i].size;

    if (start > address)
      break;
    else if (address >= start && address + n - 1 < end)
    {
      success = TRUE;
      *size = 1;

      *prot = _gum_page_protection_from_posix (mapinfos[i].flags);
      break;
    }
  }

  g_free (mapinfos);
  close (fd);

  return success;
}

GumPageProtection
_gum_page_protection_from_posix (const gint flags)
{
  GumPageProtection prot = GUM_PAGE_NO_ACCESS;

  if (flags & PROT_READ)
    prot |= GUM_PAGE_READ;
  if (flags & PROT_WRITE)
    prot |= GUM_PAGE_WRITE;
  if (flags & PROT_EXEC)
    prot |= GUM_PAGE_EXECUTE;

  return prot;
}
