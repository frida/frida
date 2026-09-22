/*
 * Copyright (C) 2013-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummemorymap.h"

#include "gumprocess-priv.h"

typedef struct _GumUpdateMemoryRangesContext GumUpdateMemoryRangesContext;

struct _GumMemoryMap
{
  GObject parent;

  GumPageProtection protection;
  GArray * ranges;
  gsize ranges_min;
  gsize ranges_max;
};

struct _GumUpdateMemoryRangesContext
{
  GArray * ranges;
  gint prev_range_index;
};

static void gum_memory_map_finalize (GObject * object);

static gboolean gum_memory_map_add_range (const GumRangeDetails * details,
    gpointer user_data);

/**
 * GumMemoryMap:
 *
 * A snapshot of the process's mapped memory ranges matching a given page
 * protection, for fast containment checks.
 *
 * Create one for the protection you care about — for example readable and
 * executable memory — then ask whether an address range falls within mapped
 * memory with [method@Gum.MemoryMap.contains]. Refresh it with
 * [method@Gum.MemoryMap.update] after the memory layout may have changed.
 */

G_DEFINE_TYPE (GumMemoryMap, gum_memory_map, G_TYPE_OBJECT)

static void
gum_memory_map_class_init (GumMemoryMapClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gum_memory_map_finalize;
}

static void
gum_memory_map_init (GumMemoryMap * self)
{
  self->ranges = g_array_new (FALSE, FALSE, sizeof (GumMemoryRange));
}

static void
gum_memory_map_finalize (GObject * object)
{
  GumMemoryMap * self = GUM_MEMORY_MAP (object);

  g_array_free (self->ranges, TRUE);

  G_OBJECT_CLASS (gum_memory_map_parent_class)->finalize (object);
}

/**
 * gum_memory_map_new:
 * @prot: protection the included ranges must have
 *
 * Creates a memory map of the process's currently mapped ranges whose
 * protection includes @prot.
 *
 * Returns: (transfer full): a new #GumMemoryMap
 */
GumMemoryMap *
gum_memory_map_new (GumPageProtection prot)
{
  GumMemoryMap * map;

  map = g_object_new (GUM_TYPE_MEMORY_MAP, NULL);
  map->protection = prot;

  gum_memory_map_update (map);

  return map;
}

/**
 * gum_memory_map_contains:
 * @self: memory map
 * @range: the range to test
 *
 * Checks whether @range lies entirely within a single mapped range of the map.
 *
 * Returns: %TRUE if @range is fully contained
 */
gboolean
gum_memory_map_contains (GumMemoryMap * self,
                         const GumMemoryRange * range)
{
  const GumAddress start = range->base_address;
  const GumAddress end = range->base_address + range->size;
  guint i;

  if (start < self->ranges_min)
    return FALSE;
  else if (end > self->ranges_max)
    return FALSE;

  for (i = 0; i < self->ranges->len; i++)
  {
    GumMemoryRange * r = &g_array_index (self->ranges, GumMemoryRange, i);
    if (start >= r->base_address && end <= r->base_address + r->size)
      return TRUE;
  }

  return FALSE;
}

/**
 * gum_memory_map_update:
 * @self: memory map
 *
 * Refreshes the map from the process's current memory layout. Call this after
 * memory may have been mapped or unmapped, since a stale map can give wrong
 * answers.
 */
void
gum_memory_map_update (GumMemoryMap * self)
{
  GumUpdateMemoryRangesContext ctx;

  ctx.ranges = self->ranges;
  ctx.prev_range_index = -1;

  g_array_set_size (self->ranges, 0);

  _gum_process_enumerate_ranges (self->protection, gum_memory_map_add_range,
      &ctx);

  if (self->ranges->len > 0)
  {
    GumMemoryRange * first_range, * last_range;

    first_range = &g_array_index (self->ranges, GumMemoryRange, 0);
    last_range = &g_array_index (self->ranges, GumMemoryRange,
        self->ranges->len - 1);

    self->ranges_min = first_range->base_address;
    self->ranges_max = last_range->base_address + last_range->size;
  }
  else
  {
    self->ranges_min = 0;
    self->ranges_max = 0;
  }
}

static gboolean
gum_memory_map_add_range (const GumRangeDetails * details,
                          gpointer user_data)
{
  GumUpdateMemoryRangesContext * ctx =
      (GumUpdateMemoryRangesContext *) user_data;
  GArray * ranges = ctx->ranges;
  const GumMemoryRange * cur = details->range;
  GumMemoryRange * prev;

  if (ctx->prev_range_index >= 0)
    prev = &g_array_index (ranges, GumMemoryRange, ctx->prev_range_index);
  else
    prev = NULL;

  if (prev != NULL && cur->base_address == prev->base_address + prev->size)
    prev->size += cur->size;
  else
    g_array_append_val (ranges, *cur);

  ctx->prev_range_index = ranges->len - 1;

  return TRUE;
}
