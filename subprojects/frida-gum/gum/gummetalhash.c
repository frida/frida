/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
 * MT safe
 */

#include "gummetalhash.h"

#include "gumlibc.h"
#include "gummemory-priv.h"

#define HASH_TABLE_MIN_SHIFT 3

#define UNUSED_HASH_VALUE 0
#define TOMBSTONE_HASH_VALUE 1
#define HASH_IS_UNUSED(h_) ((h_) == UNUSED_HASH_VALUE)
#define HASH_IS_TOMBSTONE(h_) ((h_) == TOMBSTONE_HASH_VALUE)
#define HASH_IS_REAL(h_) ((h_) >= 2)

/**
 * GumMetalHashTable: (skip)
 */
struct _GumMetalHashTable
{
  gint             size;
  gint             mod;
  guint            mask;
  gint             nnodes;
  gint             noccupied;

  gpointer        *keys;
  guint           *hashes;
  gpointer        *values;

  GHashFunc        hash_func;
  GEqualFunc       key_equal_func;
  gint             ref_count;
  GDestroyNotify   key_destroy_func;
  GDestroyNotify   value_destroy_func;
};

typedef struct
{
  GumMetalHashTable  *hash_table;
  gpointer     dummy1;
  gpointer     dummy2;
  int          position;
  gboolean     dummy3;
  int          version;
} RealIter;

static const gint prime_mod [] =
{
  1,
  2,
  3,
  7,
  13,
  31,
  61,
  127,
  251,
  509,
  1021,
  2039,
  4093,
  8191,
  16381,
  32749,
  65521,
  131071,
  262139,
  524287,
  1048573,
  2097143,
  4194301,
  8388593,
  16777213,
  33554393,
  67108859,
  134217689,
  268435399,
  536870909,
  1073741789,
  2147483647
};

#define gum_metal_new0(struct_type, n_structs) \
    (struct_type *) gum_internal_calloc (n_structs, sizeof (struct_type))

static void
gum_metal_hash_table_set_shift (GumMetalHashTable *hash_table, gint shift)
{
  gint i;
  guint mask = 0;

  hash_table->size = 1 << shift;
  hash_table->mod  = prime_mod [shift];

  for (i = 0; i < shift; i++)
    {
      mask <<= 1;
      mask |= 1;
    }

  hash_table->mask = mask;
}

static gint
gum_metal_hash_table_find_closest_shift (gint n)
{
  gint i;

  for (i = 0; n; i++)
    n >>= 1;

  return i;
}

static void
gum_metal_hash_table_set_shift_from_size (GumMetalHashTable *hash_table, gint size)
{
  gint shift;

  shift = gum_metal_hash_table_find_closest_shift (size);
  shift = MAX (shift, HASH_TABLE_MIN_SHIFT);

  gum_metal_hash_table_set_shift (hash_table, shift);
}

static inline guint
gum_metal_hash_table_lookup_node (GumMetalHashTable    *hash_table,
                          gconstpointer  key,
                          guint         *hash_return)
{
  guint node_index;
  guint node_hash;
  guint hash_value;
  guint first_tombstone = 0;
  gboolean have_tombstone = FALSE;
  guint step = 0;

  hash_value = hash_table->hash_func (key);
  if (G_UNLIKELY (!HASH_IS_REAL (hash_value)))
    hash_value = 2;

  *hash_return = hash_value;

  node_index = hash_value % hash_table->mod;
  node_hash = hash_table->hashes[node_index];

  while (!HASH_IS_UNUSED (node_hash))
    {
      if (node_hash == hash_value)
        {
          gpointer node_key = hash_table->keys[node_index];

          if (hash_table->key_equal_func)
            {
              if (hash_table->key_equal_func (node_key, key))
                return node_index;
            }
          else if (node_key == key)
            {
              return node_index;
            }
        }
      else if (HASH_IS_TOMBSTONE (node_hash) && !have_tombstone)
        {
          first_tombstone = node_index;
          have_tombstone = TRUE;
        }

      step++;
      node_index += step;
      node_index &= hash_table->mask;
      node_hash = hash_table->hashes[node_index];
    }

  if (have_tombstone)
    return first_tombstone;

  return node_index;
}

