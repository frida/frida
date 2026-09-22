/*
 * Copyright (C) 2017-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2024 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

/**
 * GumCloak:
 *
 * Keeps you from seeing yourself during process introspection.
 *
 * Introspection APIs such as [func@Gum.process_enumerate_threads] ensure that
 * cloaked resources are skipped, and things appear as if you were not inside
 * the process being instrumented.
 *
 * If you use [func@Gum.init_embedded] to initialize Gum, any resources created
 * by libffi and GLib will be cloaked automatically. (Assuming that Gum was
 * built with Frida's versions of these two libraries.)
 *
 * This means you typically only need to manage cloaked resources if you use a
 * non-GLib API to create a given resource.
 *
 * Gum's memory allocation APIs, such as [func@Gum.malloc], are automatically
 * cloaked regardless of how Gum was initialized. These use an internal heap
 * implementation that is cloak-aware. The same implementation is also used by
 * GLib when Gum is initialized as described above.
 *
 * ## Using `GumCloak`
 *
 * ```c
 * // If the current thread wasn't created by GLib, do the following two steps:
 *
 * // (1): Ignore the thread ID
 * gum_cloak_add_thread (gum_process_get_current_thread_id ());
 *
 * // (2): Ignore the thread's memory ranges (stack space)
 * GumMemoryRange ranges[2];
 * guint n = gum_thread_try_get_ranges (&ranges, G_N_ELEMENTS (ranges));
 * for (guint i = 0; i != n; i++)
 *   gum_cloak_add_range (&ranges[i]);
 *
 * // If you create a file-descriptor with a non-GLib API, also do:
 * gum_cloak_add_file_descriptor (logfile_fd);
 * ```
 */

#include "gumcloak.h"

#include "gumlibc.h"
#include "gummetalarray.h"
#include "gumspinlock.h"

#include <stdlib.h>
#include <string.h>

typedef struct _GumCloakedRange GumCloakedRange;

typedef enum
{
  GUM_KERNEL_CLOAK_ADD_THREAD = 1,
  GUM_KERNEL_CLOAK_REMOVE_THREAD,
  GUM_KERNEL_CLOAK_ADD_RANGE,
  GUM_KERNEL_CLOAK_REMOVE_RANGE,
  GUM_KERNEL_CLOAK_ADD_FD,
  GUM_KERNEL_CLOAK_REMOVE_FD,
  GUM_KERNEL_CLOAK_PING = 32,
} GumKernelCloakOp;

struct _GumCloak
{
  guint8 dummy;
};

struct _GumCloakedRange
{
  const guint8 * start;
  const guint8 * end;
};

static gint gum_cloak_index_of_thread (GumThreadId id);
static gint gum_thread_id_compare (gconstpointer element_a,
    gconstpointer element_b);
static gint gum_cloak_index_of_fd (gint fd);
static gint gum_fd_compare (gconstpointer element_a, gconstpointer element_b);

static void gum_cloak_add_range_unlocked (const GumMemoryRange * range);
static void gum_cloak_remove_range_unlocked (const GumMemoryRange * range);

static void gum_kernel_cloak_init (void);
static void gum_kernel_cloak (GumKernelCloakOp op, gsize arg1, gsize arg2);

static GumSpinlock cloak_lock = GUM_SPINLOCK_INIT;
static GumMetalArray cloaked_threads;
static GumMetalArray cloaked_ranges;
static GumMetalArray cloaked_fds;

void
_gum_cloak_init (void)
{
  gum_metal_array_init (&cloaked_threads, sizeof (GumThreadId));
  gum_metal_array_init (&cloaked_ranges, sizeof (GumCloakedRange));
  gum_metal_array_init (&cloaked_fds, sizeof (gint));

  gum_kernel_cloak_init ();
}

void
_gum_cloak_deinit (void)
{
  gum_metal_array_free (&cloaked_fds);
  gum_metal_array_free (&cloaked_ranges);
  gum_metal_array_free (&cloaked_threads);
}

