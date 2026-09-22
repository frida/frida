/*
 * Copyright (C) 2010-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2025 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2026 inforcqb <fanjiawei080615@qq.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumcodeallocator.h"

#include "gumcloak.h"
#include "gumcodesegment.h"
#include "gummemory.h"
#include "gumprocess-priv.h"
#ifdef HAVE_ARM
# include "gumarmwriter.h"
# include "gumthumbwriter.h"
#endif
#ifdef HAVE_ARM64
# include "gumarm64writer.h"
#endif
#ifdef HAVE_DARWIN
# include "backend-darwin/gumdarwin-priv.h"
#endif

#include <string.h>

#define GUM_CODE_SLICE_ELEMENT_FROM_SLICE(s) \
    ((GumCodeSliceElement *) (((guint8 *) (s)) - \
        G_STRUCT_OFFSET (GumCodeSliceElement, slice)))

#if GLIB_SIZEOF_VOID_P == 8
# define GUM_CODE_DEFLECTOR_CAVE_SIZE 24
# define GUM_MAX_CODE_DEFLECTOR_THUNK_SIZE 128
#else
# define GUM_CODE_DEFLECTOR_CAVE_SIZE 8
# define GUM_MAX_CODE_DEFLECTOR_THUNK_SIZE 64
#endif

typedef struct _GumCodePages GumCodePages;
typedef struct _GumCodeSliceElement GumCodeSliceElement;
typedef struct _GumCodeDeflectorDispatcher GumCodeDeflectorDispatcher;
typedef struct _GumCodeDeflectorImpl GumCodeDeflectorImpl;
typedef struct _GumProbeRangeForCodeCaveContext GumProbeRangeForCodeCaveContext;
typedef struct _GumInsertDeflectorContext GumInsertDeflectorContext;

struct _GumCodeSliceElement
{
  GList parent;
  GumCodeSlice slice;
};

struct _GumCodePages
{
  gint ref_count;

  GumCodeSegment * segment;
  gpointer data;
  gpointer pc;
  gsize size;

  GumCodeAllocator * allocator;

  GumCodeSliceElement elements[1];
};

struct _GumCodeDeflectorDispatcher
{
  GSList * callers;

  gpointer address;

  gpointer original_data;
  gsize original_size;

  gpointer trampoline;
  gpointer thunk;
  gsize thunk_size;
};

struct _GumCodeDeflectorImpl
{
  GumCodeDeflector parent;

  GumCodeAllocator * allocator;
};

struct _GumProbeRangeForCodeCaveContext
{
  const GumAddressSpec * caller;

  GumMemoryRange cave;
};

struct _GumInsertDeflectorContext
{
  GumAddress pc;
  gsize max_size;
  gpointer return_address;
  gpointer dedicated_target;

  GumCodeDeflectorDispatcher * dispatcher;
};

static GumCodeSlice * gum_code_allocator_try_alloc_batch_near (
    GumCodeAllocator * self, const GumAddressSpec * spec);

static void gum_code_pages_unref (GumCodePages * self);

static gboolean gum_code_slice_is_near (const GumCodeSlice * self,
    const GumAddressSpec * spec);
static gboolean gum_code_slice_is_aligned (const GumCodeSlice * slice,
    gsize alignment);

static GumCodeDeflectorDispatcher * gum_code_deflector_dispatcher_new (
    const GumAddressSpec * caller, gpointer return_address,
    gpointer dedicated_target);
static void gum_code_deflector_dispatcher_free (
    GumCodeDeflectorDispatcher * dispatcher);
static void gum_insert_deflector (gpointer cave,
    GumInsertDeflectorContext * ctx);
static void gum_write_thunk (gpointer thunk,
    GumCodeDeflectorDispatcher * dispatcher);
static void gum_remove_deflector (gpointer cave,
    GumCodeDeflectorDispatcher * dispatcher);
static gpointer gum_code_deflector_dispatcher_lookup (
    GumCodeDeflectorDispatcher * self, gpointer return_address);

static gboolean gum_probe_module_for_code_cave (GumModule * module,
    gpointer user_data);

G_DEFINE_BOXED_TYPE (GumCodeSlice, gum_code_slice, gum_code_slice_ref,
                     gum_code_slice_unref)
G_DEFINE_BOXED_TYPE (GumCodeDeflector, gum_code_deflector,
                     gum_code_deflector_ref, gum_code_deflector_unref)

void
gum_code_allocator_init (GumCodeAllocator * allocator,
                         gsize slice_size)
{
  allocator->slice_size = slice_size;
  allocator->pages_per_batch = 7;
  allocator->slices_per_batch =
      (allocator->pages_per_batch * gum_query_page_size ()) / slice_size;
  allocator->pages_metadata_size = sizeof (GumCodePages) +
      ((allocator->slices_per_batch - 1) * sizeof (GumCodeSliceElement));

  allocator->uncommitted_pages = NULL;
  allocator->dirty_pages = g_hash_table_new (NULL, NULL);
  allocator->free_slices = NULL;

  allocator->dispatchers = NULL;
}

void
gum_code_allocator_free (GumCodeAllocator * allocator)
{
  g_slist_foreach (allocator->dispatchers,
      (GFunc) gum_code_deflector_dispatcher_free, NULL);
  g_slist_free (allocator->dispatchers);
  allocator->dispatchers = NULL;

  g_list_foreach (allocator->free_slices, (GFunc) gum_code_pages_unref, NULL);
  g_hash_table_unref (allocator->dirty_pages);
  g_slist_free (allocator->uncommitted_pages);
  allocator->uncommitted_pages = NULL;
  allocator->dirty_pages = NULL;
  allocator->free_slices = NULL;
}

GumCodeSlice *
gum_code_allocator_alloc_slice (GumCodeAllocator * self)
{
  return gum_code_allocator_try_alloc_slice_near (self, NULL, 0);
}

GumCodeSlice *
gum_code_allocator_try_alloc_slice_near (GumCodeAllocator * self,
                                         const GumAddressSpec * spec,
                                         gsize alignment)
{
  GList * cur;

  for (cur = self->free_slices; cur != NULL; cur = cur->next)
  {
    GumCodeSliceElement * element = (GumCodeSliceElement *) cur;
    GumCodeSlice * slice = &element->slice;

    if (gum_code_slice_is_near (slice, spec) &&
        gum_code_slice_is_aligned (slice, alignment))
    {
      GumCodePages * pages = element->parent.data;

      self->free_slices = g_list_remove_link (self->free_slices, cur);

      g_hash_table_add (self->dirty_pages, pages);

      return slice;
    }
  }

  return gum_code_allocator_try_alloc_batch_near (self, spec);
}

void
gum_code_allocator_commit (GumCodeAllocator * self)
{
  gboolean rwx_supported, remap_supported;
  GSList * cur;
  GHashTableIter iter;
  gpointer key;

  rwx_supported = gum_query_is_rwx_supported ();
  remap_supported = gum_memory_can_remap_writable ();

  for (cur = self->uncommitted_pages; cur != NULL; cur = cur->next)
  {
    GumCodePages * pages = cur->data;
    GumCodeSegment * segment = pages->segment;

    if (segment != NULL)
    {
      gum_code_segment_realize (segment);
      gum_code_segment_map (segment, 0,
          gum_code_segment_get_virtual_size (segment),
          gum_code_segment_get_address (segment));
    }
    else if (!remap_supported)
    {
      gum_mprotect (pages->data, pages->size, GUM_PAGE_RX);
    }
  }
  g_slist_free (self->uncommitted_pages);
  self->uncommitted_pages = NULL;

  g_hash_table_iter_init (&iter, self->dirty_pages);
  while (g_hash_table_iter_next (&iter, &key, NULL))
  {
    GumCodePages * pages = key;

    gum_clear_cache (pages->data, pages->size);
  }
  g_hash_table_remove_all (self->dirty_pages);

  if (!rwx_supported)
  {
    g_list_foreach (self->free_slices, (GFunc) gum_code_pages_unref, NULL);
    self->free_slices = NULL;
  }
}

static GumCodeSlice *
gum_code_allocator_try_alloc_batch_near (GumCodeAllocator * self,
                                         const GumAddressSpec * spec)
{
  GumCodeSlice * result = NULL;
  gboolean rwx_supported, code_segment_supported, remap_supported;
  gsize page_size, size_in_pages, size_in_bytes;
  GumCodeSegment * segment;
  gpointer data, pc;
  GumCodePages * pages;
  guint i;

  rwx_supported = gum_query_is_rwx_supported ();
  code_segment_supported = gum_code_segment_is_supported ();
  remap_supported = gum_memory_can_remap_writable ();

  page_size = gum_query_page_size ();
  size_in_pages = self->pages_per_batch;
  size_in_bytes = size_in_pages * page_size;

  if (rwx_supported || !code_segment_supported)
  {
    GumPageProtection protection;
    GumMemoryRange range;

    if (rwx_supported)
      protection = GUM_PAGE_RWX;
    else
      protection = remap_supported ? GUM_PAGE_RX : GUM_PAGE_RW;

    segment = NULL;
    if (spec != NULL)
      data = gum_memory_allocate_near (spec, size_in_bytes, page_size,
          protection);
    else
      data = gum_memory_allocate (NULL, size_in_bytes, page_size, protection);
    if (data == NULL)
      return NULL;

    range.base_address = GUM_ADDRESS (data);
    range.size = size_in_bytes;
    gum_cloak_add_range (&range);

    pc = data;
    if (remap_supported)
      data = gum_memory_try_remap_writable_pages (data, size_in_pages);
  }
  else
  {
    segment = gum_code_segment_new (size_in_bytes, spec);
    if (segment == NULL)
      return NULL;
    data = gum_code_segment_get_address (segment);
    pc = data;
  }

  pages = g_slice_alloc (self->pages_metadata_size);
  pages->ref_count = self->slices_per_batch;

  pages->segment = segment;
  pages->data = data;
  pages->pc = pc;
  pages->size = size_in_bytes;

  pages->allocator = self;

  for (i = self->slices_per_batch; i != 0; i--)
  {
    guint slice_index = i - 1;
    GumCodeSliceElement * element = &pages->elements[slice_index];
    GList * link;
    GumCodeSlice * slice;

    slice = &element->slice;
    slice->data = (guint8 *) data + (slice_index * self->slice_size);
    slice->pc = (guint8 *) pc + (slice_index * self->slice_size);
    slice->size = self->slice_size;
    slice->ref_count = 1;

    link = &element->parent;
    link->data = pages;
    link->prev = NULL;
    if (slice_index == 0)
    {
      link->next = NULL;
      result = slice;
    }
    else
    {
      if (self->free_slices != NULL)
        self->free_slices->prev = link;
      link->next = self->free_slices;
      self->free_slices = link;
    }
  }

  if (!rwx_supported)
    self->uncommitted_pages = g_slist_prepend (self->uncommitted_pages, pages);

  g_hash_table_add (self->dirty_pages, pages);

  return result;
}

static void
gum_code_pages_unref (GumCodePages * self)
{
  self->ref_count--;
  if (self->ref_count == 0)
  {
    if (self->segment != NULL)
    {
      gum_code_segment_free (self->segment);
    }
    else
    {
      GumMemoryRange range;

      if (self->pc != self->data)
      {
        guint size_in_pages;

        size_in_pages = self->size / gum_query_page_size ();
        gum_memory_dispose_writable_pages (self->data, size_in_pages);
      }

      gum_memory_free (self->pc, self->size);

      range.base_address = GUM_ADDRESS (self->pc);
      range.size = self->size;
      gum_cloak_remove_range (&range);
    }

    g_slice_free1 (self->allocator->pages_metadata_size, self);
  }
}

GumCodeSlice *
gum_code_slice_ref (GumCodeSlice * slice)
{
  g_atomic_int_inc (&slice->ref_count);

  return slice;
}

void
gum_code_slice_unref (GumCodeSlice * slice)
{
  GumCodeSliceElement * element;
  GumCodePages * pages;

  if (slice == NULL)
    return;

  if (!g_atomic_int_dec_and_test (&slice->ref_count))
    return;

  element = GUM_CODE_SLICE_ELEMENT_FROM_SLICE (slice);
  pages = element->parent.data;

  if (gum_query_is_rwx_supported ())
  {
    GumCodeAllocator * allocator = pages->allocator;
    GList * link = &element->parent;

    if (allocator->free_slices != NULL)
      allocator->free_slices->prev = link;
    link->next = allocator->free_slices;
    allocator->free_slices = link;
  }
  else
  {
    gum_code_pages_unref (pages);
  }
}

static gboolean
gum_code_slice_is_near (const GumCodeSlice * self,
                        const GumAddressSpec * spec)
{
  gssize near_address;
  gssize slice_start, slice_end;
  gsize distance_start, distance_end;

  if (spec == NULL)
    return TRUE;

  near_address = (gssize) spec->near_address;

  slice_start = (gssize) self->pc;
  slice_end = slice_start + self->size - 1;

  distance_start = ABS (near_address - slice_start);
  distance_end = ABS (near_address - slice_end);

  return distance_start <= spec->max_distance &&
      distance_end <= spec->max_distance;
}

static gboolean
gum_code_slice_is_aligned (const GumCodeSlice * slice,
                           gsize alignment)
{
  if (alignment == 0)
    return TRUE;

  return GPOINTER_TO_SIZE (slice->pc) % alignment == 0;
}

GumCodeDeflector *
gum_code_allocator_alloc_deflector (GumCodeAllocator * self,
                                    const GumAddressSpec * caller,
                                    gpointer return_address,
                                    gpointer target,
                                    gboolean dedicated)
{
  GumCodeDeflectorDispatcher * dispatcher = NULL;
  GSList * cur;
  GumCodeDeflectorImpl * impl;
  GumCodeDeflector * deflector;

  if (!dedicated)
  {
    for (cur = self->dispatchers; cur != NULL; cur = cur->next)
    {
      GumCodeDeflectorDispatcher * d = cur->data;
      gsize distance;

      distance = ABS ((gssize) GPOINTER_TO_SIZE (d->address) -
          (gssize) caller->near_address);
      if (distance <= caller->max_distance)
      {
        dispatcher = d;
        break;
      }
    }
  }

  if (dispatcher == NULL)
  {
    dispatcher = gum_code_deflector_dispatcher_new (caller, return_address,
        dedicated ? target : NULL);
    if (dispatcher == NULL)
      return NULL;
    self->dispatchers = g_slist_prepend (self->dispatchers, dispatcher);
  }

  impl = g_slice_new (GumCodeDeflectorImpl);

  deflector = &impl->parent;
  deflector->return_address = return_address;
  deflector->target = target;
  deflector->trampoline = dispatcher->trampoline;
  deflector->ref_count = 1;

  impl->allocator = self;

  dispatcher->callers = g_slist_prepend (dispatcher->callers, deflector);

  return deflector;
}

GumCodeDeflector *
gum_code_deflector_ref (GumCodeDeflector * deflector)
{
  g_atomic_int_inc (&deflector->ref_count);

  return deflector;
}

void
gum_code_deflector_unref (GumCodeDeflector * deflector)
{
  GumCodeDeflectorImpl * impl = (GumCodeDeflectorImpl *) deflector;
  GumCodeAllocator * allocator;
  GSList * cur;

  if (deflector == NULL)
    return;

  if (!g_atomic_int_dec_and_test (&deflector->ref_count))
    return;

  allocator = impl->allocator;

  for (cur = allocator->dispatchers; cur != NULL; cur = cur->next)
  {
    GumCodeDeflectorDispatcher * dispatcher = cur->data;
    GSList * entry;

    entry = g_slist_find (dispatcher->callers, deflector);
    if (entry != NULL)
    {
      g_slice_free (GumCodeDeflectorImpl, impl);

      dispatcher->callers = g_slist_delete_link (dispatcher->callers, entry);
      if (dispatcher->callers == NULL)
      {
        gum_code_deflector_dispatcher_free (dispatcher);
        allocator->dispatchers = g_slist_remove (allocator->dispatchers,
            dispatcher);
      }

      return;
    }
  }

  g_assert_not_reached ();
}

static GumCodeDeflectorDispatcher *
gum_code_deflector_dispatcher_new (const GumAddressSpec * caller,
                                   gpointer return_address,
                                   gpointer dedicated_target)
{
#if defined (HAVE_DARWIN) || (defined (HAVE_ELF) && GLIB_SIZEOF_VOID_P == 4)
  GumCodeDeflectorDispatcher * dispatcher;
  GumProbeRangeForCodeCaveContext probe_ctx;
  GumInsertDeflectorContext insert_ctx;
  gboolean remap_supported;

  remap_supported = gum_memory_can_remap_writable ();

  probe_ctx.caller = caller;

  probe_ctx.cave.base_address = 0;
  probe_ctx.cave.size = 0;

  gum_process_enumerate_modules (gum_probe_module_for_code_cave, &probe_ctx);

  if (probe_ctx.cave.base_address == 0)
    return NULL;

  dispatcher = g_slice_new0 (GumCodeDeflectorDispatcher);

  dispatcher->address = GSIZE_TO_POINTER (probe_ctx.cave.base_address);

  dispatcher->original_data = g_memdup (dispatcher->address,
      probe_ctx.cave.size);
  dispatcher->original_size = probe_ctx.cave.size;

  if (dedicated_target == NULL)
  {
    gsize thunk_size;
    GumMemoryRange range;
    GumPageProtection protection;

    thunk_size = gum_query_page_size ();
    protection = remap_supported ? GUM_PAGE_RX : GUM_PAGE_RW;

    dispatcher->thunk =
        gum_memory_allocate (NULL, thunk_size, thunk_size, protection);
    dispatcher->thunk_size = thunk_size;

    gum_memory_patch_code (dispatcher->thunk, GUM_MAX_CODE_DEFLECTOR_THUNK_SIZE,
        (GumMemoryPatchApplyFunc) gum_write_thunk, dispatcher);

    range.base_address = GUM_ADDRESS (dispatcher->thunk);
    range.size = thunk_size;
    gum_cloak_add_range (&range);
  }

  insert_ctx.pc = GUM_ADDRESS (dispatcher->address);
  insert_ctx.max_size = dispatcher->original_size;
  insert_ctx.return_address = return_address;
  insert_ctx.dedicated_target = dedicated_target;

  insert_ctx.dispatcher = dispatcher;

  gum_memory_patch_code (dispatcher->address, dispatcher->original_size,
      (GumMemoryPatchApplyFunc) gum_insert_deflector, &insert_ctx);

  return dispatcher;
#else
  (void) gum_insert_deflector;
  (void) gum_write_thunk;
  (void) gum_probe_module_for_code_cave;

  return NULL;
#endif
}

static void
gum_code_deflector_dispatcher_free (GumCodeDeflectorDispatcher * dispatcher)
{
  gum_memory_patch_code (dispatcher->address, dispatcher->original_size,
      (GumMemoryPatchApplyFunc) gum_remove_deflector, dispatcher);

  if (dispatcher->thunk != NULL)
  {
    GumMemoryRange range;

    gum_memory_release (dispatcher->thunk, dispatcher->thunk_size);

    range.base_address = GUM_ADDRESS (dispatcher->thunk);
    range.size = dispatcher->thunk_size;
    gum_cloak_remove_range (&range);
  }

  g_free (dispatcher->original_data);

  g_slist_foreach (dispatcher->callers, (GFunc) gum_code_deflector_unref, NULL);
  g_slist_free (dispatcher->callers);

  g_slice_free (GumCodeDeflectorDispatcher, dispatcher);
}

static void
gum_insert_deflector (gpointer cave,
                      GumInsertDeflectorContext * ctx)
{
# if defined (HAVE_ARM)
  GumCodeDeflectorDispatcher * dispatcher = ctx->dispatcher;
  GumThumbWriter tw;

  if (ctx->dedicated_target != NULL)
  {
    gboolean owner_is_arm;

    owner_is_arm = (GPOINTER_TO_SIZE (ctx->return_address) & 1) == 0;
    if (owner_is_arm)
    {
      GumArmWriter aw;

      gum_arm_writer_init (&aw, cave);
      aw.cpu_features = gum_query_cpu_features ();
      aw.pc = ctx->pc;
      gum_arm_writer_put_ldr_reg_address (&aw, ARM_REG_PC,
          GUM_ADDRESS (ctx->dedicated_target));
      gum_arm_writer_flush (&aw);
      g_assert (gum_arm_writer_offset (&aw) <= ctx->max_size);
      gum_arm_writer_clear (&aw);

      dispatcher->trampoline = GSIZE_TO_POINTER (ctx->pc);

      return;
    }

    gum_thumb_writer_init (&tw, cave);
    tw.pc = ctx->pc;
    gum_thumb_writer_put_ldr_reg_address (&tw, ARM_REG_PC,
        GUM_ADDRESS (ctx->dedicated_target));
  }
  else
  {
    gum_thumb_writer_init (&tw, cave);
    tw.pc = ctx->pc;
    gum_thumb_writer_put_ldr_reg_address (&tw, ARM_REG_PC,
        GUM_ADDRESS (dispatcher->thunk) + 1);
  }

  gum_thumb_writer_flush (&tw);
  g_assert (gum_thumb_writer_offset (&tw) <= ctx->max_size);
  gum_thumb_writer_clear (&tw);

  dispatcher->trampoline = GSIZE_TO_POINTER (ctx->pc + 1);
# elif defined (HAVE_ARM64)
  GumCodeDeflectorDispatcher * dispatcher = ctx->dispatcher;
  GumArm64Writer aw;

  gum_arm64_writer_init (&aw, cave);
  aw.pc = ctx->pc;

  if (ctx->dedicated_target != NULL)
  {
    gum_arm64_writer_put_push_reg_reg (&aw, ARM64_REG_X0, ARM64_REG_LR);
    gum_arm64_writer_put_ldr_reg_address (&aw, ARM64_REG_X0,
        GUM_ADDRESS (ctx->dedicated_target));
    gum_arm64_writer_put_jmp_reg (&aw, ARM64_REG_X0);
  }
  else
  {
    gum_arm64_writer_put_ldr_reg_address (&aw, ARM64_REG_X0,
        GUM_ADDRESS (gum_sign_code_pointer (dispatcher->thunk)));
    gum_arm64_writer_put_jmp_reg (&aw, ARM64_REG_X0);
  }

  gum_arm64_writer_flush (&aw);
  g_assert (gum_arm64_writer_offset (&aw) <= ctx->max_size);
  gum_arm64_writer_clear (&aw);

  dispatcher->trampoline = GSIZE_TO_POINTER (ctx->pc);
# else
  (void) gum_code_deflector_dispatcher_lookup;
# endif
}

static void
gum_write_thunk (gpointer thunk,
                 GumCodeDeflectorDispatcher * dispatcher)
{
# if defined (HAVE_ARM)
  GumThumbWriter tw;

  gum_thumb_writer_init (&tw, thunk);
  tw.pc = GUM_ADDRESS (dispatcher->thunk);

  gum_thumb_writer_put_push_regs (&tw, 2, ARM_REG_R9, ARM_REG_R12);

  gum_thumb_writer_put_call_address_with_arguments (&tw,
      GUM_ADDRESS (gum_code_deflector_dispatcher_lookup), 2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (dispatcher),
      GUM_ARG_REGISTER, ARM_REG_LR);

  gum_thumb_writer_put_pop_regs (&tw, 2, ARM_REG_R9, ARM_REG_R12);

  gum_thumb_writer_put_bx_reg (&tw, ARM_REG_R0);
  gum_thumb_writer_clear (&tw);
# elif defined (HAVE_ARM64)
  GumArm64Writer aw;

  gum_arm64_writer_init (&aw, thunk);
  aw.pc = GUM_ADDRESS (dispatcher->thunk);

  /* push {q0-q7} */
  gum_arm64_writer_put_instruction (&aw, 0xadbf1fe6);
  gum_arm64_writer_put_instruction (&aw, 0xadbf17e4);
  gum_arm64_writer_put_instruction (&aw, 0xadbf0fe2);
  gum_arm64_writer_put_instruction (&aw, 0xadbf07e0);

  gum_arm64_writer_put_push_reg_reg (&aw, ARM64_REG_X17, ARM64_REG_X18);
  gum_arm64_writer_put_push_reg_reg (&aw, ARM64_REG_X15, ARM64_REG_X16);
  gum_arm64_writer_put_push_reg_reg (&aw, ARM64_REG_X13, ARM64_REG_X14);
  gum_arm64_writer_put_push_reg_reg (&aw, ARM64_REG_X11, ARM64_REG_X12);
  gum_arm64_writer_put_push_reg_reg (&aw, ARM64_REG_X9, ARM64_REG_X10);
  gum_arm64_writer_put_push_reg_reg (&aw, ARM64_REG_X7, ARM64_REG_X8);
  gum_arm64_writer_put_push_reg_reg (&aw, ARM64_REG_X5, ARM64_REG_X6);
  gum_arm64_writer_put_push_reg_reg (&aw, ARM64_REG_X3, ARM64_REG_X4);
  gum_arm64_writer_put_push_reg_reg (&aw, ARM64_REG_X1, ARM64_REG_X2);

  gum_arm64_writer_put_call_address_with_arguments (&aw,
      GUM_ADDRESS (gum_code_deflector_dispatcher_lookup), 2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (dispatcher),
      GUM_ARG_REGISTER, ARM64_REG_LR);

  gum_arm64_writer_put_pop_reg_reg (&aw, ARM64_REG_X1, ARM64_REG_X2);
  gum_arm64_writer_put_pop_reg_reg (&aw, ARM64_REG_X3, ARM64_REG_X4);
  gum_arm64_writer_put_pop_reg_reg (&aw, ARM64_REG_X5, ARM64_REG_X6);
  gum_arm64_writer_put_pop_reg_reg (&aw, ARM64_REG_X7, ARM64_REG_X8);
  gum_arm64_writer_put_pop_reg_reg (&aw, ARM64_REG_X9, ARM64_REG_X10);
  gum_arm64_writer_put_pop_reg_reg (&aw, ARM64_REG_X11, ARM64_REG_X12);
  gum_arm64_writer_put_pop_reg_reg (&aw, ARM64_REG_X13, ARM64_REG_X14);
  gum_arm64_writer_put_pop_reg_reg (&aw, ARM64_REG_X15, ARM64_REG_X16);
  gum_arm64_writer_put_pop_reg_reg (&aw, ARM64_REG_X17, ARM64_REG_X18);

  /* pop {q0-q7} */
  gum_arm64_writer_put_instruction (&aw, 0xacc107e0);
  gum_arm64_writer_put_instruction (&aw, 0xacc10fe2);
  gum_arm64_writer_put_instruction (&aw, 0xacc117e4);
  gum_arm64_writer_put_instruction (&aw, 0xacc11fe6);

  gum_arm64_writer_put_jmp_reg (&aw, ARM64_REG_X0);
  gum_arm64_writer_clear (&aw);
