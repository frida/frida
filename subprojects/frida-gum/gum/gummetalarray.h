/*
 * Copyright (C) 2017-2019 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_METAL_ARRAY_H__
#define __GUM_METAL_ARRAY_H__

#include <gum/gumdefs.h>

typedef struct _GumMetalArray GumMetalArray;

struct _GumMetalArray
{
  gpointer data;
  guint length;
  guint capacity;

  guint element_size;
};

G_BEGIN_DECLS

GUM_API void gum_metal_array_init (GumMetalArray * array, guint element_size);
GUM_API void gum_metal_array_free (GumMetalArray * array);

GUM_API gpointer gum_metal_array_element_at (GumMetalArray * self,
    guint index_);
GUM_API gpointer gum_metal_array_insert_at (GumMetalArray * self, guint index_);
GUM_API void gum_metal_array_remove_at (GumMetalArray * self, guint index_);
GUM_API void gum_metal_array_remove_all (GumMetalArray * self);
GUM_API gpointer gum_metal_array_append (GumMetalArray * self);

GUM_API void gum_metal_array_get_extents (GumMetalArray * self,
    gpointer * start, gpointer * end);
GUM_API void gum_metal_array_ensure_capacity (GumMetalArray * self,
    guint capacity);

G_END_DECLS

#endif