/**
 * gum_cloak_add_thread:
 * @id: the thread ID to cloak
 *
 * Updates the registry of cloaked resources so the given thread `id` becomes
 * invisible to cloak-aware APIs, such as [func@Gum.process_enumerate_threads].
 */
void
gum_cloak_add_thread (GumThreadId id)
{
  GumThreadId * element, * elements;
  gint i;

  gum_spinlock_acquire (&cloak_lock);

  element = NULL;

  elements = cloaked_threads.data;
  for (i = (gint) cloaked_threads.length - 1; i >= 0; i--)
  {
    if (id >= elements[i])
    {
      element = gum_metal_array_insert_at (&cloaked_threads, i + 1);
      break;
    }
  }

  if (element == NULL)
    element = gum_metal_array_insert_at (&cloaked_threads, 0);

  *element = id;

  gum_spinlock_release (&cloak_lock);

  gum_kernel_cloak (GUM_KERNEL_CLOAK_ADD_THREAD, id, 0);
}

/**
 * gum_cloak_remove_thread:
 * @id: the thread ID to uncloak
 *
 * Updates the registry of cloaked resources so the given thread `id` becomes
 * visible to cloak-aware APIs, such as [func@Gum.process_enumerate_threads].
 */
void
gum_cloak_remove_thread (GumThreadId id)
{
  gint index_;

  gum_spinlock_acquire (&cloak_lock);

  index_ = gum_cloak_index_of_thread (id);
  if (index_ != -1)
    gum_metal_array_remove_at (&cloaked_threads, index_);

  gum_spinlock_release (&cloak_lock);

  gum_kernel_cloak (GUM_KERNEL_CLOAK_REMOVE_THREAD, id, 0);
}

/**
 * gum_cloak_has_thread:
 * @id: the thread ID to check
 *
 * Checks whether the given thread `id` is currently being cloaked.
 *
 * Used internally by e.g. [func@Gum.process_enumerate_threads] to determine
 * whether a thread should be visible.
 *
 * May also be used by you to check if a thread is among your own, e.g.:
 *
 * ```c
 * if (gum_cloak_has_thread (gum_process_get_current_thread_id ()))
 *   return;
 * ```
 *
 * Returns: true if cloaked; false otherwise
 */
gboolean
gum_cloak_has_thread (GumThreadId id)
{
  gboolean result;

  gum_spinlock_acquire (&cloak_lock);

  result = gum_cloak_index_of_thread (id) != -1;

  gum_spinlock_release (&cloak_lock);

  return result;
}

/**
 * gum_cloak_enumerate_threads:
 * @func: (not nullable) (scope call): function called with each thread ID
 * @user_data: (nullable): data to pass to `func`
 *
 * Enumerates all currently cloaked thread IDs, calling `func` with each.
 *
 * The passed in function must take special care to avoid using APIs that result
 * in cloak APIs getting called. Exactly what this means is described in further
 * detail in the toplevel [struct@Gum.Cloak] documentation.
 */
void
gum_cloak_enumerate_threads (GumCloakFoundThreadFunc func,
                             gpointer user_data)
{
  guint length, size, i;
  GumThreadId * threads;

  gum_spinlock_acquire (&cloak_lock);

  length = cloaked_threads.length;
  size = length * cloaked_threads.element_size;
  threads = g_alloca (size);
  gum_memcpy (threads, cloaked_threads.data, size);

  gum_spinlock_release (&cloak_lock);

  for (i = 0; i != length; i++)
  {
    if (!func (threads[i], user_data))
      return;
  }
}

static gint
gum_cloak_index_of_thread (GumThreadId id)
{
  GumThreadId * elements, * element;

  elements = cloaked_threads.data;

  element = bsearch (&id, elements, cloaked_threads.length,
      cloaked_threads.element_size, gum_thread_id_compare);
  if (element == NULL)
    return -1;

  return element - elements;
}