static void
gum_metal_hash_table_remove_node (GumMetalHashTable   *hash_table,
                          gint          i,
                          gboolean      notify)
{
  gpointer key;
  gpointer value;

  key = hash_table->keys[i];
  value = hash_table->values[i];

  hash_table->hashes[i] = TOMBSTONE_HASH_VALUE;

  hash_table->keys[i] = NULL;
  hash_table->values[i] = NULL;

  hash_table->nnodes--;

  if (notify && hash_table->key_destroy_func)
    hash_table->key_destroy_func (key);

  if (notify && hash_table->value_destroy_func)
    hash_table->value_destroy_func (value);

}

static void
gum_metal_hash_table_remove_all_nodes (GumMetalHashTable *hash_table,
                               gboolean    notify)
{
  int i;
  gpointer key;
  gpointer value;

  hash_table->nnodes = 0;
  hash_table->noccupied = 0;

  if (!notify ||
      (hash_table->key_destroy_func == NULL &&
       hash_table->value_destroy_func == NULL))
    {
      gum_memset (hash_table->hashes, 0, hash_table->size * sizeof (guint));
      gum_memset (hash_table->keys, 0, hash_table->size * sizeof (gpointer));
      gum_memset (hash_table->values, 0, hash_table->size * sizeof (gpointer));

      return;
    }

  for (i = 0; i < hash_table->size; i++)
    {
      if (HASH_IS_REAL (hash_table->hashes[i]))
        {
          key = hash_table->keys[i];
          value = hash_table->values[i];

          hash_table->hashes[i] = UNUSED_HASH_VALUE;
          hash_table->keys[i] = NULL;
          hash_table->values[i] = NULL;

          if (hash_table->key_destroy_func != NULL)
            hash_table->key_destroy_func (key);

          if (hash_table->value_destroy_func != NULL)
            hash_table->value_destroy_func (value);
        }
      else if (HASH_IS_TOMBSTONE (hash_table->hashes[i]))
        {
          hash_table->hashes[i] = UNUSED_HASH_VALUE;
        }
    }
}

static void
gum_metal_hash_table_resize (GumMetalHashTable *hash_table)
{
  gpointer *new_keys;
  gpointer *new_values;
  guint *new_hashes;
  gint old_size;
  gint i;

  old_size = hash_table->size;
  gum_metal_hash_table_set_shift_from_size (hash_table, hash_table->nnodes * 2);

  new_keys = gum_metal_new0 (gpointer, hash_table->size);
  if (hash_table->keys == hash_table->values)
    new_values = new_keys;
  else
    new_values = gum_metal_new0 (gpointer, hash_table->size);
  new_hashes = gum_metal_new0 (guint, hash_table->size);

  for (i = 0; i < old_size; i++)
    {
      guint node_hash = hash_table->hashes[i];
      guint hash_val;
      guint step = 0;

      if (!HASH_IS_REAL (node_hash))
        continue;

      hash_val = node_hash % hash_table->mod;

      while (!HASH_IS_UNUSED (new_hashes[hash_val]))
        {
          step++;
          hash_val += step;
          hash_val &= hash_table->mask;
        }

      new_hashes[hash_val] = hash_table->hashes[i];
      new_keys[hash_val] = hash_table->keys[i];
      new_values[hash_val] = hash_table->values[i];
    }

  if (hash_table->keys != hash_table->values)
    gum_internal_free (hash_table->values);

  gum_internal_free (hash_table->keys);
  gum_internal_free (hash_table->hashes);

  hash_table->keys = new_keys;
  hash_table->values = new_values;
  hash_table->hashes = new_hashes;

  hash_table->noccupied = hash_table->nnodes;
}

static inline void
gum_metal_hash_table_maybe_resize (GumMetalHashTable *hash_table)
{
  gint noccupied = hash_table->noccupied;
  gint size = hash_table->size;

  if ((size > hash_table->nnodes * 4 && size > 1 << HASH_TABLE_MIN_SHIFT) ||
      (size <= noccupied + (noccupied / 16)))
    gum_metal_hash_table_resize (hash_table);
}

GumMetalHashTable *
gum_metal_hash_table_new (GHashFunc  hash_func,
                  GEqualFunc key_equal_func)
{
  return gum_metal_hash_table_new_full (hash_func, key_equal_func, NULL, NULL);
}


