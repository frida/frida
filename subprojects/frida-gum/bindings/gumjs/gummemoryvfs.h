/*
 * Copyright (C) 2017-2021 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_MEMORY_VFS_H__
#define __GUM_MEMORY_VFS_H__

#include <glib.h>
#include <sqlite3.h>

G_BEGIN_DECLS

typedef struct _GumMemoryVfs GumMemoryVfs;

struct _GumMemoryVfs
{
  sqlite3_vfs vfs;

  gchar * name;
  struct sqlite3_vfs * default_vfs;
  GHashTable * entries;
  guint next_entry_id;
};

G_GNUC_INTERNAL GumMemoryVfs * gum_memory_vfs_new (void);
G_GNUC_INTERNAL void gum_memory_vfs_free (GumMemoryVfs * self);

G_GNUC_INTERNAL const gchar * gum_memory_vfs_add_file (GumMemoryVfs * self,
    gpointer contents, gsize size);
G_GNUC_INTERNAL void gum_memory_vfs_remove_file (GumMemoryVfs * self,
    const gchar * path);
G_GNUC_INTERNAL gboolean gum_memory_vfs_get_file_contents (GumMemoryVfs * self,
    const gchar * path, gpointer * contents, gsize * size);

G_GNUC_INTERNAL gchar * gum_memory_vfs_contents_to_string (
    gconstpointer contents, gsize size);
G_GNUC_INTERNAL gboolean gum_memory_vfs_contents_from_string (const gchar * str,
    gpointer * contents, gsize * size);

G_END_DECLS

#endif