static gint
gum_thread_id_compare (gconstpointer element_a,
                       gconstpointer element_b)
{
  GumThreadId a = *((GumThreadId *) element_a);
  GumThreadId b = *((GumThreadId *) element_b);

  if (a == b)
    return 0;
  if (a < b)
    return -1;
  return 1;
}

/**
 * gum_cloak_add_range:
 * @range: the range to cloak
 *
 * Updates the registry of cloaked resources so the given memory `range` becomes
 * invisible to cloak-aware APIs, such as [func@Gum.process_enumerate_ranges].
 */
void
gum_cloak_add_range (const GumMemoryRange * range)
{
  gum_spinlock_acquire (&cloak_lock);

  gum_cloak_add_range_unlocked (range);

  gum_spinlock_release (&cloak_lock);

  gum_kernel_cloak (GUM_KERNEL_CLOAK_ADD_RANGE, range->base_address,
      range->size);
}

/**
 * gum_cloak_remove_range:
 * @range: the range to uncloak
 *
 * Updates the registry of cloaked resources so the given memory `range` becomes
 * visible to cloak-aware APIs, such as [func@Gum.process_enumerate_ranges].
 */
void
gum_cloak_remove_range (const GumMemoryRange * range)
{
  gum_spinlock_acquire (&cloak_lock);

  gum_cloak_remove_range_unlocked (range);

  gum_spinlock_release (&cloak_lock);

  gum_kernel_cloak (GUM_KERNEL_CLOAK_REMOVE_RANGE, range->base_address,
      range->size);
}

static void
gum_cloak_add_range_unlocked (const GumMemoryRange * range)
{
  const guint8 * start, * end;
  gboolean added_to_existing;
  guint i;

  start = GSIZE_TO_POINTER (range->base_address);
  end = start + range->size;

  added_to_existing = FALSE;

  for (i = 0; i != cloaked_ranges.length && !added_to_existing; i++)
  {
    GumCloakedRange * cloaked;

    cloaked = gum_metal_array_element_at (&cloaked_ranges, i);

    if (cloaked->start == end)
    {
      cloaked->start = start;
      added_to_existing = TRUE;
    }
    else if (cloaked->end == start)
    {
      cloaked->end = end;
      added_to_existing = TRUE;
    }
  }

  if (!added_to_existing)
  {
    GumCloakedRange * r;

    r = gum_metal_array_append (&cloaked_ranges);
    r->start = start;
    r->end = end;
  }
}

static void
gum_cloak_remove_range_unlocked (const GumMemoryRange * range)
{
  const guint8 * start, * end;
  gboolean found_match;

  start = GSIZE_TO_POINTER (range->base_address);
  end = start + range->size;

  do
  {
    guint i;

    found_match = FALSE;

    for (i = 0; i != cloaked_ranges.length && !found_match; i++)
    {
      GumCloakedRange * cloaked;
      gsize bottom_remainder, top_remainder;
      gboolean slot_available;

      cloaked = gum_metal_array_element_at (&cloaked_ranges, i);

      if (cloaked->start >= end || start >= cloaked->end)
        continue;

      bottom_remainder = MAX (cloaked->start, start) - cloaked->start;
      top_remainder = cloaked->end - MIN (cloaked->end, end);

      found_match = TRUE;
      slot_available = TRUE;

      if (bottom_remainder + top_remainder == 0)
      {
        gum_metal_array_remove_at (&cloaked_ranges, i);
      }
      else
      {
        const guint8 * previous_top_end = cloaked->end;

        if (bottom_remainder != 0)
        {
          cloaked->end = cloaked->start + bottom_remainder;
          slot_available = FALSE;
        }

        if (top_remainder != 0)
        {
          GumMemoryRange top;

          top.base_address = GUM_ADDRESS (previous_top_end - top_remainder);
          top.size = top_remainder;

          if (slot_available)
          {
            cloaked->start = GSIZE_TO_POINTER (top.base_address);
            cloaked->end = cloaked->start + top.size;
          }
          else
          {
            gum_cloak_add_range_unlocked (&top);
          }
        }
      }
    }
  }
  while (found_match);
}

