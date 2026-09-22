/*
 * Copyright (C) 2010-2021 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2025 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_CODE_ALLOCATOR_H__
#define __GUM_CODE_ALLOCATOR_H__

#include "gummemory.h"

#define GUM_TYPE_CODE_SLICE (gum_code_slice_get_type ())
#define GUM_TYPE_CODE_DEFLECTOR (gum_code_deflector_get_type ())

G_BEGIN_DECLS

typedef struct _GumCodeAllocator GumCodeAllocator;
typedef struct _GumCodeSlice GumCodeSlice;
typedef struct _GumCodeDeflector GumCodeDeflector;

struct _GumCodeAllocator
{
  gsize slice_size;
  gsize pages_per_batch;
  gsize slices_per_batch;
  gsize pages_metadata_size;

  GSList * uncommitted_pages;
  GHashTable * dirty_pages;
  GList * free_slices;

  GSList * dispatchers;
};

struct _GumCodeSlice
{
  gpointer data;
  gpointer pc;
  guint size;

  /*< private >*/
  gint ref_count;
};

struct _GumCodeDeflector
{
  gpointer return_address;
  gpointer target;
  gpointer trampoline;

  /*< private >*/
  gint ref_count;
};

GUM_API void gum_code_allocator_init (GumCodeAllocator * allocator,
    gsize slice_size);
GUM_API void gum_code_allocator_free (GumCodeAllocator * allocator);

GUM_API GumCodeSlice * gum_code_allocator_alloc_slice (GumCodeAllocator * self);
GUM_API GumCodeSlice * gum_code_allocator_try_alloc_slice_near (
    GumCodeAllocator * self, const GumAddressSpec * spec, gsize alignment);
GUM_API void gum_code_allocator_commit (GumCodeAllocator * self);
GUM_API GType gum_code_slice_get_type (void) G_GNUC_CONST;
GUM_API GumCodeSlice * gum_code_slice_ref (GumCodeSlice * slice);
GUM_API void gum_code_slice_unref (GumCodeSlice * slice);

GUM_API GumCodeDeflector * gum_code_allocator_alloc_deflector (
    GumCodeAllocator * self, const GumAddressSpec * caller,
    gpointer return_address, gpointer target, gboolean dedicated);
GUM_API GType gum_code_deflector_get_type (void) G_GNUC_CONST;
GUM_API GumCodeDeflector * gum_code_deflector_ref (
    GumCodeDeflector * deflector);
GUM_API void gum_code_deflector_unref (GumCodeDeflector * deflector);

G_END_DECLS

#endif
