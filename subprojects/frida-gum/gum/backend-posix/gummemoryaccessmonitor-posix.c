/*
 * Copyright (C) 2010-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2019 Álvaro Felipe Melchor <alvaro.felipe91@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummemoryaccessmonitor.h"

#include "gumexceptor.h"

#include <sys/mman.h>

typedef struct _GumPageState GumPageState;
typedef struct _GumRangeStats GumRangeStats;
typedef struct _GumLivePageDetails GumLivePageDetails;
typedef struct _GumEnumerateLivePagesContext GumEnumerateLivePagesContext;

typedef gboolean (* GumFoundLivePageFunc) (const GumLivePageDetails * details,
    gpointer user_data);

struct _GumMemoryAccessMonitor
{
  GObject parent;

  guint page_size;

  gboolean enabled;
  GumExceptor * exceptor;

  GumMemoryRange * ranges;
  guint num_ranges;
  volatile gint pages_remaining;
  gint pages_total;

  GumPageProtection access_mask;
  GArray * pages;
  gboolean auto_reset;

  GumMemoryAccessNotify notify_func;
  gpointer notify_data;
  GDestroyNotify notify_data_destroy;
};

struct _GumPageState
{
  gpointer base;
  GumPageProtection protection;
  guint range_index;
  volatile guint completed;
};

struct _GumRangeStats
{
  guint live_count;
  guint guarded_count;
};

struct _GumLivePageDetails
{
  gpointer base;
  GumPageProtection protection;
  guint range_index;
};

struct _GumEnumerateLivePagesContext
{
  GumFoundLivePageFunc func;
  gpointer user_data;

  GumMemoryAccessMonitor * monitor;
};

static void gum_memory_access_monitor_dispose (GObject * object);
static void gum_memory_access_monitor_finalize (GObject * object);

static gboolean gum_collect_range_stats (const GumLivePageDetails * details,
    gpointer user_data);
static gboolean gum_monitor_range (const GumLivePageDetails * details,
    gpointer user_data);
static gboolean gum_demonitor_range (const GumLivePageDetails * details,
    gpointer user_data);

static void gum_memory_access_monitor_enumerate_live_pages (
    GumMemoryAccessMonitor * self, GumFoundLivePageFunc func,
    gpointer user_data);
static gboolean gum_emit_live_range_if_monitored (
    const GumRangeDetails * details, gpointer user_data);

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
    g_clear_pointer (&self->notify_data, self->notify_data_destroy);

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
  monitor->ranges = g_memdup (ranges, num_ranges * sizeof (GumMemoryRange));
  monitor->num_ranges = num_ranges;
  monitor->access_mask = access_mask;
  monitor->auto_reset = auto_reset;
  monitor->pages_total = 0;

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

  stats.live_count = 0;
  stats.guarded_count = 0;
  gum_memory_access_monitor_enumerate_live_pages (self,
      gum_collect_range_stats, &stats);

  if (stats.live_count != self->pages_total)
    goto error_invalid_pages;
  else if (stats.guarded_count != 0)
    goto error_inaccessible_pages;

  self->exceptor = gum_exceptor_obtain ();
  gum_exceptor_add (self->exceptor, gum_memory_access_monitor_on_exception,
      self);

  self->pages = g_array_new (FALSE, FALSE, sizeof (GumPageState));
  gum_memory_access_monitor_enumerate_live_pages (self, gum_monitor_range,
      self);

  self->enabled = TRUE;

  return TRUE;

error_invalid_pages:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT,
        "One or more pages are unallocated");
    return FALSE;
  }
error_inaccessible_pages:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT,
        "One or more pages are already fully inaccessible");
    return FALSE;
  }
}

void
gum_memory_access_monitor_disable (GumMemoryAccessMonitor * self)
{
  if (!self->enabled)
    return;

  gum_memory_access_monitor_enumerate_live_pages (self, gum_demonitor_range,
      self);

  gum_exceptor_remove (self->exceptor, gum_memory_access_monitor_on_exception,
      self);
  g_object_unref (self->exceptor);
  self->exceptor = NULL;

  g_array_free (self->pages, TRUE);

  self->pages = NULL;
  self->enabled = FALSE;
}

static gboolean
gum_collect_range_stats (const GumLivePageDetails * details,
                         gpointer user_data)
{
  GumRangeStats * stats = user_data;

  stats->live_count++;
  if (details->protection == GUM_PAGE_NO_ACCESS)
    stats->guarded_count++;

  return TRUE;
}

static gboolean
gum_monitor_range (const GumLivePageDetails * details,
                   gpointer user_data)
{
  GumMemoryAccessMonitor * self = user_data;
  GumPageProtection old_prot, new_prot;
  GumPageState page;

  old_prot = details->protection;
  new_prot = (old_prot ^ self->access_mask) & old_prot;

  page.base = details->base;
  page.protection = old_prot;
  page.range_index = details->range_index;
  page.completed = 0;

  g_array_append_val (self->pages, page);

  gum_try_mprotect (page.base, self->page_size, new_prot);

  return TRUE;
}

static gboolean
gum_demonitor_range (const GumLivePageDetails * details,
                     gpointer user_data)
{
  GumMemoryAccessMonitor * self = user_data;
  guint i;

  for (i = 0; i != self->pages->len; i++)
  {
    const GumPageState * page = &g_array_index (self->pages, GumPageState, i);

    if (page->base == details->base)
    {
      gum_try_mprotect (page->base, self->page_size, page->protection);
      return TRUE;
    }
  }

  return TRUE;
}

static void
gum_memory_access_monitor_enumerate_live_pages (GumMemoryAccessMonitor * self,
                                                GumFoundLivePageFunc func,
                                                gpointer user_data)
{
  GumEnumerateLivePagesContext ctx;

  ctx.func = func;
  ctx.user_data = user_data;

  ctx.monitor = self;

  gum_process_enumerate_ranges (GUM_PAGE_NO_ACCESS,
      gum_emit_live_range_if_monitored, &ctx);
}

static gboolean
gum_emit_live_range_if_monitored (const GumRangeDetails * details,
                                  gpointer user_data)
{
  gboolean carry_on;
  GumEnumerateLivePagesContext * ctx = user_data;
  GumMemoryAccessMonitor * self = ctx->monitor;
  const guint page_size = self->page_size;
  const GumMemoryRange * range = details->range;
  gpointer range_start, range_end;
  guint i;

  range_start = GSIZE_TO_POINTER (range->base_address);
  range_end = range_start + range->size;

  carry_on = TRUE;

  for (i = 0; i != self->num_ranges && carry_on; i++)
  {
    const GumMemoryRange * r = &self->ranges[i];
    gpointer candidate_start, candidate_end;
    gpointer intersect_start, intersect_end;
    gpointer cur;

    candidate_start = GSIZE_TO_POINTER (r->base_address);
    candidate_end = candidate_start + r->size;

    intersect_start = MAX (range_start, candidate_start);
    intersect_end = MIN (range_end, candidate_end);
    if (intersect_end <= intersect_start)
      continue;

    for (cur = intersect_start;
        cur != intersect_end && carry_on;
        cur += page_size)
    {
      GumLivePageDetails d;

      d.base = cur;
      d.protection = details->protection;
      d.range_index = i;

      carry_on = ctx->func (&d, ctx->user_data);
    }
  }

  return carry_on;
}

static gboolean
gum_memory_access_monitor_on_exception (GumExceptionDetails * details,
                                        gpointer user_data)
{
  GumMemoryAccessMonitor * self = user_data;
  const guint page_size = self->page_size;
  GumMemoryAccessDetails d;
  guint i;

  if (details->type != GUM_EXCEPTION_ACCESS_VIOLATION)
    return FALSE;

  d.thread_id = details->thread_id;
  d.operation = details->memory.operation;
  d.from = details->address;
  d.address = details->memory.address;
  d.context = &details->context;

  for (i = 0; i != self->pages->len; i++)
  {
    GumPageState * page;
    const GumMemoryRange * r;
    guint operation_mask;
    guint operations_reported;
    guint pages_remaining;

    page = &g_array_index (self->pages, GumPageState, i);
    r = &self->ranges[page->range_index];

    if (d.address >= page->base && d.address < page->base + page_size)
    {
      GumPageProtection original_prot = page->protection;

      switch (d.operation)
      {
        case GUM_MEMOP_READ:
          if ((original_prot & GUM_PAGE_READ) == 0)
            return FALSE;
          break;
        case GUM_MEMOP_WRITE:
          if ((original_prot & GUM_PAGE_WRITE) == 0)
            return FALSE;
          break;
        case GUM_MEMOP_EXECUTE:
          if ((original_prot & GUM_PAGE_EXECUTE) == 0)
            return FALSE;
          break;
        default:
          g_assert_not_reached ();
      }

      if (self->auto_reset)
        gum_try_mprotect (page->base, page_size, page->protection);

      operation_mask = 1 << d.operation;
      operations_reported = g_atomic_int_or (&page->completed, operation_mask);
      if (operations_reported != 0 && self->auto_reset)
        return FALSE;
      if (operations_reported == 0)
        pages_remaining = g_atomic_int_add (&self->pages_remaining, -1) - 1;
      else
        pages_remaining = g_atomic_int_get (&self->pages_remaining);
      d.pages_completed = self->pages_total - pages_remaining;

      d.range_index = page->range_index;
      d.page_index =
          (d.address - GSIZE_TO_POINTER (r->base_address)) / page_size;
      d.pages_total = self->pages_total;

      self->notify_func (self, &d, self->notify_data);

      return TRUE;
    }
  }

  return FALSE;
}