/**
 * gum_cloak_has_range_containing:
 * @address: the address to look for
 *
 * Determines whether a memory range containing `address` is currently cloaked.
 *
 * Returns: true if cloaked; false otherwise
 */
gboolean
gum_cloak_has_range_containing (GumAddress address)
{
  gboolean is_cloaked = FALSE;
  guint i;

  gum_spinlock_acquire (&cloak_lock);

  for (i = 0; i != cloaked_ranges.length; i++)
  {
    GumCloakedRange * cr = gum_metal_array_element_at (&cloaked_ranges, i);

    if (address >= GUM_ADDRESS (cr->start) && address < GUM_ADDRESS (cr->end))
    {
      is_cloaked = TRUE;
      break;
    }
  }

  gum_spinlock_release (&cloak_lock);

  return is_cloaked;
}

/**
 * gum_cloak_clip_range:
 * @range: the range to determine the visible parts of
 *
 * Determines how much of the given memory `range` is currently visible.
 * May return an empty array if the entire range is cloaked, or NULL if it is
 * entirely visible.
 *
 * Returns: (transfer full) (element-type Gum.MemoryRange): NULL if all
 * visible, or visible parts.
 */
GArray *
gum_cloak_clip_range (const GumMemoryRange * range)
{
  GArray * chunks;
  gboolean found_match, dirty;

  chunks = g_array_sized_new (FALSE, FALSE, sizeof (GumMemoryRange), 2);
  g_array_append_val (chunks, *range);

  dirty = FALSE;

  do
  {
    guint chunk_index;

    found_match = FALSE;

    gum_spinlock_acquire (&cloak_lock);

    for (chunk_index = 0;
        chunk_index != chunks->len && !found_match;
        chunk_index++)
    {
      GumMemoryRange * chunk;
      const guint8 * chunk_start, * chunk_end;
      guint cloaked_index;
      GumCloakedRange threads;
      GumCloakedRange ranges;

      chunk = &g_array_index (chunks, GumMemoryRange, chunk_index);
      chunk_start = GSIZE_TO_POINTER (chunk->base_address);
      chunk_end = chunk_start + chunk->size;

      gum_metal_array_get_extents (&cloaked_threads,
          (gpointer *) &threads.start, (gpointer *) &threads.end);
      gum_metal_array_get_extents (&cloaked_ranges,
          (gpointer *) &ranges.start, (gpointer *) &ranges.end);

      for (cloaked_index = 0;
          cloaked_index != 2 + cloaked_ranges.length && !found_match;
          cloaked_index++)
      {
        const GumCloakedRange * cloaked;
        const guint8 * lower_bound, * upper_bound;
        gsize bottom_remainder, top_remainder;
        gboolean chunk_available;

        if (cloaked_index == 0)
        {
          cloaked = &threads;
        }
        else if (cloaked_index == 1)
        {
          cloaked = &ranges;
        }
        else
        {
          cloaked = gum_metal_array_element_at (&cloaked_ranges,
              cloaked_index - 2);
        }

        lower_bound = MAX (cloaked->start, chunk_start);
        upper_bound = MIN (cloaked->end, chunk_end);
        if (lower_bound >= upper_bound)
          continue;

        bottom_remainder = lower_bound - chunk_start;
        top_remainder = chunk_end - upper_bound;

        found_match = TRUE;
        dirty = TRUE;
        chunk_available = TRUE;

        if (bottom_remainder + top_remainder == 0)
        {
          g_array_remove_index (chunks, chunk_index);
        }
        else
        {
          if (bottom_remainder != 0)
          {
            chunk->base_address = GUM_ADDRESS (chunk_start);
            chunk->size = bottom_remainder;
            chunk_available = FALSE;
          }

          if (top_remainder != 0)
          {
            GumMemoryRange top;

            top.base_address = GUM_ADDRESS (chunk_end - top_remainder);
            top.size = top_remainder;

            if (chunk_available)
            {
              chunk->base_address = top.base_address;
              chunk->size = top.size;
            }
            else
            {
              gum_spinlock_release (&cloak_lock);
              g_array_insert_val (chunks, chunk_index + 1, top);
              gum_spinlock_acquire (&cloak_lock);
            }
          }
        }
      }
    }

    gum_spinlock_release (&cloak_lock);
  }
  while (found_match);

  if (!dirty)
  {
    g_array_free (chunks, TRUE);
    return NULL;
  }

  return chunks;
}