GumMetalHashTable *
gum_metal_hash_table_new_full (GHashFunc      hash_func,
                       GEqualFunc     key_equal_func,
                       GDestroyNotify key_destroy_func,
                       GDestroyNotify value_destroy_func)
{
  GumMetalHashTable *hash_table;

  hash_table = gum_internal_malloc (sizeof (GumMetalHashTable));
  gum_metal_hash_table_set_shift (hash_table, HASH_TABLE_MIN_SHIFT);
  hash_table->nnodes             = 0;
  hash_table->noccupied          = 0;
  hash_table->hash_func          = hash_func ? hash_func : g_direct_hash;
  hash_table->key_equal_func     = key_equal_func;
  hash_table->ref_count          = 1;
  hash_table->key_destroy_func   = key_destroy_func;
  hash_table->value_destroy_func = value_destroy_func;
  hash_table->keys               = gum_metal_new0 (gpointer, hash_table->size);
  hash_table->values             = hash_table->keys;
  hash_table->hashes             = gum_metal_new0 (guint, hash_table->size);

  return hash_table;
}

void
gum_metal_hash_table_iter_init (GumMetalHashTableIter *iter,
                        GumMetalHashTable     *hash_table)
{
  RealIter *ri = (RealIter *) iter;

  g_return_if_fail (iter != NULL);
  g_return_if_fail (hash_table != NULL);

  ri->hash_table = hash_table;
  ri->position = -1;
}

gboolean
gum_metal_hash_table_iter_next (GumMetalHashTableIter *iter,
                        gpointer       *key,
                        gpointer       *value)
{
  RealIter *ri = (RealIter *) iter;
  gint position;

  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (ri->position < ri->hash_table->size, FALSE);

  position = ri->position;

  do
    {
      position++;
      if (position >= ri->hash_table->size)
        {
          ri->position = position;
          return FALSE;
        }
    }
  while (!HASH_IS_REAL (ri->hash_table->hashes[position]));

  if (key != NULL)
    *key = ri->hash_table->keys[position];
  if (value != NULL)
    *value = ri->hash_table->values[position];

  ri->position = position;
  return TRUE;
}

GumMetalHashTable *
gum_metal_hash_table_iter_get_hash_table (GumMetalHashTableIter *iter)
{
  g_return_val_if_fail (iter != NULL, NULL);

  return ((RealIter *) iter)->hash_table;
}

static void
iter_remove_or_steal (RealIter *ri, gboolean notify)
{
  g_return_if_fail (ri != NULL);
  g_return_if_fail (ri->position >= 0);
  g_return_if_fail (ri->position < ri->hash_table->size);

  gum_metal_hash_table_remove_node (ri->hash_table, ri->position, notify);
}

void
gum_metal_hash_table_iter_remove (GumMetalHashTableIter *iter)
{
  iter_remove_or_steal ((RealIter *) iter, TRUE);
}

static gboolean
gum_metal_hash_table_insert_node (GumMetalHashTable *hash_table,
                          guint       node_index,
                          guint       key_hash,
                          gpointer    new_key,
                          gpointer    new_value,
                          gboolean    keep_new_key,
                          gboolean    reusing_key)
{
  gboolean already_exists;
  guint old_hash;
  gpointer key_to_free = NULL;
  gpointer value_to_free = NULL;

  old_hash = hash_table->hashes[node_index];
  already_exists = HASH_IS_REAL (old_hash);

  if (already_exists)
    {
      value_to_free = hash_table->values[node_index];

      if (keep_new_key)
        {
          key_to_free = hash_table->keys[node_index];
          hash_table->keys[node_index] = new_key;
        }
      else
        key_to_free = new_key;
    }
  else
    {
      hash_table->hashes[node_index] = key_hash;
      hash_table->keys[node_index] = new_key;
    }

  if (G_UNLIKELY (hash_table->keys == hash_table->values && hash_table->keys[node_index] != new_value))
    {
      hash_table->values = gum_metal_new0 (gpointer, hash_table->size);
      gum_memcpy (hash_table->values, hash_table->keys, hash_table->size * sizeof (gpointer));
    }

  hash_table->values[node_index] = new_value;

  if (!already_exists)
    {
      hash_table->nnodes++;

      if (HASH_IS_UNUSED (old_hash))
        {
          hash_table->noccupied++;
          gum_metal_hash_table_maybe_resize (hash_table);
        }
    }

  if (already_exists)
    {
      if (hash_table->key_destroy_func && !reusing_key)
        (* hash_table->key_destroy_func) (key_to_free);
      if (hash_table->value_destroy_func)
        (* hash_table->value_destroy_func) (value_to_free);
    }

  return !already_exists;
}

