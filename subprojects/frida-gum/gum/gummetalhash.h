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

#ifndef __GUM_METAL_HASH_H__
#define __GUM_METAL_HASH_H__

#include <gum/gumdefs.h>

G_BEGIN_DECLS

typedef struct _GumMetalHashTable GumMetalHashTable;
typedef struct _GumMetalHashTableIter GumMetalHashTableIter;

struct _GumMetalHashTableIter
{
  gpointer dummy1;
  gpointer dummy2;
  gpointer dummy3;
  int dummy4;
  gboolean dummy5;
  gpointer dummy6;
};

GUM_API GumMetalHashTable * gum_metal_hash_table_new (GHashFunc hash_func,
    GEqualFunc key_equal_func);
GUM_API GumMetalHashTable * gum_metal_hash_table_new_full (GHashFunc hash_func,
    GEqualFunc key_equal_func, GDestroyNotify key_destroy_func,
    GDestroyNotify value_destroy_func);
GUM_API void gum_metal_hash_table_destroy (GumMetalHashTable * hash_table);
GUM_API gboolean gum_metal_hash_table_insert (GumMetalHashTable * hash_table,
    gpointer key, gpointer value);
GUM_API gboolean gum_metal_hash_table_replace (GumMetalHashTable * hash_table,
    gpointer key, gpointer value);
GUM_API gboolean gum_metal_hash_table_add (GumMetalHashTable * hash_table,
    gpointer key);
GUM_API gboolean gum_metal_hash_table_remove (GumMetalHashTable * hash_table,
    gconstpointer key);
GUM_API void gum_metal_hash_table_remove_all (GumMetalHashTable * hash_table);
GUM_API gboolean gum_metal_hash_table_steal (GumMetalHashTable * hash_table,
    gconstpointer key);
GUM_API void gum_metal_hash_table_steal_all (GumMetalHashTable * hash_table);
GUM_API gpointer gum_metal_hash_table_lookup (GumMetalHashTable * hash_table,
    gconstpointer key);
GUM_API gboolean gum_metal_hash_table_contains (GumMetalHashTable * hash_table,
    gconstpointer key);
GUM_API gboolean gum_metal_hash_table_lookup_extended (
    GumMetalHashTable * hash_table, gconstpointer lookup_key,
    gpointer * orig_key, gpointer * value);
GUM_API void gum_metal_hash_table_foreach (GumMetalHashTable * hash_table,
    GHFunc func, gpointer user_data);
GUM_API gpointer gum_metal_hash_table_find (GumMetalHashTable * hash_table,
    GHRFunc predicate, gpointer user_data);
GUM_API guint gum_metal_hash_table_foreach_remove (
    GumMetalHashTable * hash_table, GHRFunc func, gpointer user_data);
GUM_API guint gum_metal_hash_table_foreach_steal (GumMetalHashTable * hash_table,
    GHRFunc func, gpointer user_data);
GUM_API guint gum_metal_hash_table_size (GumMetalHashTable * hash_table);

GUM_API void gum_metal_hash_table_iter_init (GumMetalHashTableIter * iter,
    GumMetalHashTable * hash_table);
GUM_API gboolean gum_metal_hash_table_iter_next (GumMetalHashTableIter * iter,
    gpointer * key, gpointer * value);
GUM_API GumMetalHashTable* gum_metal_hash_table_iter_get_hash_table (
    GumMetalHashTableIter * iter);
GUM_API void gum_metal_hash_table_iter_remove (GumMetalHashTableIter * iter);
GUM_API void gum_metal_hash_table_iter_replace (GumMetalHashTableIter * iter,
    gpointer value);
GUM_API void gum_metal_hash_table_iter_steal (GumMetalHashTableIter * iter);

GUM_API GumMetalHashTable * gum_metal_hash_table_ref (
    GumMetalHashTable * hash_table);
GUM_API void gum_metal_hash_table_unref (GumMetalHashTable * hash_table);

G_END_DECLS

#endif