# else
  (void) gum_code_deflector_dispatcher_lookup;
# endif
}

static void
gum_remove_deflector (gpointer cave,
                      GumCodeDeflectorDispatcher * dispatcher)
{
  memcpy (cave, dispatcher->original_data, dispatcher->original_size);
}

static gpointer
gum_code_deflector_dispatcher_lookup (GumCodeDeflectorDispatcher * self,
                                      gpointer return_address)
{
  GSList * cur;

  for (cur = self->callers; cur != NULL; cur = cur->next)
  {
    GumCodeDeflector * caller = cur->data;

    if (caller->return_address == return_address)
      return caller->target;
  }

  return NULL;
}

static gboolean
gum_probe_module_for_code_cave (GumModule * module,
                                gpointer user_data)
{
  GumProbeRangeForCodeCaveContext * ctx = user_data;
  const GumAddressSpec * caller = ctx->caller;
  const GumMemoryRange * range;
  GumAddress header_address, cave_address;
  gsize distance;
  const guint8 empty_cave[GUM_CODE_DEFLECTOR_CAVE_SIZE] = { 0, };

  range = gum_module_get_range (module);
  header_address = range->base_address;

#ifdef HAVE_DARWIN
  cave_address = header_address + 4096 - sizeof (empty_cave);
#else
  cave_address = header_address + 8;
#endif

  distance = ABS ((gssize) cave_address - (gssize) caller->near_address);
  if (distance > caller->max_distance)
    return TRUE;

  if (memcmp (GSIZE_TO_POINTER (cave_address), empty_cave,
      sizeof (empty_cave)) != 0)
  {
#ifdef HAVE_DARWIN
    gboolean found_empty_cave, nothing_in_front_of_cave;

    found_empty_cave = FALSE;
    nothing_in_front_of_cave = TRUE;

    do
    {
      cave_address -= sizeof (empty_cave);

      found_empty_cave = memcmp (GSIZE_TO_POINTER (cave_address), empty_cave,
          sizeof (empty_cave)) == 0;
    }
    while (!found_empty_cave && cave_address > header_address + 0x500);

    if (found_empty_cave)
    {
      gsize offset;

      for (offset = sizeof (empty_cave);
          offset <= 2 * sizeof (empty_cave);
          offset += sizeof (empty_cave))
      {
        nothing_in_front_of_cave = memcmp (
            GSIZE_TO_POINTER (cave_address - offset), empty_cave,
            sizeof (empty_cave)) == 0;
      }
    }

    if (!(found_empty_cave && nothing_in_front_of_cave))
      return TRUE;
#else
    return TRUE;
#endif
  }

  ctx->cave.base_address = cave_address;
  ctx->cave.size = sizeof (empty_cave);
  return FALSE;
}