void
gum_metal_hash_table_iter_replace (GumMetalHashTableIter *iter,
                           gpointer        value)
{
  RealIter *ri;
  guint node_hash;
  gpointer key;

  ri = (RealIter *) iter;

  g_return_if_fail (ri != NULL);
  g_return_if_fail (ri->position >= 0);
  g_return_if_fail (ri->position < ri->hash_table->size);

  node_hash = ri->hash_table->hashes[ri->position];
  key = ri->hash_table->keys[ri->position];

  gum_metal_hash_table_insert_node (ri->hash_table, ri->position, node_hash, key, value, TRUE, TRUE);
}

void
gum_metal_hash_table_iter_steal (GumMetalHashTableIter *iter)
{
  iter_remove_or_steal ((RealIter *) iter, FALSE);
}


GumMetalHashTable *
gum_metal_hash_table_ref (GumMetalHashTable *hash_table)
{
  g_return_val_if_fail (hash_table != NULL, NULL);

  g_atomic_int_inc (&hash_table->ref_count);

  return hash_table;
}

void
gum_metal_hash_table_unref (GumMetalHashTable *hash_table)
{
  g_return_if_fail (hash_table != NULL);

  if (g_atomic_int_dec_and_test (&hash_table->ref_count))
    {
      gum_metal_hash_table_remove_all_nodes (hash_table, TRUE);
      if (hash_table->keys != hash_table->values)
        gum_internal_free (hash_table->values);
      gum_internal_free (hash_table->keys);
      gum_internal_free (hash_table->hashes);
      gum_internal_free (hash_table);
    }
}

void
gum_metal_hash_table_destroy (GumMetalHashTable *hash_table)
{
  g_return_if_fail (hash_table != NULL);

  gum_metal_hash_table_remove_all (hash_table);
  gum_metal_hash_table_unref (hash_table);
}

gpointer
gum_metal_hash_table_lookup (GumMetalHashTable    *hash_table,
                     gconstpointer  key)
{
  guint node_index;
  guint node_hash;

  g_return_val_if_fail (hash_table != NULL, NULL);

  node_index = gum_metal_hash_table_lookup_node (hash_table, key, &node_hash);

  return HASH_IS_REAL (hash_table->hashes[node_index])
    ? hash_table->values[node_index]
    : NULL;
}

gboolean
gum_metal_hash_table_lookup_extended (GumMetalHashTable    *hash_table,
                              gconstpointer  lookup_key,
                              gpointer      *orig_key,
                              gpointer      *value)
{
  guint node_index;
  guint node_hash;

  g_return_val_if_fail (hash_table != NULL, FALSE);

  node_index = gum_metal_hash_table_lookup_node (hash_table, lookup_key, &node_hash);

  if (!HASH_IS_REAL (hash_table->hashes[node_index]))
    return FALSE;

  if (orig_key)
    *orig_key = hash_table->keys[node_index];

  if (value)
    *value = hash_table->values[node_index];

  return TRUE;
}

static gboolean
gum_metal_hash_table_insert_internal (GumMetalHashTable *hash_table,
                              gpointer    key,
                              gpointer    value,
                              gboolean    keep_new_key)
{
  guint key_hash;
  guint node_index;

  g_return_val_if_fail (hash_table != NULL, FALSE);

  node_index = gum_metal_hash_table_lookup_node (hash_table, key, &key_hash);

  return gum_metal_hash_table_insert_node (hash_table, node_index, key_hash, key, value, keep_new_key, FALSE);
}

gboolean
gum_metal_hash_table_insert (GumMetalHashTable *hash_table,
                     gpointer    key,
                     gpointer    value)
{
  return gum_metal_hash_table_insert_internal (hash_table, key, value, FALSE);
}

gboolean
gum_metal_hash_table_replace (GumMetalHashTable *hash_table,
                      gpointer    key,
                      gpointer    value)
{
  return gum_metal_hash_table_insert_internal (hash_table, key, value, TRUE);
}

gboolean
gum_metal_hash_table_add (GumMetalHashTable *hash_table,
                  gpointer    key)
{
  return gum_metal_hash_table_insert_internal (hash_table, key, key, TRUE);
}

gboolean
gum_metal_hash_table_contains (GumMetalHashTable    *hash_table,
                       gconstpointer  key)
{
  guint node_index;
  guint node_hash;

  g_return_val_if_fail (hash_table != NULL, FALSE);

  node_index = gum_metal_hash_table_lookup_node (hash_table, key, &node_hash);

  return HASH_IS_REAL (hash_table->hashes[node_index]);
}

