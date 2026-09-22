/*
 * Copyright (C) 2010-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2015 Eloi Vanderbeken <eloi.vanderbeken@synacktiv.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummemoryaccessmonitor.h"

#include "gumexceptor.h"
#include "gum/gumwindows.h"

#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

typedef struct _GumPageDetails GumPageDetails;
typedef struct _GumLiveRangeDetails GumLiveRangeDetails;
typedef struct _GumRangeStats GumRangeStats;

typedef gboolean (* GumFoundLiveRangeFunc) (const GumLiveRangeDetails * details,
    gpointer user_data);

struct _GumMemoryAccessMonitor
{
  GObject parent;

  guint page_size;

  gboolean enabled;
  GumExceptor * exceptor;

  GumMemoryRange * ranges;
  guint num_ranges;
  gint pages_remaining;
  gint pages_total;

  GumPageProtection access_mask;
  GumPageDetails * pages_details;
  guint num_pages;
  gboolean auto_reset;

  GumMemoryAccessNotify notify_func;
  gpointer notify_data;
  GDestroyNotify notify_data_destroy;
};

struct _GumPageDetails
{
  guint range_index;
  gpointer address;
  gboolean is_guarded;
  DWORD original_protection;
  guint completed;
};

struct _GumLiveRangeDetails
{
  const GumMemoryRange * range;
  guint range_index;
  DWORD protection;
};

struct _GumRangeStats
{
  guint live_size;
  guint guarded_size;
};

static void gum_memory_access_monitor_dispose (GObject * object);
static void gum_memory_access_monitor_finalize (GObject * object);

static gboolean gum_collect_range_stats (const GumLiveRangeDetails * details,
    gpointer user_data);
static gboolean gum_set_guard_flag (const GumLiveRangeDetails * details,
    gpointer user_data);
static gboolean gum_clear_guard_flag (const GumLiveRangeDetails * details,
    gpointer user_data);

static void gum_memory_access_monitor_enumerate_live_ranges (
    GumMemoryAccessMonitor * self, GumFoundLiveRangeFunc func,
    gpointer user_data);

static gboolean gum_memory_access_monitor_on_exception (
    GumExceptionDetails * details, gpointer user_data);

G_DEFINE_TYPE (GumMemoryAccessMonitor, gum_memory_access_monitor, G_TYPE_OBJECT)

static void
gum_memory_access_monitor_class_init (GumMemoryAccessMonitorClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_memory_access_monitor_dispose;
  object_class->finalize = gum_memory_access_monitor_finalize;
}

static void
gum_memory_access_monitor_init (GumMemoryAccessMonitor * self)
{
  self->page_size = gum_query_page_size ();
}

static void
gum_memory_access_monitor_dispose (GObject * object)
{
  GumMemoryAccessMonitor * self = GUM_MEMORY_ACCESS_MONITOR (object);

  gum_memory_access_monitor_disable (self);

  if (self->notify_data_destroy != NULL)
  {
    self->notify_data_destroy (self->notify_data);
    self->notify_data_destroy = NULL;
  }
  self->notify_data = NULL;
  self->notify_func = NULL;

  G_OBJECT_CLASS (gum_memory_access_monitor_parent_class)->dispose (object);
}

static void
gum_memory_access_monitor_finalize (GObject * object)
{
  GumMemoryAccessMonitor * self = GUM_MEMORY_ACCESS_MONITOR (object);

  g_free (self->ranges);

  G_OBJECT_CLASS (gum_memory_access_monitor_parent_class)->finalize (object);
}

GumMemoryAccessMonitor *
gum_memory_access_monitor_new (const GumMemoryRange * ranges,
                               guint num_ranges,
                               GumPageProtection access_mask,
                               gboolean auto_reset,
                               GumMemoryAccessNotify func,
                               gpointer data,
                               GDestroyNotify data_destroy)
{
  GumMemoryAccessMonitor * monitor;
  guint i;

  monitor = g_object_new (GUM_TYPE_MEMORY_ACCESS_MONITOR, NULL);

  monitor->ranges = g_memdup2 (ranges, num_ranges * sizeof (GumMemoryRange));
  monitor->num_ranges = num_ranges;
  monitor->access_mask = access_mask;
  monitor->auto_reset = auto_reset;
  for (i = 0; i != num_ranges; i++)
  {
    GumMemoryRange * r = &monitor->ranges[i];
    gsize aligned_start, aligned_end;
    guint num_pages;

    aligned_start = r->base_address & ~((gsize) monitor->page_size - 1);
    aligned_end = (r->base_address + r->size + monitor->page_size - 1) &
        ~((gsize) monitor->page_size - 1);
    r->base_address = aligned_start;
    r->size = aligned_end - aligned_start;

    num_pages = r->size / monitor->page_size;
    g_atomic_int_add (&monitor->pages_remaining, num_pages);
    monitor->pages_total += num_pages;
  }

  monitor->notify_func = func;
  monitor->notify_data = data;
  monitor->notify_data_destroy = data_destroy;

  return monitor;
}

gboolean
gum_memory_access_monitor_enable (GumMemoryAccessMonitor * self,
                                  GError ** error)
{
  GumRangeStats stats;

  if (self->enabled)
    return TRUE;

  stats.live_size = 0;
  stats.guarded_size = 0;
  gum_memory_access_monitor_enumerate_live_ranges (self,
      gum_collect_range_stats, &stats);

  if (stats.live_size != self->pages_total * self->page_size)
    goto error_invalid_pages;
  else if (stats.guarded_size != 0)
    goto error_guarded_pages;

  self->exceptor = gum_exceptor_obtain ();
  gum_exceptor_add (self->exceptor, gum_memory_access_monitor_on_exception,
      self);

  self->num_pages = 0;
  self->pages_details = NULL;
  gum_memory_access_monitor_enumerate_live_ranges (self, gum_set_guard_flag,
      self);

  self->enabled = TRUE;

  return TRUE;

error_invalid_pages:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT,
        "One or more pages are unallocated");
    return FALSE;
  }
error_guarded_pages:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT,
        "One or more pages already have the guard bit set");
    return FALSE;
  }
}

void
gum_memory_access_monitor_disable (GumMemoryAccessMonitor * self)
{
  if (!self->enabled)
    return;

  gum_memory_access_monitor_enumerate_live_ranges (self,
      gum_clear_guard_flag, self);

  gum_exceptor_remove (self->exceptor, gum_memory_access_monitor_on_exception,
      self);
  g_object_unref (self->exceptor);
  self->exceptor = NULL;

  g_free (self->pages_details);
  self->num_pages = 0;
  self->pages_details = NULL;
  self->enabled = FALSE;
}

static gboolean
gum_collect_range_stats (const GumLiveRangeDetails * details,
                         gpointer user_data)
{
  GumRangeStats * stats = (GumRangeStats *) user_data;

  stats->live_size += details->range->size;
  if ((details->protection & PAGE_GUARD) == PAGE_GUARD)
    stats->guarded_size += details->range->size;

  return TRUE;
}

static gboolean
gum_set_guard_flag (const GumLiveRangeDetails * details,
                    gpointer user_data)
{
  GumMemoryAccessMonitor * self;
  DWORD old_prot, new_prot;
  BOOL success;
  gboolean is_guarded = FALSE;
  guint num_pages;

  self = GUM_MEMORY_ACCESS_MONITOR (user_data);
  new_prot = PAGE_NOACCESS;

  if ((self->access_mask & GUM_PAGE_READ) != 0)
  {
    if (self->auto_reset)
    {
      is_guarded = TRUE;
      new_prot = details->protection | PAGE_GUARD;
    }
    else
    {
      new_prot = PAGE_NOACCESS;
    }
  }
  else
  {
    switch (details->protection & 0xFF)
    {
    case PAGE_EXECUTE:
      if ((self->access_mask & GUM_PAGE_EXECUTE) != 0)
        new_prot = PAGE_READONLY;
      else
        return TRUE;
      break;
    case PAGE_EXECUTE_READ:
      if ((self->access_mask & GUM_PAGE_EXECUTE) != 0)
        new_prot = PAGE_READONLY;
      else
        return TRUE;
      break;
    case PAGE_EXECUTE_READWRITE:
      if (self->access_mask == GUM_PAGE_WRITE)
        new_prot = PAGE_EXECUTE_READ;
      else if (self->access_mask == (GUM_PAGE_EXECUTE | GUM_PAGE_WRITE))
        new_prot = PAGE_READONLY;
      else if (self->access_mask == GUM_PAGE_EXECUTE)
        new_prot = PAGE_READWRITE;
      else
        g_assert_not_reached ();
      break;
    case PAGE_EXECUTE_WRITECOPY:
      if (self->access_mask == GUM_PAGE_WRITE)
        new_prot = PAGE_EXECUTE_READ;
      else if (self->access_mask == (GUM_PAGE_EXECUTE | GUM_PAGE_WRITE))
        new_prot = PAGE_READONLY;
      else if (self->access_mask == GUM_PAGE_EXECUTE)
        new_prot = PAGE_WRITECOPY;
      else
        g_assert_not_reached ();
      break;
    case PAGE_NOACCESS:
      return TRUE;
    case PAGE_READONLY:
      return TRUE;
    case PAGE_READWRITE:
      if ((self->access_mask & GUM_PAGE_WRITE) != 0)
        new_prot = PAGE_READONLY;
      else
        return TRUE;
      break;
    case PAGE_WRITECOPY:
      if ((self->access_mask & GUM_PAGE_WRITE) != 0)
        new_prot = PAGE_READONLY;
      else
        return TRUE;
      break;
    default:
      g_assert_not_reached ();
    }
  }

  num_pages = self->num_pages;

  self->pages_details = g_realloc (self->pages_details,
      (num_pages + 1) * sizeof (self->pages_details[0]));

  self->pages_details[num_pages].range_index = details->range_index;
  self->pages_details[num_pages].original_protection = details->protection;
  self->pages_details[num_pages].address =
      GSIZE_TO_POINTER (details->range->base_address);
  self->pages_details[num_pages].is_guarded = is_guarded;
  self->pages_details[num_pages].completed = 0;

  self->num_pages++;

  success = VirtualProtect (GSIZE_TO_POINTER (details->range->base_address),
      details->range->size, new_prot, &old_prot);
  if (!success)
    g_atomic_int_add (&self->pages_remaining, -1);

  return TRUE;
}

static gboolean
gum_clear_guard_flag (const GumLiveRangeDetails * details,
                      gpointer user_data)
{
  DWORD old_prot;
  GumMemoryAccessMonitor * self = GUM_MEMORY_ACCESS_MONITOR (user_data);
  guint i;

  for (i = 0; i != self->num_pages; i++)
  {
    const GumPageDetails * page = &self->pages_details[i];
    const GumMemoryRange * r = &self->ranges[page->range_index];

    if (GUM_MEMORY_RANGE_INCLUDES (r, details->range->base_address))
    {
      return VirtualProtect (GSIZE_TO_POINTER (details->range->base_address),
          details->range->size, page->original_protection, &old_prot);
    }
  }
  return FALSE;
}

static void
gum_memory_access_monitor_enumerate_live_ranges (GumMemoryAccessMonitor * self,
                                                 GumFoundLiveRangeFunc func,
                                                 gpointer user_data)
{
  guint i;
  gboolean carry_on = TRUE;

  for (i = 0; i != self->num_ranges && carry_on; i++)
  {
    GumMemoryRange * r = &self->ranges[i];
    gpointer cur = GSIZE_TO_POINTER (r->base_address);
    gpointer end = GSIZE_TO_POINTER (r->base_address + r->size);

    do
    {
      MEMORY_BASIC_INFORMATION mbi;
      SIZE_T size;
      GumLiveRangeDetails details;
      GumMemoryRange range;

      size = VirtualQuery (cur, &mbi, sizeof (mbi));
      if (size == 0)
        break;

      /* force the iteration one page at a time */
      size = MIN (mbi.RegionSize, self->page_size);

      details.range = &range;
      details.protection = mbi.Protect;
      details.range_index = i;

      range.base_address = GUM_ADDRESS (cur);
      range.size = MIN ((gsize) ((guint8 *) end - (guint8 *) cur),
          size - ((guint8 *) cur - (guint8 *) mbi.BaseAddress));

      carry_on = func (&details, user_data);

      cur = (guint8 *) mbi.BaseAddress + size;
    }
    while (cur < end && carry_on);
  }
}