/**
 * gum_cloak_enumerate_ranges:
 * @func: (not nullable) (scope call): function called with each memory range
 * @user_data: (nullable): data to pass to `func`
 *
 * Enumerates all currently cloaked memory ranges, calling `func` with each.
 *
 * The passed in function must take special care to avoid using APIs that result
 * in cloak APIs getting called. Exactly what this means is described in further
 * detail in the toplevel [struct@Gum.Cloak] documentation.
 */
void
gum_cloak_enumerate_ranges (GumCloakFoundRangeFunc func,
                            gpointer user_data)
{
  guint length, size, i;
  GumCloakedRange * ranges;

  gum_spinlock_acquire (&cloak_lock);

  length = cloaked_ranges.length;
  size = length * cloaked_ranges.element_size;
  ranges = g_alloca (size);
  gum_memcpy (ranges, cloaked_ranges.data, size);

  gum_spinlock_release (&cloak_lock);

  for (i = 0; i != length; i++)
  {
    GumCloakedRange * cr = &ranges[i];
    GumMemoryRange mr;

    mr.base_address = GPOINTER_TO_SIZE (cr->start);
    mr.size = cr->end - cr->start;

    if (!func (&mr, user_data))
      return;
  }
}

/**
 * gum_cloak_add_file_descriptor:
 * @fd: the file descriptor to cloak
 *
 * Updates the registry of cloaked resources so the given `fd` becomes invisible
 * to cloak-aware APIs.
 */
void
gum_cloak_add_file_descriptor (gint fd)
{
  gint * element, * elements;
  gint i;

  gum_spinlock_acquire (&cloak_lock);

  element = NULL;

  elements = cloaked_fds.data;
  for (i = (gint) cloaked_fds.length - 1; i >= 0; i--)
  {
    if (fd >= elements[i])
    {
      element = gum_metal_array_insert_at (&cloaked_fds, i + 1);
      break;
    }
  }

  if (element == NULL)
    element = gum_metal_array_insert_at (&cloaked_fds, 0);

  *element = fd;

  gum_spinlock_release (&cloak_lock);

  gum_kernel_cloak (GUM_KERNEL_CLOAK_ADD_FD, fd, 0);
}

/**
 * gum_cloak_remove_file_descriptor:
 * @fd: the file descriptor to uncloak
 *
 * Updates the registry of cloaked resources so the given `fd` becomes visible
 * to cloak-aware APIs.
 */
void
gum_cloak_remove_file_descriptor (gint fd)
{
  gint index_;

  gum_spinlock_acquire (&cloak_lock);

  index_ = gum_cloak_index_of_fd (fd);
  if (index_ != -1)
    gum_metal_array_remove_at (&cloaked_fds, index_);

  gum_spinlock_release (&cloak_lock);

  gum_kernel_cloak (GUM_KERNEL_CLOAK_REMOVE_FD, fd, 0);
}

/**
 * gum_cloak_has_file_descriptor:
 * @fd: the file descriptor to check
 *
 * Checks whether the given `fd` is currently being cloaked.
 *
 * Returns: true if cloaked; false otherwise
 */