static gboolean
gum_metal_hash_table_remove_internal (GumMetalHashTable    *hash_table,
                              gconstpointer  key,
                              gboolean       notify)
{
  guint node_index;
  guint node_hash;

  g_return_val_if_fail (hash_table != NULL, FALSE);

  node_index = gum_metal_hash_table_lookup_node (hash_table, key, &node_hash);

  if (!HASH_IS_REAL (hash_table->hashes[node_index]))
    return FALSE;

  gum_metal_hash_table_remove_node (hash_table, node_index, notify);
  gum_metal_hash_table_maybe_resize (hash_table);

  return TRUE;
}

gboolean
gum_metal_hash_table_remove (GumMetalHashTable    *hash_table,
                     gconstpointer  key)
{
  return gum_metal_hash_table_remove_internal (hash_table, key, TRUE);
}

gboolean
gum_metal_hash_table_steal (GumMetalHashTable    *hash_table,
                    gconstpointer  key)
{
  return gum_metal_hash_table_remove_internal (hash_table, key, FALSE);
}

void
gum_metal_hash_table_remove_all (GumMetalHashTable *hash_table)
{
  g_return_if_fail (hash_table != NULL);

  gum_metal_hash_table_remove_all_nodes (hash_table, TRUE);
  gum_metal_hash_table_maybe_resize (hash_table);
}

void
gum_metal_hash_table_steal_all (GumMetalHashTable *hash_table)
{
  g_return_if_fail (hash_table != NULL);

  gum_metal_hash_table_remove_all_nodes (hash_table, FALSE);
  gum_metal_hash_table_maybe_resize (hash_table);
}

static guint
gum_metal_hash_table_foreach_remove_or_steal (GumMetalHashTable *hash_table,
                                      GHRFunc     func,
                                      gpointer    user_data,
                                      gboolean    notify)
{
  guint deleted = 0;
  gint i;

  for (i = 0; i < hash_table->size; i++)
    {
      guint node_hash = hash_table->hashes[i];
      gpointer node_key = hash_table->keys[i];
      gpointer node_value = hash_table->values[i];

      if (HASH_IS_REAL (node_hash) &&
          (* func) (node_key, node_value, user_data))
        {
          gum_metal_hash_table_remove_node (hash_table, i, notify);
          deleted++;
        }
    }

  gum_metal_hash_table_maybe_resize (hash_table);

  return deleted;
}

guint
gum_metal_hash_table_foreach_remove (GumMetalHashTable *hash_table,
                             GHRFunc     func,
                             gpointer    user_data)
{
  g_return_val_if_fail (hash_table != NULL, 0);
  g_return_val_if_fail (func != NULL, 0);

  return gum_metal_hash_table_foreach_remove_or_steal (hash_table, func, user_data, TRUE);
}

guint
gum_metal_hash_table_foreach_steal (GumMetalHashTable *hash_table,
                            GHRFunc     func,
                            gpointer    user_data)
{
  g_return_val_if_fail (hash_table != NULL, 0);
  g_return_val_if_fail (func != NULL, 0);

  return gum_metal_hash_table_foreach_remove_or_steal (hash_table, func, user_data, FALSE);
}

void
gum_metal_hash_table_foreach (GumMetalHashTable *hash_table,
                      GHFunc      func,
                      gpointer    user_data)
{
  gint i;

  g_return_if_fail (hash_table != NULL);
  g_return_if_fail (func != NULL);

  for (i = 0; i < hash_table->size; i++)
    {
      guint node_hash = hash_table->hashes[i];
      gpointer node_key = hash_table->keys[i];
      gpointer node_value = hash_table->values[i];

      if (HASH_IS_REAL (node_hash))
        (* func) (node_key, node_value, user_data);
    }
}

gpointer
gum_metal_hash_table_find (GumMetalHashTable *hash_table,
                   GHRFunc     predicate,
                   gpointer    user_data)
{
  gint i;
  gboolean match;

  g_return_val_if_fail (hash_table != NULL, NULL);
  g_return_val_if_fail (predicate != NULL, NULL);

  match = FALSE;

  for (i = 0; i < hash_table->size; i++)
    {
      guint node_hash = hash_table->hashes[i];
      gpointer node_key = hash_table->keys[i];
      gpointer node_value = hash_table->values[i];

      if (HASH_IS_REAL (node_hash))
        match = predicate (node_key, node_value, user_data);

      if (match)
        return node_value;
    }

  return NULL;
}

guint
gum_metal_hash_table_size (GumMetalHashTable *hash_table)
{
  g_return_val_if_fail (hash_table != NULL, 0);

  return hash_table->nnodes;
}