static gboolean
gum_memory_access_monitor_on_exception (GumExceptionDetails * details,
                                        gpointer user_data)
{
  GumMemoryAccessMonitor * self;
  GumMemoryAccessDetails d;
  guint i;

  self = GUM_MEMORY_ACCESS_MONITOR (user_data);

  d.thread_id = details->thread_id;
  d.operation = details->memory.operation;
  d.from = details->address;
  d.address = details->memory.address;
  d.context = &details->context;

  for (i = 0; i != self->num_pages; i++)
  {
    GumPageDetails * page = &self->pages_details[i];
    const GumMemoryRange * r = &self->ranges[page->range_index];
    guint operation_mask;
    guint operations_reported;
    guint pages_remaining;

    if ((page->address <= d.address) &&
        ((guint8 *) page->address + self->page_size > (guint8 *) d.address))
    {
      /* make sure that we don't misinterpret access violation / page guard */
      if (page->is_guarded)
      {
        if (details->type != GUM_EXCEPTION_GUARD_PAGE)
          return FALSE;
      }
      else if (details->type == GUM_EXCEPTION_ACCESS_VIOLATION)
      {
        GumPageProtection gum_original_protection =
            gum_page_protection_from_windows (page->original_protection);
        switch (d.operation)
        {
        case GUM_MEMOP_READ:
          if ((gum_original_protection & GUM_PAGE_READ) == 0)
            return FALSE;
          break;
        case GUM_MEMOP_WRITE:
          if ((gum_original_protection & GUM_PAGE_WRITE) == 0)
            return FALSE;
          break;
        case GUM_MEMOP_EXECUTE:
          if ((gum_original_protection & GUM_PAGE_EXECUTE) == 0)
            return FALSE;
          break;
        default:
          g_assert_not_reached ();
        }
      }
      else
        return FALSE;

      /* restore the original protection if needed */
      if (self->auto_reset && !page->is_guarded)
      {
        DWORD old_prot;
        /* may be called multiple times in case of simultaneous access
         * but it should not be a problem */
        VirtualProtect (
            (guint8 *) d.address - (((guintptr) d.address) % self->page_size),
            self->page_size, page->original_protection, &old_prot);
      }

      /* if an operation was already reported, don't report it. */
      operation_mask = 1 << d.operation;
      operations_reported = g_atomic_int_or (&page->completed, operation_mask);
      if ((operations_reported != 0) && self->auto_reset)
        return FALSE;

      if (!operations_reported)
        pages_remaining = g_atomic_int_add (&self->pages_remaining, -1) - 1;
      else
        pages_remaining = g_atomic_int_get (&self->pages_remaining);
      d.pages_completed = self->pages_total - pages_remaining;

      d.range_index = page->range_index;
      d.page_index = ((guint8 *) d.address -
            (guint8 *) GSIZE_TO_POINTER (r->base_address)) /
          self->page_size;
      d.pages_total = self->pages_total;

      self->notify_func (self, &d, self->notify_data);

      return TRUE;
    }
  }

  return FALSE;
}
