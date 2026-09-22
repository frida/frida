/*
 * Copyright (C) 2008-2018 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumallocationtracker.h"

#include <string.h>

#include "gumallocationblock.h"
#include "gumallocationgroup.h"
#include "gummemory.h"
#include "gumreturnaddress.h"
#include "gumbacktracer.h"

typedef struct _GumAllocationTrackerBlock GumAllocationTrackerBlock;

struct _GumAllocationTracker
{
  GObject parent;

  gboolean disposed;

  GMutex mutex;

  volatile gint enabled;

  GumAllocationTrackerFilterFunction filter_func;
  gpointer filter_func_user_data;

  guint block_count;
  guint block_total_size;
  GHashTable * known_blocks_ht;
  GHashTable * block_groups_ht;

  GumBacktracerInterface * backtracer_iface;
  GumBacktracer * backtracer_instance;
};

enum
{
  PROP_0,
  PROP_BACKTRACER,
};

struct _GumAllocationTrackerBlock
{
  guint size;
  GumReturnAddress return_addresses[1];
};

#define GUM_ALLOCATION_TRACKER_LOCK(o) g_mutex_lock (&(o)->mutex)
#define GUM_ALLOCATION_TRACKER_UNLOCK(o) g_mutex_unlock (&(o)->mutex)

static void gum_allocation_tracker_constructed (GObject * object);
static void gum_allocation_tracker_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gum_allocation_tracker_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gum_allocation_tracker_dispose (GObject * object);
static void gum_allocation_tracker_finalize (GObject * object);

static void gum_allocation_tracker_size_stats_add_block (
    GumAllocationTracker * self, guint size);
static void gum_allocation_tracker_size_stats_remove_block (
    GumAllocationTracker * self, guint size);

G_DEFINE_TYPE (GumAllocationTracker, gum_allocation_tracker, G_TYPE_OBJECT)

static void
gum_allocation_tracker_class_init (GumAllocationTrackerClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);
  GParamSpec * pspec;

  object_class->set_property = gum_allocation_tracker_set_property;
  object_class->get_property = gum_allocation_tracker_get_property;
  object_class->dispose = gum_allocation_tracker_dispose;
  object_class->finalize = gum_allocation_tracker_finalize;
  object_class->constructed = gum_allocation_tracker_constructed;

  pspec = g_param_spec_object ("backtracer", "Backtracer",
      "Backtracer Implementation", GUM_TYPE_BACKTRACER,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_BACKTRACER, pspec);
}

static void
gum_allocation_tracker_init (GumAllocationTracker * self)
{
  g_mutex_init (&self->mutex);
}

static void
gum_allocation_tracker_constructed (GObject * object)
{
  GumAllocationTracker * self = GUM_ALLOCATION_TRACKER (object);

  if (self->backtracer_instance != NULL)
  {
    self->known_blocks_ht = g_hash_table_new_full (NULL, NULL, NULL, g_free);
  }
  else
  {
    self->known_blocks_ht = g_hash_table_new (NULL, NULL);
  }

  self->block_groups_ht = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_allocation_group_free);
}

static void
gum_allocation_tracker_set_property (GObject * object,
                                     guint property_id,
                                     const GValue * value,
                                     GParamSpec * pspec)
{
  GumAllocationTracker * self = GUM_ALLOCATION_TRACKER (object);

  switch (property_id)
  {
    case PROP_BACKTRACER:
      if (self->backtracer_instance != NULL)
        g_object_unref (self->backtracer_instance);
      self->backtracer_instance = g_value_dup_object (value);

      if (self->backtracer_instance != NULL)
      {
        self->backtracer_iface =
            GUM_BACKTRACER_GET_IFACE (self->backtracer_instance);
      }
      else
      {
        self->backtracer_iface = NULL;
      }

      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
gum_allocation_tracker_get_property (GObject * object,
                                     guint property_id,
                                     GValue * value,
                                     GParamSpec * pspec)
{
  GumAllocationTracker * self = GUM_ALLOCATION_TRACKER (object);

  switch (property_id)
  {
    case PROP_BACKTRACER:
      g_value_set_object (value, self->backtracer_instance);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
gum_allocation_tracker_dispose (GObject * object)
{
  GumAllocationTracker * self = GUM_ALLOCATION_TRACKER (object);

  if (!self->disposed)
  {
    self->disposed = TRUE;

    g_clear_object (&self->backtracer_instance);
    self->backtracer_iface = NULL;

    g_hash_table_unref (self->known_blocks_ht);
    self->known_blocks_ht = NULL;

    g_hash_table_unref (self->block_groups_ht);
    self->block_groups_ht = NULL;
  }

  G_OBJECT_CLASS (gum_allocation_tracker_parent_class)->dispose (object);
}

static void
gum_allocation_tracker_finalize (GObject * object)
{
  GumAllocationTracker * self = GUM_ALLOCATION_TRACKER (object);

  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (gum_allocation_tracker_parent_class)->finalize (object);
}

GumAllocationTracker *
gum_allocation_tracker_new (void)
{
  return gum_allocation_tracker_new_with_backtracer (NULL);
}

GumAllocationTracker *
gum_allocation_tracker_new_with_backtracer (GumBacktracer * backtracer)
{
  return g_object_new (GUM_TYPE_ALLOCATION_TRACKER,
      "backtracer", backtracer,
      NULL);
}

void
gum_allocation_tracker_set_filter_function (
    GumAllocationTracker * self,
    GumAllocationTrackerFilterFunction filter,
    gpointer user_data)
{
  g_assert (g_atomic_int_get (&self->enabled) == FALSE);

  self->filter_func = filter;
  self->filter_func_user_data = user_data;
}

void
gum_allocation_tracker_begin (GumAllocationTracker * self)
{
  GUM_ALLOCATION_TRACKER_LOCK (self);
  self->block_count = 0;
  self->block_total_size = 0;
  g_hash_table_remove_all (self->known_blocks_ht);
  GUM_ALLOCATION_TRACKER_UNLOCK (self);

  g_atomic_int_set (&self->enabled, TRUE);
}

void
gum_allocation_tracker_end (GumAllocationTracker * self)
{
  g_atomic_int_set (&self->enabled, FALSE);

  GUM_ALLOCATION_TRACKER_LOCK (self);
  self->block_count = 0;
  self->block_total_size = 0;
  g_hash_table_remove_all (self->known_blocks_ht);
  g_hash_table_remove_all (self->block_groups_ht);
  GUM_ALLOCATION_TRACKER_UNLOCK (self);
}

guint
gum_allocation_tracker_peek_block_count (GumAllocationTracker * self)
{
  return self->block_count;
}

guint
gum_allocation_tracker_peek_block_total_size (GumAllocationTracker * self)
{
  return self->block_total_size;
}

GList *
gum_allocation_tracker_peek_block_list (GumAllocationTracker * self)
{
  GList * blocks = NULL;
  GHashTableIter iter;
  gpointer key, value;

  GUM_ALLOCATION_TRACKER_LOCK (self);
  g_hash_table_iter_init (&iter, self->known_blocks_ht);
  while (g_hash_table_iter_next (&iter, &key, &value))
  {
    if (self->backtracer_instance != NULL)
    {
      GumAllocationTrackerBlock * tb = (GumAllocationTrackerBlock *) value;
      GumAllocationBlock * block;
      guint i;

      block = gum_allocation_block_new (key, tb->size);

      for (i = 0; tb->return_addresses[i] != NULL; i++)
        block->return_addresses.items[i] = tb->return_addresses[i];
      block->return_addresses.len = i;

      blocks = g_list_prepend (blocks, block);
    }
    else
    {
      blocks = g_list_prepend (blocks,
          gum_allocation_block_new (key, GPOINTER_TO_UINT (value)));
    }
  }
  GUM_ALLOCATION_TRACKER_UNLOCK (self);

  return blocks;
}

GList *
gum_allocation_tracker_peek_block_groups (GumAllocationTracker * self)
{
  GList * groups, * cur;

  GUM_ALLOCATION_TRACKER_LOCK (self);
  groups = g_hash_table_get_values (self->block_groups_ht);
  for (cur = groups; cur != NULL; cur = cur->next)
    cur->data = gum_allocation_group_copy ((GumAllocationGroup *) cur->data);
  GUM_ALLOCATION_TRACKER_UNLOCK (self);

  return groups;
}

void
gum_allocation_tracker_on_malloc (GumAllocationTracker * self,
                                  gpointer address,
                                  guint size)
{
  gum_allocation_tracker_on_malloc_full (self, address, size, NULL);
}

void
gum_allocation_tracker_on_free (GumAllocationTracker * self,
                                gpointer address)
{
  gum_allocation_tracker_on_free_full (self, address, NULL);
}

void
gum_allocation_tracker_on_realloc (GumAllocationTracker * self,
                                   gpointer old_address,
                                   gpointer new_address,
                                   guint new_size)
{
  gum_allocation_tracker_on_realloc_full (self, old_address, new_address,
      new_size, NULL);
}

void
gum_allocation_tracker_on_malloc_full (GumAllocationTracker * self,
                                       gpointer address,
                                       guint size,
                                       const GumCpuContext * cpu_context)
{
  gpointer value;

  if (!g_atomic_int_get (&self->enabled))
    return;

  if (self->backtracer_instance != NULL)
  {
    gboolean do_backtrace = TRUE;
    GumReturnAddressArray return_addresses;
    GumAllocationTrackerBlock * block;

    if (self->filter_func != NULL)
    {
      do_backtrace = self->filter_func (self, address, size,
          self->filter_func_user_data);
    }

    if (do_backtrace)
    {
      self->backtracer_iface->generate (self->backtracer_instance, cpu_context,
          &return_addresses, GUM_MAX_BACKTRACE_DEPTH);
    }
    else
    {
      return_addresses.len = 0;
    }

    block = (GumAllocationTrackerBlock *)
        g_malloc (sizeof (GumAllocationTrackerBlock) +
            (return_addresses.len * sizeof (GumReturnAddress)));
    block->size = size;
    block->return_addresses[return_addresses.len] = NULL;

    if (return_addresses.len > 0)
    {
      memcpy (block->return_addresses, &return_addresses.items,
          return_addresses.len * sizeof (GumReturnAddress));
    }

    value = block;
  }
  else
  {
    value = GUINT_TO_POINTER (size);
  }

  GUM_ALLOCATION_TRACKER_LOCK (self);

  g_hash_table_insert (self->known_blocks_ht, address, value);

  gum_allocation_tracker_size_stats_add_block (self, size);

  GUM_ALLOCATION_TRACKER_UNLOCK (self);
}

void
gum_allocation_tracker_on_free_full (GumAllocationTracker * self,
                                     gpointer address,
                                     const GumCpuContext * cpu_context)
{
  gpointer value;

  if (!g_atomic_int_get (&self->enabled))
    return;

  GUM_ALLOCATION_TRACKER_LOCK (self);

  value = g_hash_table_lookup (self->known_blocks_ht, address);
  if (value != NULL)
  {
    guint size;

    if (self->backtracer_instance != NULL)
      size = ((GumAllocationTrackerBlock *) value)->size;
    else
      size = GPOINTER_TO_UINT (value);

    gum_allocation_tracker_size_stats_remove_block (self, size);

    g_hash_table_remove (self->known_blocks_ht, address);
  }

  GUM_ALLOCATION_TRACKER_UNLOCK (self);
}

void
gum_allocation_tracker_on_realloc_full (GumAllocationTracker * self,
                                        gpointer old_address,
                                        gpointer new_address,
                                        guint new_size,
                                        const GumCpuContext * cpu_context)
{
  if (!g_atomic_int_get (&self->enabled))
    return;

  if (old_address != NULL)
  {
    if (new_size != 0)
    {
      gpointer value;

      GUM_ALLOCATION_TRACKER_LOCK (self);

      value = g_hash_table_lookup (self->known_blocks_ht, old_address);
      if (value != NULL)
      {
        guint old_size;

        g_hash_table_steal (self->known_blocks_ht, old_address);

        if (self->backtracer_instance != NULL)
        {
          GumAllocationTrackerBlock * block;

          block = (GumAllocationTrackerBlock *) value;

          g_hash_table_insert (self->known_blocks_ht, new_address, block);

          old_size = block->size;
          block->size = new_size;
        }
        else
        {
          g_hash_table_insert (self->known_blocks_ht, new_address,
              GUINT_TO_POINTER (new_size));

          old_size = GPOINTER_TO_UINT (value);
        }

        gum_allocation_tracker_size_stats_remove_block (self, old_size);
        gum_allocation_tracker_size_stats_add_block (self, new_size);
      }

      GUM_ALLOCATION_TRACKER_UNLOCK (self);
    }
    else
    {
      gum_allocation_tracker_on_free_full (self, old_address, cpu_context);
    }
  }
  else
  {
    gum_allocation_tracker_on_malloc_full (self, new_address, new_size,
        cpu_context);
  }
}

static void
gum_allocation_tracker_size_stats_add_block (GumAllocationTracker * self,
                                             guint size)
{
  GumAllocationGroup * group;

  self->block_count++;
  self->block_total_size += size;

  group = g_hash_table_lookup (self->block_groups_ht, GUINT_TO_POINTER (size));

  if (group == NULL)
  {
    group = gum_allocation_group_new (size);
    g_hash_table_insert (self->block_groups_ht, GUINT_TO_POINTER (size),
        group);
  }

  group->alive_now++;
  if (group->alive_now > group->alive_peak)
    group->alive_peak = group->alive_now;
  group->total_peak++;
}

static void
gum_allocation_tracker_size_stats_remove_block (GumAllocationTracker * self,
                                                guint size)
{
  GumAllocationGroup * group;

  self->block_count--;
  self->block_total_size -= size;

  group = g_hash_table_lookup (self->block_groups_ht, GUINT_TO_POINTER (size));
  group->alive_now--;
}