gboolean
gum_cloak_has_file_descriptor (gint fd)
{
  gboolean result;

  gum_spinlock_acquire (&cloak_lock);

  result = gum_cloak_index_of_fd (fd) != -1;

  gum_spinlock_release (&cloak_lock);

  return result;
}

/**
 * gum_cloak_enumerate_file_descriptors:
 * @func: (not nullable) (scope call): function called with each file descriptor
 * @user_data: (nullable): data to pass to `func`
 *
 * Enumerates all currently cloaked file descriptors, calling `func` with each.
 *
 * The passed in function must take special care to avoid using APIs that result
 * in cloak APIs getting called. Exactly what this means is described in further
 * detail in the toplevel [struct@Gum.Cloak] documentation.
 */
void
gum_cloak_enumerate_file_descriptors (GumCloakFoundFDFunc func,
                                      gpointer user_data)
{
  guint length, size, i;
  gint * fds;

  gum_spinlock_acquire (&cloak_lock);

  length = cloaked_fds.length;
  size = length * cloaked_fds.element_size;
  fds = g_alloca (size);
  gum_memcpy (fds, cloaked_fds.data, size);

  gum_spinlock_release (&cloak_lock);

  for (i = 0; i != length; i++)
  {
    if (!func (fds[i], user_data))
      return;
  }
}

static gint
gum_cloak_index_of_fd (gint fd)
{
  gint * elements, * element;

  elements = cloaked_fds.data;

  element = bsearch (&fd, elements, cloaked_fds.length,
      cloaked_fds.element_size, gum_fd_compare);
  if (element == NULL)
    return -1;

  return element - elements;
}

static gint
gum_fd_compare (gconstpointer element_a,
                gconstpointer element_b)
{
  gint a = *((gint *) element_a);
  gint b = *((gint *) element_b);

  if (a == b)
    return 0;
  if (a < b)
    return -1;
  return 1;
}

/**
 * gum_cloak_with_lock_held:
 * @func: (scope call): function to call while holding the lock
 * @user_data: data to pass to @func
 *
 * Calls @func while holding the cloak lock.
 */
void
gum_cloak_with_lock_held (GumCloakLockedFunc func,
                          gpointer user_data)
{
  gum_spinlock_acquire (&cloak_lock);
  func (user_data);
  gum_spinlock_release (&cloak_lock);
}

gboolean
gum_cloak_is_locked (void)
{
  if (!gum_spinlock_try_acquire (&cloak_lock))
    return TRUE;

  gum_spinlock_release (&cloak_lock);
  return FALSE;
}

#ifdef HAVE_LINUX

# include <unistd.h>
# include <sys/syscall.h>

/* Must match FRIDA_CONTROL_{MAGIC,TOKEN} in the Frida kernel module. */
# define GUM_KERNEL_CLOAK_MAGIC 0x46524944
# define GUM_KERNEL_CLOAK_TOKEN 0x1d5f9e6b2c7a4038ULL

static gboolean gum_kernel_cloak_available = FALSE;

static void
gum_kernel_cloak_init (void)
{
  long reply;

  reply = syscall (SYS_prctl, GUM_KERNEL_CLOAK_MAGIC,
      GUM_KERNEL_CLOAK_PING, 0, 0, GUM_KERNEL_CLOAK_TOKEN);
  gum_kernel_cloak_available = reply == GUM_KERNEL_CLOAK_MAGIC;
}

static void
gum_kernel_cloak (GumKernelCloakOp op,
                  gsize arg1,
                  gsize arg2)
{
  if (!gum_kernel_cloak_available)
    return;

  syscall (SYS_prctl, GUM_KERNEL_CLOAK_MAGIC, op, arg1, arg2,
      GUM_KERNEL_CLOAK_TOKEN);
}

#else

static void
gum_kernel_cloak_init (void)
{
}

static void
gum_kernel_cloak (GumKernelCloakOp op,
                  gsize arg1,
                  gsize arg2)
{
}

#endif
