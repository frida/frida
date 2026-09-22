/*
 * Copyright (C) 2015-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023 Fabian Freyer <fabian.freyer@physik.tu-berlin.de>
 * Copyright (C) 2026 Håvard Sørbø <havard@hsorbo.no>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gum/gumdarwinmapper.h"

#include "gum/gumdarwin.h"
#include "gumdarwinmodule.h"
#include "helpers/fixupchainprocessor.h"

#include <dlfcn.h>
#include <string.h>
#ifdef HAVE_I386
# include <gum/arch-x86/gumx86writer.h>
#else
# include <gum/arch-arm/gumthumbwriter.h>
# include <gum/arch-arm64/gumarm64writer.h>
#endif

#define GUM_MAPPER_HEADER_BASE_SIZE         64
#define GUM_MAPPER_CODE_BASE_SIZE           80
#define GUM_MAPPER_DEPENDENCY_SIZE          32
#define GUM_MAPPER_CHAINED_FIXUP_CALL_SIZE  64
#define GUM_MAPPER_THREADED_BINDS_CALL_SIZE 64
#define GUM_MAPPER_RESOLVER_SIZE            40
#define GUM_MAPPER_INIT_SIZE               128
#define GUM_MAPPER_TERM_SIZE                64
#define GUM_MAPPER_INIT_TLV_SIZE           196

#define GUM_CHECK_MACH_RESULT(n1, cmp, n2, op) \
    if (!(n1 cmp n2)) \
    { \
      failed_operation = op; \
      goto mach_failure; \
    }

typedef struct _GumDarwinMapping GumDarwinMapping;
typedef struct _GumDarwinSymbolValue GumDarwinSymbolValue;

typedef struct _GumAccumulateFootprintContext GumAccumulateFootprintContext;

typedef struct _GumMapContext GumMapContext;

typedef struct _GumLibdyldDyld4Section32 GumLibdyldDyld4Section32;
typedef struct _GumLibdyldDyld4Section64 GumLibdyldDyld4Section64;

struct _GumDarwinMapper
{
  GObject object;

  gchar * name;
  GumDarwinModule * module;
  GumDarwinModuleImage * image;
  GumDarwinModuleResolver * resolver;
  GumDarwinModuleResolver * local_resolver;
  GumDarwinMapper * parent;

  gboolean mapped;
  GPtrArray * dependencies;
  GPtrArray * apple_parameters;

  gsize vm_size;
  gpointer runtime;
  GumAddress runtime_address;
  GumAddress empty_strv;
  GumAddress apple_strv;
  GumAddress process_chained_fixups;
  GumAddress chained_symbols_vector;
  GumAddress tlv_get_addr_addr;
  GumAddress tlv_area;
  GumAddress pthread_key;
  GumAddress pthread_key_create;
  GumAddress pthread_key_delete;
  gsize runtime_vm_size;
  gsize runtime_file_size;
  gsize runtime_header_size;
  gsize constructor_offset;
  gsize destructor_offset;
  guint chained_fixups_count;
  GumMemoryRange shared_cache_range;
  GumDarwinTlvParameters tlv;

  GArray * chained_symbols;
  GArray * threaded_symbols;
  GArray * threaded_regions;

  GSList * children;
  GHashTable * mappings;
};

enum
{
  PROP_0,
  PROP_NAME,
  PROP_MODULE,
  PROP_RESOLVER,
  PROP_PARENT
};

struct _GumDarwinMapping
{
  gint ref_count;
  GumDarwinModule * module;
  GumDarwinMapper * mapper;
};

struct _GumDarwinSymbolValue
{
  GumAddress address;
  GumAddress resolver;
};

struct _GumAccumulateFootprintContext
{
  GumDarwinMapper * mapper;
  gsize total;
  guint chained_fixups_count;
  guint chained_imports_count;
  guint threaded_regions_count;
};

struct _GumMapContext
{
  GumDarwinMapper * mapper;
  gboolean success;
  GError ** error;
};

struct _GumLibdyldDyld4Section32
{
  guint32 apis;
  guint32 all_image_infos;
  guint32 default_vars[5];
  guint32 dyld_lookup_func_addr;
  guint32 tlv_get_addr_addr;
};

struct _GumLibdyldDyld4Section64
{
  guint64 apis;
  guint64 all_image_infos;
  guint64 default_vars[5];
  guint64 dyld_lookup_func_addr;
  guint64 tlv_get_addr_addr;
};

static void gum_darwin_mapper_constructed (GObject * object);
static void gum_darwin_mapper_dispose (GObject * object);
static void gum_darwin_mapper_finalize (GObject * object);
static void gum_darwin_mapper_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec);
static void gum_darwin_mapper_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec);

static GumDarwinMapper * gum_darwin_mapper_new_from_file_with_parent (
    GumDarwinMapper * parent, const gchar * path,
    GumDarwinModuleResolver * resolver, GError ** error);
static gsize gum_darwin_mapper_get_footprint_budget (GumDarwinMapper * self);
static void gum_darwin_mapper_discard_footprint_budget (GumDarwinMapper * self);
static void gum_darwin_mapper_init_footprint_budget (GumDarwinMapper * self);
static GumAddress gum_darwin_mapper_make_code_address (GumDarwinMapper * self,
    GumAddress value);

static void gum_darwin_mapper_alloc_and_emit_runtime (GumDarwinMapper * self,
    GumAddress base_address, gsize size);
static void gum_emit_runtime (GumDarwinMapper * self, gpointer output_buffer,
    GumAddress pc, gsize * size);
static gboolean gum_accumulate_chained_fixups_size (
    const GumDarwinChainedFixupsDetails * details, gpointer user_data);
static gboolean gum_accumulate_bind_footprint_size (
    const GumDarwinBindDetails * details, gpointer user_data);
static void gum_accumulate_bind_pointer_footprint_size (
    GumAccumulateFootprintContext * ctx, const GumDarwinBindDetails * details);
static void gum_accumulate_bind_threaded_table_footprint_size (
    GumAccumulateFootprintContext * ctx, const GumDarwinBindDetails * details);
static void gum_accumulate_bind_threaded_items_footprint_size (
    GumAccumulateFootprintContext * ctx, const GumDarwinBindDetails * details);
static gboolean gum_accumulate_init_pointers_footprint_size (
    const GumDarwinInitPointersDetails * details, gpointer user_data);
static gboolean gum_accumulate_init_offsets_footprint_size (
    const GumDarwinInitOffsetsDetails * details, gpointer user_data);
static gboolean gum_accumulate_term_footprint_size (
    const GumDarwinTermPointersDetails * details, gpointer user_data);

static gpointer gum_darwin_mapper_data_from_offset (GumDarwinMapper * self,
    guint64 offset, guint size);
static GumDarwinMapping * gum_darwin_mapper_get_dependency_by_ordinal (
    GumDarwinMapper * self, gint ordinal, GError ** error);
static GumDarwinMapping * gum_darwin_mapper_get_dependency_by_name (
    GumDarwinMapper * self, const gchar * name, GError ** error);
static gboolean gum_darwin_mapper_resolve_import (GumDarwinMapper * self,
    gint library_ordinal, const gchar * symbol_name, gboolean is_weak,
    GumDarwinSymbolValue * value, GError ** error);
static gboolean gum_darwin_mapper_resolve_symbol (GumDarwinMapper * self,
    GumDarwinModule * module, const gchar * symbol,
    GumDarwinSymbolValue * value);
static GumDarwinMapping * gum_darwin_mapper_add_existing_mapping (
    GumDarwinMapper * self, GumDarwinModule * module);
static GumDarwinMapping * gum_darwin_mapper_add_pending_mapping (
    GumDarwinMapper * self, const gchar * name, GumDarwinMapper * mapper);
static GumDarwinMapping * gum_darwin_mapper_add_alias_mapping (
    GumDarwinMapper * self, const gchar * name, const GumDarwinMapping * to);
static gboolean gum_darwin_mapper_resolve_chained_imports (
    const GumDarwinChainedFixupsDetails * details, gpointer user_data);
static gboolean gum_darwin_mapper_append_chained_symbol (GumDarwinMapper * self,
    gint library_ordinal, const gchar * symbol_name, gboolean is_weak,
    gint64 addend, GError ** error);
static gboolean gum_darwin_mapper_rebase (
    const GumDarwinRebaseDetails * details, gpointer user_data);
static gboolean gum_darwin_mapper_bind (const GumDarwinBindDetails * details,
    gpointer user_data);
static gboolean gum_darwin_mapper_bind_pointer (GumDarwinMapper * self,
    const GumDarwinBindDetails * bind, GError ** error);
static gboolean gum_darwin_mapper_bind_table (GumDarwinMapper * self,
    const GumDarwinBindDetails * bind, GError ** error);
static gboolean gum_darwin_mapper_bind_items (GumDarwinMapper * self,
    const GumDarwinBindDetails * bind, GError ** error);

static void gum_darwin_mapping_free (GumDarwinMapping * self);

static gboolean gum_find_tlv_get_addr (const GumDarwinSectionDetails * details,
    gpointer user_data);

G_DEFINE_TYPE (GumDarwinMapper, gum_darwin_mapper, G_TYPE_OBJECT)

#if defined (HAVE_ARM) || defined (HAVE_ARM64)
/* Compiled from helpers/threadedbindprocessor.c */
const guint32 gum_threaded_bind_processor_code[] = {
  0xd2800008U, 0x2a0403e9U, 0xeb09011fU, 0x54000620U, 0xf86878aaU, 0xf940014bU,
  0xb7f001ebU, 0xd36bfd6cU, 0x9240a96dU, 0x936aa96eU, 0x925531ceU, 0xb3481d8dU,
  0xaa0e01acU, 0x92407d6dU, 0xf241017fU, 0x9a8003eeU, 0x9a8d018cU, 0x8b0101cdU,
  0x8b0c01acU, 0xb6f8036bU, 0x14000004U, 0x92403d6cU, 0xf86c786cU, 0xb6f802ebU,
  0xd371fd6eU, 0xd360bd6dU, 0xaa0a03efU, 0xb3503dafU, 0xf250017fU, 0x9a8f01adU,
  0x924005d0U, 0xf1000e1fU, 0x9a9f9210U, 0x10000291U, 0xd503201fU, 0xb8b07a30U,
  0x10000011U, 0x8b100230U, 0xd61f0200U, 0xdac101acU, 0x14000006U, 0xdac109acU,
  0x14000004U, 0xdac105acU, 0x14000002U, 0xdac10dacU, 0xd373f56bU, 0xf900014cU,
  0x8b2b4d4aU, 0x35fffa8bU, 0x91000508U, 0x17ffffcfU, 0xd65f03c0U, 0x0000000cU,
  0x0000001cU, 0x00000014U, 0x00000024U
};
#endif

/* Compiled from helpers/fixupchainprocessor.c */
#if defined (HAVE_ARM) || defined (HAVE_ARM64)
const guint32 gum_fixup_chain_processor_code[] = {
  0xd10283ffU, 0xa9046ffcU, 0xa90567faU, 0xa9065ff8U, 0xa90757f6U, 0xa9084ff4U,
  0xa9097bfdU, 0x910243fdU, 0xaa0303f3U, 0xaa0103f5U, 0xd2800009U, 0xb9400408U,
  0x8b080008U, 0xa9000be8U, 0xb840450aU, 0xa9012be8U, 0xb26db3fcU, 0xf9400fe8U,
  0xeb08013fU, 0x54000c80U, 0xf90013e9U, 0xf9400be8U, 0xb8697908U, 0x34000ba8U,
  0xd2800018U, 0xf94003e9U, 0x8b08013aU, 0x79400f48U, 0x79402b4aU, 0x91005b49U,
  0xa9032be9U, 0x121d7909U, 0xb9002fe9U, 0x7100311fU, 0x529fffe9U, 0x12bfe00aU,
  0x9a89015bU, 0x7100051fU, 0xf94007eaU, 0x9a9f0149U, 0xcb0902b7U, 0x7100191fU,
  0x9a8a03e8U, 0xcb0802b4U, 0xf9401fe8U, 0xeb08031fU, 0x540008c0U, 0xf9401be8U,
  0x78787908U, 0x529fffe9U, 0xeb09011fU, 0x540007e0U, 0xf9400749U, 0x8b0902a9U,
  0x79400b4aU, 0x9b0a2709U, 0x8b080136U, 0xb9402fe8U, 0x7100091fU, 0x54000241U,
  0xf94002c8U, 0xb7f800e8U, 0xd36cad09U, 0x92481d29U, 0x92408d0aU, 0x8b0a028aU,
  0x8b090149U, 0x14000005U, 0x92405d09U, 0xf8697a69U, 0xd358fd0aU, 0x8b2a0129U,
  0xf90002c9U, 0xd373f908U, 0x8b080ad6U, 0xb5fffe28U, 0x14000026U, 0xf94002d9U,
  0xd37eff30U, 0xd360bf22U, 0xd370c323U, 0xd371cb21U, 0xf1000e1fU, 0x9a9f9210U,
  0x10000571U, 0xd503201fU, 0xb8b07a30U, 0x10000011U, 0x8b100230U, 0xd61f0200U,
  0xd373cb28U, 0x92481d08U, 0x9240ab29U, 0x8b0902e9U, 0x8b080120U, 0x1400000fU,
  0x8a1b0328U, 0xf8687a68U, 0xd360cb29U, 0xf141013fU, 0x9a9c33e9U, 0xb360cb29U,
  0x8b090100U, 0x14000007U, 0x8b3942a0U, 0x14000003U, 0x8a1b0328U, 0xf8687a60U,
  0xaa1603e4U, 0x94000016U, 0xf90002c0U, 0xd373f728U, 0x8b080ed6U, 0xb5fffb88U,
  0x91000718U, 0x17ffffb9U, 0xf94013e9U, 0x91000529U, 0x17ffff9bU, 0xa9497bfdU,
  0xa9484ff4U, 0xa94757f6U, 0xa9465ff8U, 0xa94567faU, 0xa9446ffcU, 0x910283ffU,
  0xd65f03c0U, 0x0000000cU, 0x00000024U, 0x00000044U, 0x0000004cU, 0xb3503c44U,
  0x7100007fU, 0x9a840048U, 0x71000c3fU, 0x54000228U, 0x2a0103f0U, 0xf1000e1fU,
  0x9a9f9210U, 0x100001d1U, 0xd503201fU, 0xb8b07a30U, 0x10000011U, 0x8b100230U,
  0xd61f0200U, 0xdac10100U, 0xd65f03c0U, 0xdac10500U, 0xd65f03c0U, 0xdac10900U,
  0xd65f03c0U, 0xdac10d00U, 0xd65f03c0U, 0x0000000cU, 0x00000014U, 0x0000001cU,
  0x00000024U
};
#else
const guint8 gum_fixup_chain_processor_code[] = {
  0x55, 0x48, 0x89, 0xe5, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55, 0x41, 0x54, 0x53,
  0x48, 0x89, 0x55, 0xd0, 0x8b, 0x47, 0x04, 0x4c, 0x8d, 0x14, 0x07, 0x8b, 0x04,
  0x07, 0x48, 0x89, 0x45, 0xc8, 0x49, 0xbd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xff, 0x48, 0xb8, 0xff, 0xff, 0xff, 0xff, 0x0f, 0x00, 0x00, 0x00, 0x45,
  0x31, 0xdb, 0x4c, 0x3b, 0x5d, 0xc8, 0x0f, 0x84, 0xb4, 0x00, 0x00, 0x00, 0x43,
  0x8b, 0x7c, 0x9a, 0x04, 0x48, 0x85, 0xff, 0x0f, 0x84, 0x9e, 0x00, 0x00, 0x00,
  0x4d, 0x8d, 0x3c, 0x3a, 0x45, 0x0f, 0xb7, 0x74, 0x3a, 0x14, 0x66, 0x41, 0x83,
  0x7c, 0x3a, 0x06, 0x06, 0x48, 0x8b, 0x7d, 0xd0, 0xba, 0x00, 0x00, 0x00, 0x00,
  0x48, 0x0f, 0x44, 0xfa, 0x48, 0x89, 0xf3, 0x48, 0x29, 0xfb, 0x45, 0x31, 0xe4,
  0x4d, 0x39, 0xf4, 0x74, 0x72, 0x43, 0x0f, 0xb7, 0x7c, 0x67, 0x16, 0x48, 0x81,
  0xff, 0xff, 0xff, 0x00, 0x00, 0x74, 0x5e, 0x41, 0x0f, 0xb7, 0x57, 0x04, 0x49,
  0x0f, 0xaf, 0xd4, 0x49, 0x03, 0x7f, 0x08, 0x48, 0x01, 0xd7, 0x48, 0x01, 0xf7,
  0x4c, 0x8b, 0x0f, 0x4d, 0x85, 0xc9, 0x78, 0x18, 0x4c, 0x89, 0xca, 0x48, 0xc1,
  0xe2, 0x14, 0x4c, 0x21, 0xea, 0x4d, 0x89, 0xc8, 0x49, 0x21, 0xc0, 0x49, 0x01,
  0xd8, 0x49, 0x01, 0xd0, 0xeb, 0x14, 0x44, 0x89, 0xca, 0x81, 0xe2, 0xff, 0xff,
  0xff, 0x00, 0x45, 0x89, 0xc8, 0x41, 0xc1, 0xe8, 0x18, 0x4c, 0x03, 0x04, 0xd1,
  0x4c, 0x89, 0x07, 0x49, 0xc1, 0xe9, 0x33, 0x41, 0x81, 0xe1, 0xff, 0x0f, 0x00,
  0x00, 0x4a, 0x8d, 0x3c, 0x8f, 0x4d, 0x85, 0xc9, 0x75, 0xb5, 0x49, 0xff, 0xc4,
  0xeb, 0x89, 0x49, 0xff, 0xc3, 0xe9, 0x42, 0xff, 0xff, 0xff, 0x5b, 0x41, 0x5c,
  0x41, 0x5d, 0x41, 0x5e, 0x41, 0x5f, 0x5d, 0xc3
};
#endif

static void
gum_darwin_mapper_class_init (GumDarwinMapperClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gum_darwin_mapper_constructed;
  object_class->dispose = gum_darwin_mapper_dispose;
  object_class->finalize = gum_darwin_mapper_finalize;
  object_class->get_property = gum_darwin_mapper_get_property;
  object_class->set_property = gum_darwin_mapper_set_property;

  g_object_class_install_property (object_class, PROP_NAME,
      g_param_spec_string ("name", "Name", "Name", NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_MODULE,
      g_param_spec_object ("module", "Module", "Module",
      GUM_TYPE_DARWIN_MODULE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_RESOLVER,
      g_param_spec_object ("resolver", "Resolver", "Module resolver",
      GUM_DARWIN_TYPE_MODULE_RESOLVER, G_PARAM_READWRITE |
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_PARENT,
      g_param_spec_object ("parent", "Parent", "Parent mapper",
      GUM_DARWIN_TYPE_MAPPER, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS));
}

static void
gum_darwin_mapper_init (GumDarwinMapper * self)
{
  self->local_resolver =
      gum_darwin_module_resolver_new (mach_task_self (), NULL);

  self->mapped = FALSE;
  self->apple_parameters = g_ptr_array_new_with_free_func (g_free);
}

static void
gum_darwin_mapper_constructed (GObject * object)
{
  GumDarwinMapper * self = GUM_DARWIN_MAPPER (object);
  GumDarwinMapper * parent = self->parent;

  g_assert (self->name != NULL);
  g_assert (self->module != NULL);
  g_assert (self->resolver != NULL);

  gum_darwin_query_shared_cache_range (self->resolver->task,
      &self->shared_cache_range);

  gum_darwin_module_query_tlv_parameters (self->module, &self->tlv);

  if (self->tlv.num_descriptors != 0)
  {
    GumDarwinModule * pthread = gum_darwin_module_resolver_find_module_by_name (
        self->resolver, "/usr/lib/system/libsystem_pthread.dylib");
    if (pthread != NULL)
    {
      self->pthread_key_create =
          gum_darwin_module_resolver_find_export_address (self->resolver,
              pthread, "pthread_key_create");
      self->pthread_key_delete =
          gum_darwin_module_resolver_find_export_address (self->resolver,
              pthread, "pthread_key_delete");
      g_object_unref (pthread);
    }
  }

  if (parent != NULL)
  {
    parent->children = g_slist_prepend (parent->children, self);

    gum_darwin_mapper_add_pending_mapping (parent, self->name, self);
  }
}

static void
gum_darwin_mapper_dispose (GObject * object)
{
  GumDarwinMapper * self = GUM_DARWIN_MAPPER (object);

  g_clear_object (&self->local_resolver);
  g_clear_object (&self->resolver);
  g_clear_object (&self->module);

  G_OBJECT_CLASS (gum_darwin_mapper_parent_class)->dispose (object);
}

static void
gum_darwin_mapper_finalize (GObject * object)
{
  GumDarwinMapper * self = GUM_DARWIN_MAPPER (object);

  g_clear_pointer (&self->mappings, g_hash_table_unref);
  g_slist_free_full (self->children, g_object_unref);

  g_clear_pointer (&self->threaded_regions, g_array_unref);
  g_clear_pointer (&self->threaded_symbols, g_array_unref);
  g_clear_pointer (&self->chained_symbols, g_array_unref);

  g_free (self->runtime);

  g_ptr_array_unref (self->apple_parameters);
  g_ptr_array_unref (self->dependencies);

  g_free (self->name);

  G_OBJECT_CLASS (gum_darwin_mapper_parent_class)->finalize (object);
}

static void
gum_darwin_mapper_get_property (GObject * object,
                                guint property_id,
                                GValue * value,
                                GParamSpec * pspec)
{
  GumDarwinMapper * self = GUM_DARWIN_MAPPER (object);

  switch (property_id)
  {
    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;
    case PROP_MODULE:
      g_value_set_object (value, self->module);
      break;
    case PROP_RESOLVER:
      g_value_set_object (value, self->resolver);
      break;
    case PROP_PARENT:
      g_value_set_object (value, self->parent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
gum_darwin_mapper_set_property (GObject * object,
                                guint property_id,
                                const GValue * value,
                                GParamSpec * pspec)
{
  GumDarwinMapper * self = GUM_DARWIN_MAPPER (object);

  switch (property_id)
  {
    case PROP_NAME:
      g_free (self->name);
      self->name = g_value_dup_string (value);
      break;
    case PROP_MODULE:
      g_clear_object (&self->module);
      self->module = g_value_dup_object (value);
      self->image = (self->module != NULL) ? self->module->image : NULL;
      break;
    case PROP_RESOLVER:
      g_clear_object (&self->resolver);
      self->resolver = g_value_dup_object (value);
      break;
    case PROP_PARENT:
      self->parent = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

GumDarwinMapper *
gum_darwin_mapper_new_from_file (const gchar * path,
                                 GumDarwinModuleResolver * resolver,
                                 GError ** error)
{
  return gum_darwin_mapper_new_from_file_with_parent (NULL, path, resolver,
      error);
}

GumDarwinMapper *
gum_darwin_mapper_new_take_blob (const gchar * name,
                                 GBytes * blob,
                                 GumDarwinModuleResolver * resolver,
                                 GError ** error)
{
  GumDarwinModule * module;
  GumDarwinMapper * mapper;

  module = gum_darwin_module_new_from_blob (blob, resolver->cpu_type,
      resolver->ptrauth_support, GUM_DARWIN_MODULE_FLAGS_NONE, error);
  if (module == NULL)
    goto malformed_blob;

  if (module->name == NULL)
    g_object_set (module, "name", name, NULL);

  mapper = g_object_new (GUM_DARWIN_TYPE_MAPPER,
      "name", name,
      "module", module,
      "resolver", resolver,
      NULL);
  if (!gum_darwin_mapper_load (mapper, error))
  {
    g_object_unref (mapper);
    mapper = NULL;
  }

  g_object_unref (module);
  g_bytes_unref (blob);

  return mapper;

malformed_blob:
  {
    g_bytes_unref (blob);

    return NULL;
  }
}

static GumDarwinMapper *
gum_darwin_mapper_new_from_file_with_parent (GumDarwinMapper * parent,
                                             const gchar * path,
                                             GumDarwinModuleResolver * resolver,
                                             GError ** error)
{
  GumDarwinMapper * mapper = NULL;
  GumDarwinModule * module;

  module = gum_darwin_module_new_from_file (path, resolver->cpu_type,
      resolver->ptrauth_support, GUM_DARWIN_MODULE_FLAGS_NONE,
      error);
  if (module == NULL)
    goto beach;

  if (module->name == NULL)
    g_object_set (module, "name", path, NULL);

  mapper = g_object_new (GUM_DARWIN_TYPE_MAPPER,
      "name", path,
      "module", module,
      "resolver", resolver,
      "parent", parent,
      NULL);
  if (!gum_darwin_mapper_load (mapper, error))
  {
    g_object_unref (mapper);
    mapper = NULL;
  }

beach:
  g_clear_object (&module);

  return mapper;
}

gboolean
gum_darwin_mapper_load (GumDarwinMapper * self,
                        GError ** error)
{
  GumDarwinModule * module = self->module;
  GArray * dependencies;
  guint i;

  if (self->dependencies != NULL)
    return TRUE;

  self->dependencies = g_ptr_array_sized_new (5);

  if (self->parent == NULL)
  {
    self->mappings = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
        (GDestroyNotify) gum_darwin_mapping_free);
    gum_darwin_mapper_add_pending_mapping (self, module->name, self);
  }

  dependencies = module->dependencies;
  for (i = 0; i != dependencies->len; i++)
  {
    GumDarwinMapping * dependency;

    dependency = gum_darwin_mapper_get_dependency_by_name (self,
        g_array_index (dependencies, GumDependencyDetails, i).name,
        error);
    if (dependency == NULL)
      return FALSE;
    g_ptr_array_add (self->dependencies, dependency);
  }

  return TRUE;
}

void
gum_darwin_mapper_add_apple_parameter (GumDarwinMapper * self,
                                       const gchar * key,
                                       const gchar * value)
{
  g_ptr_array_add (self->apple_parameters, g_strconcat (key, "=", value, NULL));

  gum_darwin_mapper_discard_footprint_budget (self);
}

gsize
gum_darwin_mapper_size (GumDarwinMapper * self)
{
  gsize total;
  GSList * cur;

  total = 0;

  for (cur = self->children; cur != NULL; cur = cur->next)
  {
    GumDarwinMapper * child = cur->data;

    total += gum_darwin_mapper_get_footprint_budget (child);
  }

  total += gum_darwin_mapper_get_footprint_budget (self);

  return total;
}

static gsize
gum_darwin_mapper_get_footprint_budget (GumDarwinMapper * self)
{
  if (self->vm_size == 0)
    gum_darwin_mapper_init_footprint_budget (self);

  return self->vm_size;
}

static void
gum_darwin_mapper_discard_footprint_budget (GumDarwinMapper * self)
{
  self->vm_size = 0;
}

static void
gum_darwin_mapper_init_footprint_budget (GumDarwinMapper * self)
{
  GumDarwinModule * module = self->module;
  GumDarwinModuleImage * image = self->image;
  gsize pointer_size = self->module->pointer_size;
  guint page_size = self->resolver->page_size;
  gsize segments_size;
  guint i;
  GumAccumulateFootprintContext runtime;
  gsize header_size;
  GPtrArray * params;
  const gsize rounded_alignment_padding_for_code = 4;
  const gsize rounded_alignment_padding_for_pointers = pointer_size;

  if (image->shared_segments->len == 0)
  {
    segments_size = 0;
    for (i = 0; i != module->segments->len; i++)
    {
      GumDarwinSegment * segment =
          &g_array_index (module->segments, GumDarwinSegment, i);

      segments_size += segment->vm_size;
      if (segment->vm_size % page_size != 0)
        segments_size += page_size - (segment->vm_size % page_size);
    }
  }
  else
  {
    segments_size = image->size;
  }

  runtime.mapper = self;
  runtime.total = 0;
  runtime.chained_fixups_count = 0;
  runtime.chained_imports_count = 0;
  runtime.threaded_regions_count = 0;

  header_size = GUM_MAPPER_HEADER_BASE_SIZE;
  params = self->apple_parameters;
  header_size += params->len * pointer_size;
  for (i = 0; i != params->len; i++)
  {
    const gchar * param = g_ptr_array_index (params, i);
    header_size += strlen (param) + 1;
  }
  header_size = GUM_ALIGN_SIZE (header_size, 16);

  gum_darwin_module_enumerate_chained_fixups (module,
      gum_accumulate_chained_fixups_size, &runtime);
  gum_darwin_module_enumerate_binds (module,
      gum_accumulate_bind_footprint_size, &runtime);
  gum_darwin_module_enumerate_lazy_binds (module,
      gum_accumulate_bind_footprint_size, &runtime);
  gum_darwin_module_enumerate_init_pointers (module,
      gum_accumulate_init_pointers_footprint_size, &runtime);
  gum_darwin_module_enumerate_init_offsets (module,
      gum_accumulate_init_offsets_footprint_size, &runtime);
  gum_darwin_module_enumerate_term_pointers (module,
      gum_accumulate_term_footprint_size, &runtime);

  if (self->tlv.num_descriptors != 0)
    runtime.total += GUM_MAPPER_INIT_TLV_SIZE;

  if (runtime.chained_fixups_count != 0)
  {
    header_size += rounded_alignment_padding_for_code;
    header_size += sizeof (gum_fixup_chain_processor_code);
  }

  if (runtime.chained_imports_count != 0)
  {
    header_size += rounded_alignment_padding_for_pointers;
    header_size += runtime.chained_imports_count * pointer_size;
  }

  runtime.total += header_size;
  runtime.total += g_slist_length (self->children) * GUM_MAPPER_DEPENDENCY_SIZE;
  runtime.total += GUM_MAPPER_CODE_BASE_SIZE;
  if (runtime.threaded_regions_count != 0)
    runtime.total += GUM_MAPPER_THREADED_BINDS_CALL_SIZE;

  self->runtime_vm_size = runtime.total;
  if (runtime.total % page_size != 0)
    self->runtime_vm_size += page_size - (runtime.total % page_size);
  self->runtime_file_size = runtime.total;
  self->runtime_header_size = header_size;

  self->vm_size = segments_size + self->runtime_vm_size;

  self->chained_fixups_count = runtime.chained_fixups_count;
}

gboolean
gum_darwin_mapper_map (GumDarwinMapper * self,
                       GumAddress base_address,
                       GError ** error)
{
  GumMapContext ctx;
  gsize total_vm_size;
  GumAddress macho_base_address;
  GSList * cur;
  GumDarwinModule * module = self->module;
  mach_port_t task = self->resolver->task;
  const GumDarwinTlvParameters * tlv = &self->tlv;
  guint i;
  mach_vm_address_t mapped_address;
  vm_prot_t cur_protection, max_protection;
  GArray * shared_segments;
  const gchar * failed_operation;
  kern_return_t kr;
  static gboolean use_memory_mapping = TRUE;

  g_assert (!self->mapped);

  ctx.mapper = self;
  ctx.success = TRUE;
  ctx.error = error;

  total_vm_size = gum_darwin_mapper_size (self);

  self->runtime_address = base_address;
  macho_base_address = base_address + self->runtime_vm_size;

  for (cur = self->children; cur != NULL; cur = cur->next)
  {
    GumDarwinMapper * child = cur->data;

    ctx.success = gum_darwin_mapper_map (child, macho_base_address, error);
    if (!ctx.success)
      goto beach;
    macho_base_address += child->vm_size;
  }

  g_object_set (module, "base-address", macho_base_address, NULL);

  gum_darwin_module_enumerate_chained_fixups (module,
      gum_darwin_mapper_resolve_chained_imports, &ctx);
  if (!ctx.success)
    goto beach;

  gum_darwin_module_enumerate_rebases (module, gum_darwin_mapper_rebase, &ctx);
  if (!ctx.success)
    goto beach;

  gum_darwin_module_enumerate_binds (module, gum_darwin_mapper_bind, &ctx);
  if (!ctx.success)
    goto beach;

  gum_darwin_module_enumerate_lazy_binds (module, gum_darwin_mapper_bind, &ctx);
  if (!ctx.success)
    goto beach;

  self->tlv_area = 0;
  if (tlv->num_descriptors != 0)
  {
    GumDarwinModule * libdyld;

    libdyld = gum_darwin_module_resolver_find_module_by_name (self->resolver,
        "libdyld.dylib");
    ctx.success = FALSE;
    gum_darwin_module_enumerate_sections (libdyld, gum_find_tlv_get_addr, &ctx);
    g_object_unref (libdyld);
    if (!ctx.success)
      goto unsupported_dyld_version;

    kr = mach_vm_allocate (task, &self->tlv_area,
        tlv->data_size + tlv->bss_size, VM_FLAGS_ANYWHERE);
    GUM_CHECK_MACH_RESULT (kr, ==, KERN_SUCCESS, "mach_vm_allocate(tlv)");

    kr = mach_vm_protect (task, self->tlv_area,
        tlv->data_size + tlv->bss_size, FALSE, VM_PROT_READ | VM_PROT_WRITE);
    GUM_CHECK_MACH_RESULT (kr, ==, KERN_SUCCESS, "mach_vm_protect(tlv)");

    kr = mach_vm_write (task, self->tlv_area,
        GPOINTER_TO_SIZE (self->image->data) + tlv->data_offset,
        tlv->data_size);
    GUM_CHECK_MACH_RESULT (kr, ==, KERN_SUCCESS, "mach_vm_write(tlv)");
  }

  gum_darwin_mapper_alloc_and_emit_runtime (self, base_address, total_vm_size);

  for (i = 0; i != module->segments->len; i++)
  {
    GumDarwinSegment * s =
        &g_array_index (module->segments, GumDarwinSegment, i);
    GumAddress segment_address;
    guint64 file_offset;

    if (s->file_size == 0)
      continue;

    segment_address =
        macho_base_address + s->vm_address - module->preferred_address;
    file_offset =
        (s->file_offset != 0) ? s->file_offset - self->image->source_offset : 0;

    mapped_address = segment_address;
    if (use_memory_mapping)
    {
      kr = mach_vm_remap (task, &mapped_address, s->file_size, 0,
          VM_FLAGS_OVERWRITE, mach_task_self (),
          (vm_offset_t) (self->image->data + file_offset), TRUE,
          &cur_protection, &max_protection, VM_INHERIT_COPY);
      GUM_CHECK_MACH_RESULT (kr, ==, KERN_SUCCESS, "mach_vm_remap(segment)");

      kr = mach_vm_protect (task, segment_address, s->vm_size, FALSE,
          s->protection);
      if (kr == KERN_PROTECTION_FAILURE)
      {
        use_memory_mapping = FALSE;

        kr = mach_vm_allocate (task, &mapped_address, s->vm_size,
            VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE);
        GUM_CHECK_MACH_RESULT (kr, ==, KERN_SUCCESS, "mach_vm_allocate(oops)");

        goto fallback;
      }
      else
      {
        GUM_CHECK_MACH_RESULT (kr, ==, KERN_SUCCESS,
            "mach_vm_protect(segment)");
      }
    }
    else
    {
fallback:
      kr = mach_vm_write (task, segment_address,
          (vm_offset_t) (self->image->data + file_offset), s->file_size);
      GUM_CHECK_MACH_RESULT (kr, ==, KERN_SUCCESS, "mach_vm_write(segment)");

      kr = mach_vm_protect (task, segment_address, s->vm_size, FALSE,
          s->protection);
      GUM_CHECK_MACH_RESULT (kr, ==, KERN_SUCCESS, "mach_vm_protect(segment)");
    }
  }

  shared_segments = self->image->shared_segments;
  for (i = 0; i != shared_segments->len; i++)
  {
    GumDarwinModuleImageSegment * s =
        &g_array_index (shared_segments, GumDarwinModuleImageSegment, i);

    mapped_address = macho_base_address + s->offset;
    kr = mach_vm_remap (task, &mapped_address, s->size, 0, VM_FLAGS_OVERWRITE,
        mach_task_self (), (vm_offset_t) (self->image->data + s->offset), TRUE,
        &cur_protection, &max_protection, VM_INHERIT_COPY);
    GUM_CHECK_MACH_RESULT (kr, ==, KERN_SUCCESS,
        "mach_vm_remap(shared_segment)");

    kr = mach_vm_protect (task, macho_base_address + s->offset, s->size, FALSE,
        s->protection);
    GUM_CHECK_MACH_RESULT (kr, ==, KERN_SUCCESS,
        "mach_vm_protect(shared_segment)");
  }

  if (gum_query_is_rwx_supported () || !gum_code_segment_is_supported ())
  {
    kr = mach_vm_write (task, self->runtime_address,
        (vm_offset_t) self->runtime, self->runtime_file_size);
    GUM_CHECK_MACH_RESULT (kr, ==, KERN_SUCCESS, "mach_vm_write(runtime)");

    kr = mach_vm_protect (task, self->runtime_address, self->runtime_vm_size,
        FALSE, VM_PROT_READ | VM_PROT_EXECUTE);
    GUM_CHECK_MACH_RESULT (kr, ==, KERN_SUCCESS, "mach_vm_protect(runtime)");
  }
  else
  {
    GumCodeSegment * segment;
    guint8 * scratch_page;

    segment = gum_code_segment_new (self->runtime_vm_size, NULL);

    scratch_page = gum_code_segment_get_address (segment);
    memcpy (scratch_page, self->runtime, self->runtime_file_size);

    gum_code_segment_realize (segment);
    gum_code_segment_map (segment, 0, self->runtime_vm_size, scratch_page);

    mapped_address = self->runtime_address;
    kr = mach_vm_remap (task, &mapped_address, self->runtime_vm_size, 0,
        VM_FLAGS_OVERWRITE, mach_task_self (), (mach_vm_address_t) scratch_page,
        FALSE, &cur_protection, &max_protection, VM_INHERIT_COPY);

    gum_code_segment_free (segment);

    GUM_CHECK_MACH_RESULT (kr, ==, KERN_SUCCESS, "mach_vm_remap(runtime)");
  }

  self->mapped = TRUE;

beach:
  return ctx.success;

unsupported_dyld_version:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_FAILED,
        "Unsupported dyld version; please file a bug");
    return FALSE;
  }
mach_failure:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_FAILED,
        "Unexpected error while mapping dylib (%s returned '%s')",
        failed_operation, mach_error_string (kr));
    return FALSE;
  }
}

GumAddress
gum_darwin_mapper_constructor (GumDarwinMapper * self)
{
  g_assert (self->mapped);

  return gum_darwin_mapper_make_code_address (self, self->runtime_address +
      self->runtime_header_size + self->constructor_offset);
}

GumAddress
gum_darwin_mapper_destructor (GumDarwinMapper * self)
{
  g_assert (self->mapped);

  return gum_darwin_mapper_make_code_address (self, self->runtime_address +
      self->runtime_header_size + self->destructor_offset);
}

GumAddress
gum_darwin_mapper_resolve (GumDarwinMapper * self,
                           const gchar * symbol)
{
  GumDarwinModule * module = self->module;
  gchar * mangled_symbol;
  GumDarwinSymbolValue v;
  gboolean success;

  g_assert (self->mapped);

  mangled_symbol = g_strconcat ("_", symbol, NULL);
  success = gum_darwin_mapper_resolve_symbol (self, module, mangled_symbol, &v);
  g_free (mangled_symbol);

  if (!success)
    return 0;

  if (v.resolver != 0)
    return 0;

  if (gum_darwin_module_is_address_in_text_section (module, v.address))
    v.address = gum_darwin_mapper_make_code_address (self, v.address);

  return v.address;
}

static GumAddress
gum_darwin_mapper_make_code_address (GumDarwinMapper * self,
                                     GumAddress value)
{
  GumAddress result = value;

  if (self->resolver->ptrauth_support == GUM_PTRAUTH_SUPPORTED)
    result = gum_sign_code_address (result);

  return result;
}

static void
gum_darwin_mapper_alloc_and_emit_runtime (GumDarwinMapper * self,
                                          GumAddress base_address,
                                          gsize size)
{
  GPtrArray * params = self->apple_parameters;
  gsize header_size = self->runtime_header_size;
  gsize pointer_size = self->module->pointer_size;
  const GumDarwinTlvParameters * tlv = &self->tlv;
  gpointer runtime;
  guint strv_length, strv_size;
  gint * strv_offsets;
  GString * strv_blob;
  guint i;
  gpointer cursor;
  GumAddress pc, alignment_offset;
  gsize code_size;

  runtime = g_malloc0 (self->runtime_file_size);

  strv_length = 1 + params->len;
  strv_size = (strv_length + 1) * pointer_size;
  strv_offsets = g_newa (gint, strv_length);
  strv_blob = g_string_new ("");

  strv_offsets[0] = 0;
  g_string_append_printf (strv_blob,
      "frida_dylib_range=0x%" G_GINT64_MODIFIER "x,0x%" G_GSIZE_MODIFIER "x",
      base_address, size);

  for (i = 0; i != params->len; i++)
  {
    g_string_append_c (strv_blob, '\0');

    strv_offsets[1 + i] = strv_blob->len;
    g_string_append (strv_blob, g_ptr_array_index (params, i));
  }

  cursor = runtime;
  pc = base_address;

#define GUM_ADVANCE_BY(n) \
    G_STMT_START \
    { \
      cursor += n; \
      pc += n; \
    } \
    G_STMT_END

  self->apple_strv = pc;

  for (i = 0; i != strv_length; i++)
  {
    gint offset = strv_offsets[i];
    GumAddress str_address;

    str_address = base_address + strv_size + offset;

    if (pointer_size == 4)
      *((guint32 *) cursor) = str_address;
    else
      *((guint64 *) cursor) = str_address;

    GUM_ADVANCE_BY (pointer_size);
  }

  /* String vector terminator goes here. */
  self->empty_strv = pc;
  GUM_ADVANCE_BY (pointer_size);

  memcpy (cursor, strv_blob->str, strv_blob->len);
  GUM_ADVANCE_BY (strv_blob->len + 1);

  g_string_free (strv_blob, TRUE);

  if (self->chained_fixups_count != 0)
  {
    alignment_offset = pc % 4;
    if (alignment_offset != 0)
      GUM_ADVANCE_BY (4 - alignment_offset);

    self->process_chained_fixups = pc;
    memcpy (cursor, gum_fixup_chain_processor_code,
        sizeof (gum_fixup_chain_processor_code));
    GUM_ADVANCE_BY (sizeof (gum_fixup_chain_processor_code));
  }
  else
  {
    self->process_chained_fixups = 0;
  }

  if (self->chained_symbols != NULL && self->chained_symbols->len != 0)
  {
    alignment_offset = pc % pointer_size;
    if (alignment_offset != 0)
      GUM_ADVANCE_BY (pointer_size - alignment_offset);

    self->chained_symbols_vector = pc;
    memcpy (cursor, self->chained_symbols->data,
        self->chained_symbols->len * pointer_size);
    GUM_ADVANCE_BY (self->chained_symbols->len * pointer_size);
  }
  else
  {
    self->chained_symbols_vector = 0;
  }

  if (tlv->num_descriptors != 0)
  {
    self->pthread_key =
        self->module->base_address + tlv->descriptors_offset + pointer_size;
  }

#undef GUM_ADVANCE_BY

  gum_emit_runtime (self, runtime + header_size,
      self->runtime_address + header_size, &code_size);
  g_assert (header_size + code_size <= self->runtime_file_size);

  g_free (self->runtime);
  self->runtime = runtime;
}

#if defined (HAVE_I386)

typedef struct _GumEmitX86Context GumEmitX86Context;

struct _GumEmitX86Context
{
  GumDarwinMapper * mapper;
  GumX86Writer * cw;
};

static void gum_emit_child_constructor_call (GumDarwinMapper * child,
    GumEmitX86Context * ctx);
static void gum_emit_child_destructor_call (GumDarwinMapper * child,
    GumEmitX86Context * ctx);
static gboolean gum_emit_chained_fixup_call (
    const GumDarwinChainedFixupsDetails * details, GumEmitX86Context * ctx);
static gboolean gum_emit_resolve_if_needed (
    const GumDarwinBindDetails * details, GumEmitX86Context * ctx);
static gboolean gum_emit_init_calls (
    const GumDarwinInitPointersDetails * details, GumEmitX86Context * ctx);
static gboolean gum_emit_term_calls (
    const GumDarwinTermPointersDetails * details, GumEmitX86Context * ctx);
static void gum_emit_tlv_init_code (GumEmitX86Context * ctx);

static void
gum_emit_runtime (GumDarwinMapper * self,
                  gpointer output_buffer,
                  GumAddress pc,
                  gsize * size)
{
  GumDarwinModule * module = self->module;
  const GumDarwinTlvParameters * tlv = &self->tlv;
  GumX86Writer cw;
  GumEmitX86Context ctx;
  GSList * children_reversed;

  gum_x86_writer_init (&cw, output_buffer);
  cw.pc = pc;
  gum_x86_writer_set_target_cpu (&cw, self->module->cpu_type);

  ctx.mapper = self;
  ctx.cw = &cw;

  if (self->parent == NULL)
  {
    /* atexit stub */
    gum_x86_writer_put_xor_reg_reg (&cw, GUM_X86_XAX, GUM_X86_XAX);
    gum_x86_writer_put_ret (&cw);
  }

  self->constructor_offset = gum_x86_writer_offset (&cw);
  gum_x86_writer_put_push_reg (&cw, GUM_X86_XBP);
  gum_x86_writer_put_push_reg (&cw, GUM_X86_XBX);
  gum_x86_writer_put_sub_reg_imm (&cw, GUM_X86_XSP, self->module->pointer_size);

  g_slist_foreach (self->children, (GFunc) gum_emit_child_constructor_call,
      &ctx);
  gum_darwin_module_enumerate_chained_fixups (module,
      (GumFoundDarwinChainedFixupsFunc) gum_emit_chained_fixup_call, &ctx);
  gum_darwin_module_enumerate_binds (module,
      (GumFoundDarwinBindFunc) gum_emit_resolve_if_needed, &ctx);
  gum_darwin_module_enumerate_lazy_binds (module,
      (GumFoundDarwinBindFunc) gum_emit_resolve_if_needed, &ctx);

  if (tlv->num_descriptors != 0)
    gum_emit_tlv_init_code (&ctx);

  gum_darwin_module_enumerate_init_pointers (module,
      (GumFoundDarwinInitPointersFunc) gum_emit_init_calls, &ctx);

  gum_x86_writer_put_add_reg_imm (&cw, GUM_X86_XSP, self->module->pointer_size);
  gum_x86_writer_put_pop_reg (&cw, GUM_X86_XBX);
  gum_x86_writer_put_pop_reg (&cw, GUM_X86_XBP);
  gum_x86_writer_put_ret (&cw);

  self->destructor_offset = gum_x86_writer_offset (&cw);
  gum_x86_writer_put_push_reg (&cw, GUM_X86_XBP);
  gum_x86_writer_put_push_reg (&cw, GUM_X86_XBX);
  gum_x86_writer_put_sub_reg_imm (&cw, GUM_X86_XSP, self->module->pointer_size);

  gum_darwin_module_enumerate_term_pointers (module,
      (GumFoundDarwinTermPointersFunc) gum_emit_term_calls, &ctx);
  children_reversed = g_slist_reverse (g_slist_copy (self->children));
  g_slist_foreach (children_reversed, (GFunc) gum_emit_child_destructor_call,
      &ctx);
  g_slist_free (children_reversed);

  if (tlv->num_descriptors != 0)
  {
    gum_x86_writer_put_mov_reg_address (&cw, GUM_X86_XCX, self->pthread_key);
    gum_x86_writer_put_mov_reg_reg_ptr (&cw, GUM_X86_XAX, GUM_X86_XCX);
    gum_x86_writer_put_call_address_with_arguments (&cw, GUM_CALL_CAPI,
        self->pthread_key_delete, 1,
        GUM_ARG_REGISTER, GUM_X86_XAX);
  }

  gum_x86_writer_put_add_reg_imm (&cw, GUM_X86_XSP, self->module->pointer_size);
  gum_x86_writer_put_pop_reg (&cw, GUM_X86_XBX);
  gum_x86_writer_put_pop_reg (&cw, GUM_X86_XBP);
  gum_x86_writer_put_ret (&cw);

  gum_x86_writer_flush (&cw);
  *size = gum_x86_writer_offset (&cw);
  gum_x86_writer_clear (&cw);
}

static void
gum_emit_child_constructor_call (GumDarwinMapper * child,
                                 GumEmitX86Context * ctx)
{
  GumX86Writer * cw = ctx->cw;

  gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XCX,
      gum_darwin_mapper_constructor (child));
  gum_x86_writer_put_call_reg (cw, GUM_X86_XCX);
}

static void
gum_emit_child_destructor_call (GumDarwinMapper * child,
                                GumEmitX86Context * ctx)
{
  GumX86Writer * cw = ctx->cw;

  gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XCX,
      gum_darwin_mapper_destructor (child));
  gum_x86_writer_put_call_reg (cw, GUM_X86_XCX);
}

static gboolean
gum_emit_chained_fixup_call (const GumDarwinChainedFixupsDetails * details,
                             GumEmitX86Context * ctx)
{
  GumDarwinMapper * mapper = ctx->mapper;
  GumDarwinModule * module = mapper->module;

  gum_x86_writer_put_call_address_with_aligned_arguments (ctx->cw,
      GUM_CALL_CAPI, mapper->process_chained_fixups, 4,
      GUM_ARG_ADDRESS, details->vm_address,
      GUM_ARG_ADDRESS, module->base_address,
      GUM_ARG_ADDRESS, module->preferred_address,
      GUM_ARG_ADDRESS, mapper->chained_symbols_vector);

  return TRUE;
}

static gboolean
gum_emit_resolve_if_needed (const GumDarwinBindDetails * details,
                            GumEmitX86Context * ctx)
{
  GumDarwinMapper * self = ctx->mapper;
  GumX86Writer * cw = ctx->cw;
  GumDarwinMapping * dependency;
  GumDarwinSymbolValue value;
  gboolean success;
  GumAddress entry;

  if (details->type != GUM_DARWIN_BIND_POINTER)
    return TRUE;

  dependency = gum_darwin_mapper_get_dependency_by_ordinal (self,
      details->library_ordinal, NULL);
  if (dependency == NULL)
    return TRUE;
  success = gum_darwin_mapper_resolve_symbol (self, dependency->module,
      details->symbol_name, &value);
  if (!success || value.resolver == 0)
    return TRUE;

  entry = self->module->base_address + details->segment->vm_address +
      details->offset;

  gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XCX, value.resolver);
  gum_x86_writer_put_call_reg (cw, GUM_X86_XCX);
  gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XCX, details->addend);
  gum_x86_writer_put_add_reg_reg (cw, GUM_X86_XAX, GUM_X86_XCX);
  gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XCX, entry);
  gum_x86_writer_put_mov_reg_ptr_reg (cw, GUM_X86_XCX, GUM_X86_XAX);

  return TRUE;
}

static gboolean
gum_emit_init_calls (const GumDarwinInitPointersDetails * details,
                     GumEmitX86Context * ctx)
{
  GumDarwinMapper * self = ctx->mapper;
  GumX86Writer * cw = ctx->cw;
  gconstpointer next_label = GSIZE_TO_POINTER (details->address);

  gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XBP, details->address);
  gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XBX, details->count);

  gum_x86_writer_put_label (cw, next_label);

  gum_x86_writer_put_mov_reg_reg_ptr (cw, GUM_X86_XAX, GUM_X86_XBP);
  gum_x86_writer_put_call_reg_with_aligned_arguments (cw, GUM_CALL_CAPI,
      GUM_X86_XAX, 5,
      /*   argc */ GUM_ARG_ADDRESS, GUM_ADDRESS (0),
      /*   argv */ GUM_ARG_ADDRESS, GUM_ADDRESS (self->empty_strv),
      /*   envp */ GUM_ARG_ADDRESS, GUM_ADDRESS (self->empty_strv),
      /*  apple */ GUM_ARG_ADDRESS, GUM_ADDRESS (self->apple_strv),
      /* result */ GUM_ARG_ADDRESS, GUM_ADDRESS (0));

  gum_x86_writer_put_add_reg_imm (cw, GUM_X86_XBP, self->module->pointer_size);
  gum_x86_writer_put_dec_reg (cw, GUM_X86_XBX);
  gum_x86_writer_put_jcc_short_label (cw, X86_INS_JNE, next_label, GUM_NO_HINT);

  return TRUE;
}

static gboolean
gum_emit_term_calls (const GumDarwinTermPointersDetails * details,
                     GumEmitX86Context * ctx)
{
  GumDarwinMapper * self = ctx->mapper;
  GumX86Writer * cw = ctx->cw;
  gconstpointer next_label = GSIZE_TO_POINTER (details->address);

  gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XBP, details->address +
      ((details->count - 1) * self->module->pointer_size));
  gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XBX, details->count);

  gum_x86_writer_put_label (cw, next_label);

  gum_x86_writer_put_mov_reg_reg_ptr (cw, GUM_X86_XAX, GUM_X86_XBP);
  gum_x86_writer_put_call_reg (cw, GUM_X86_XAX);

  gum_x86_writer_put_sub_reg_imm (cw, GUM_X86_XBP, self->module->pointer_size);
  gum_x86_writer_put_dec_reg (cw, GUM_X86_XBX);
  gum_x86_writer_put_jcc_short_label (cw, X86_INS_JNE, next_label, GUM_NO_HINT);

  return TRUE;
}

static void
gum_emit_tlv_init_code (GumEmitX86Context * ctx)
{
  GumDarwinMapper * self = ctx->mapper;
  GumX86Writer * cw = ctx->cw;
  GumDarwinModule * module = self->module;
  gsize pointer_size = module->pointer_size;
  const GumDarwinTlvParameters * tlv = &self->tlv;
  GumAddress tlv_section = module->base_address + tlv->descriptors_offset;
  gconstpointer next_label = GSIZE_TO_POINTER (cw->code + 1);
  gconstpointer has_key_label = GSIZE_TO_POINTER (cw->code + 2);

  gum_x86_writer_put_call_address_with_arguments (cw, GUM_CALL_CAPI,
      self->pthread_key_create, 2,
      GUM_ARG_ADDRESS, self->pthread_key,
      GUM_ARG_ADDRESS, GUM_ADDRESS (0) /* destructor */);

  gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XAX, self->pthread_key);
  gum_x86_writer_put_mov_reg_reg_ptr (cw, GUM_X86_XAX, GUM_X86_XAX);
  gum_x86_writer_put_shl_reg_u8 (cw, GUM_X86_XAX,
      (pointer_size == 8) ? 3 : 2);
  gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XBX, self->tlv_area);
  gum_x86_writer_put_mov_gs_reg_ptr_reg (cw, GUM_X86_XAX, GUM_X86_XBX);

  gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XCX, tlv->num_descriptors);
  gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XBX, tlv_section);

  gum_x86_writer_put_label (cw, next_label);

  gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XAX, self->tlv_get_addr_addr);
  gum_x86_writer_put_mov_reg_ptr_reg (cw, GUM_X86_XBX, GUM_X86_XAX);

  gum_x86_writer_put_add_reg_imm (cw, GUM_X86_XBX, pointer_size);
  gum_x86_writer_put_mov_reg_reg_ptr (cw, GUM_X86_XAX, GUM_X86_XBX);
  gum_x86_writer_put_cmp_reg_i32 (cw, GUM_X86_XAX, 0);
  gum_x86_writer_put_jcc_short_label (cw, X86_INS_JNE, has_key_label,
      GUM_NO_HINT);

  gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XAX, self->pthread_key);
  gum_x86_writer_put_mov_reg_reg_ptr (cw, GUM_X86_XAX, GUM_X86_XAX);
  gum_x86_writer_put_mov_reg_ptr_reg (cw, GUM_X86_XBX, GUM_X86_XAX);

  gum_x86_writer_put_label (cw, has_key_label);

  gum_x86_writer_put_add_reg_imm (cw, GUM_X86_XBX, 2 * pointer_size);

  gum_x86_writer_put_dec_reg (cw, GUM_X86_XCX);
  gum_x86_writer_put_jcc_short_label (cw, X86_INS_JNE, next_label, GUM_NO_HINT);
}

#elif defined (HAVE_ARM) || defined (HAVE_ARM64)

typedef struct _GumEmitArmContext GumEmitArmContext;
typedef struct _GumEmitArm64Context GumEmitArm64Context;

struct _GumEmitArmContext
{
  GumDarwinMapper * mapper;
  GumThumbWriter * tw;
};

struct _GumEmitArm64Context
{
  GumDarwinMapper * mapper;
  GumArm64Writer * aw;
};

static void gum_emit_arm_runtime (GumDarwinMapper * self,
    gpointer output_buffer, GumAddress pc, gsize * size);
static void gum_emit_arm_child_constructor_call (GumDarwinMapper * child,
    GumEmitArmContext * ctx);
static void gum_emit_arm_child_destructor_call (GumDarwinMapper * child,
    GumEmitArmContext * ctx);
static gboolean gum_emit_arm_resolve_if_needed (
    const GumDarwinBindDetails * details, GumEmitArmContext * ctx);
static gboolean gum_emit_arm_init_calls (
    const GumDarwinInitPointersDetails * details, GumEmitArmContext * ctx);
static gboolean gum_emit_arm_term_calls (
    const GumDarwinTermPointersDetails * details, GumEmitArmContext * ctx);

static void gum_emit_arm64_runtime (GumDarwinMapper * self,
    gpointer output_buffer, GumAddress pc, gsize * size);
static void gum_emit_arm64_child_constructor_call (GumDarwinMapper * child,
    GumEmitArm64Context * ctx);
static void gum_emit_arm64_child_destructor_call (GumDarwinMapper * child,
    GumEmitArm64Context * ctx);
static gboolean gum_emit_arm64_chained_fixup_call (
    const GumDarwinChainedFixupsDetails * details, GumEmitArm64Context * ctx);
static gboolean gum_emit_arm64_resolve_if_needed (
    const GumDarwinBindDetails * details, GumEmitArm64Context * ctx);
static gboolean gum_emit_arm64_init_pointer_calls (
    const GumDarwinInitPointersDetails * details, GumEmitArm64Context * ctx);
static gboolean gum_emit_arm64_init_offset_calls (
    const GumDarwinInitOffsetsDetails * details, GumEmitArm64Context * ctx);
static gboolean gum_emit_arm64_term_calls (
    const GumDarwinTermPointersDetails * details, GumEmitArm64Context * ctx);
static void gum_emit_arm64_tlv_init_code (GumEmitArm64Context * ctx);

static void
gum_emit_runtime (GumDarwinMapper * self,
                  gpointer output_buffer,
                  GumAddress pc,
                  gsize * size)
{
  if (self->module->cpu_type == GUM_CPU_ARM)
    gum_emit_arm_runtime (self, output_buffer, pc, size);
  else
    gum_emit_arm64_runtime (self, output_buffer, pc, size);
}

static void
gum_emit_arm_runtime (GumDarwinMapper * self,
                      gpointer output_buffer,
                      GumAddress pc,
                      gsize * size)
{
  GumDarwinModule * module = self->module;
  const GumDarwinTlvParameters * tlv = &self->tlv;
  GumThumbWriter tw;
  GumEmitArmContext ctx;
  GSList * children_reversed;

  gum_thumb_writer_init (&tw, output_buffer);
  tw.pc = pc;

  ctx.mapper = self;
  ctx.tw = &tw;

  if (self->parent == NULL)
  {
    /* atexit stub */
    gum_thumb_writer_put_ldr_reg_u32 (&tw, ARM_REG_R0, 0);
    gum_thumb_writer_put_bx_reg (&tw, ARM_REG_LR);
  }

  self->constructor_offset = gum_thumb_writer_offset (&tw) + 1;
  gum_thumb_writer_put_push_regs (&tw, 5, ARM_REG_R4, ARM_REG_R5, ARM_REG_R6,
      ARM_REG_R7, ARM_REG_LR);

  g_slist_foreach (self->children, (GFunc) gum_emit_arm_child_constructor_call,
      &ctx);
  gum_darwin_module_enumerate_binds (module,
      (GumFoundDarwinBindFunc) gum_emit_arm_resolve_if_needed, &ctx);
  gum_darwin_module_enumerate_lazy_binds (module,
      (GumFoundDarwinBindFunc) gum_emit_arm_resolve_if_needed, &ctx);
  gum_darwin_module_enumerate_init_pointers (module,
      (GumFoundDarwinInitPointersFunc) gum_emit_arm_init_calls, &ctx);

  if (tlv->num_descriptors != 0)
  {
    gum_thumb_writer_put_call_address_with_arguments (&tw,
        self->pthread_key_create, 2,
        GUM_ARG_ADDRESS, self->pthread_key,
        GUM_ARG_ADDRESS, GUM_ADDRESS (0) /* destructor */);
  }

  gum_thumb_writer_put_pop_regs (&tw, 5, ARM_REG_R4, ARM_REG_R5, ARM_REG_R6,
      ARM_REG_R7, ARM_REG_PC);

  self->destructor_offset = gum_thumb_writer_offset (&tw) + 1;
  gum_thumb_writer_put_push_regs (&tw, 5, ARM_REG_R4, ARM_REG_R5, ARM_REG_R6,
      ARM_REG_R7, ARM_REG_LR);

  gum_darwin_module_enumerate_term_pointers (module,
      (GumFoundDarwinTermPointersFunc) gum_emit_arm_term_calls, &ctx);
  children_reversed = g_slist_reverse (g_slist_copy (self->children));
  g_slist_foreach (children_reversed,
      (GFunc) gum_emit_arm_child_destructor_call, &ctx);
  g_slist_free (children_reversed);

  if (tlv->num_descriptors != 0)
  {
    gum_thumb_writer_put_ldr_reg_address (&tw, ARM_REG_R4, self->pthread_key);
    gum_thumb_writer_put_ldr_reg_reg (&tw, ARM_REG_R4, ARM_REG_R4);

    gum_thumb_writer_put_call_address_with_arguments (&tw,
        self->pthread_key_delete, 1,
        GUM_ARG_REGISTER, ARM_REG_R4);
  }

  gum_thumb_writer_put_pop_regs (&tw, 5, ARM_REG_R4, ARM_REG_R5, ARM_REG_R6,
      ARM_REG_R7, ARM_REG_PC);

  gum_thumb_writer_flush (&tw);
  *size = gum_thumb_writer_offset (&tw);
  gum_thumb_writer_clear (&tw);
}

static void
gum_emit_arm_child_constructor_call (GumDarwinMapper * child,
                                     GumEmitArmContext * ctx)
{
  GumThumbWriter * tw = ctx->tw;

  gum_thumb_writer_put_ldr_reg_address (tw, ARM_REG_R0,
      gum_darwin_mapper_constructor (child));
  gum_thumb_writer_put_blx_reg (tw, ARM_REG_R0);
}

static void
gum_emit_arm_child_destructor_call (GumDarwinMapper * child,
                                    GumEmitArmContext * ctx)
{
  GumThumbWriter * tw = ctx->tw;

  gum_thumb_writer_put_ldr_reg_address (tw, ARM_REG_R0,
      gum_darwin_mapper_destructor (child));
  gum_thumb_writer_put_blx_reg (tw, ARM_REG_R0);
}

static gboolean
gum_emit_arm_resolve_if_needed (const GumDarwinBindDetails * details,
                                GumEmitArmContext * ctx)
{
  GumDarwinMapper * self = ctx->mapper;
  GumThumbWriter * tw = ctx->tw;
  GumDarwinMapping * dependency;
  GumDarwinSymbolValue value;
  gboolean success;
  GumAddress entry;

  if (details->type != GUM_DARWIN_BIND_POINTER)
    return TRUE;

  dependency = gum_darwin_mapper_get_dependency_by_ordinal (self,
      details->library_ordinal, NULL);
  if (dependency == NULL)
    return TRUE;
  success = gum_darwin_mapper_resolve_symbol (self, dependency->module,
      details->symbol_name, &value);
  if (!success || value.resolver == 0)
    return TRUE;

  entry = self->module->base_address + details->segment->vm_address +
      details->offset;

  gum_thumb_writer_put_ldr_reg_address (tw, ARM_REG_R1, value.resolver);
  gum_thumb_writer_put_blx_reg (tw, ARM_REG_R1);
  gum_thumb_writer_put_ldr_reg_address (tw, ARM_REG_R1, details->addend);
  gum_thumb_writer_put_add_reg_reg_reg (tw, ARM_REG_R0, ARM_REG_R0, ARM_REG_R1);
  gum_thumb_writer_put_ldr_reg_address (tw, ARM_REG_R1, entry);
  gum_thumb_writer_put_str_reg_reg_offset (tw, ARM_REG_R0, ARM_REG_R1, 0);

  return TRUE;
}

static gboolean
gum_emit_arm_init_calls (const GumDarwinInitPointersDetails * details,
                         GumEmitArmContext * ctx)
{
  GumDarwinMapper * self = ctx->mapper;
  GumThumbWriter * tw = ctx->tw;
  gconstpointer next_label = GSIZE_TO_POINTER (details->address);

  gum_thumb_writer_put_ldr_reg_address (tw, ARM_REG_R4, details->address);
  gum_thumb_writer_put_ldr_reg_address (tw, ARM_REG_R5, details->count);

  gum_thumb_writer_put_label (tw, next_label);

  gum_thumb_writer_put_ldr_reg_reg (tw, ARM_REG_R6, ARM_REG_R4);
  gum_thumb_writer_put_call_reg_with_arguments (tw, ARM_REG_R6, 5,
      /*   argc */ GUM_ARG_ADDRESS, GUM_ADDRESS (0),
      /*   argv */ GUM_ARG_ADDRESS, GUM_ADDRESS (self->empty_strv),
      /*   envp */ GUM_ARG_ADDRESS, GUM_ADDRESS (self->empty_strv),
      /*  apple */ GUM_ARG_ADDRESS, GUM_ADDRESS (self->apple_strv),
      /* result */ GUM_ARG_ADDRESS, GUM_ADDRESS (0));

  gum_thumb_writer_put_add_reg_reg_imm (tw, ARM_REG_R4, ARM_REG_R4, 4);
  gum_thumb_writer_put_sub_reg_reg_imm (tw, ARM_REG_R5, ARM_REG_R5, 1);
  gum_thumb_writer_put_cmp_reg_imm (tw, ARM_REG_R5, 0);
  gum_thumb_writer_put_bne_label (tw, next_label);

  return TRUE;
}

static gboolean
gum_emit_arm_term_calls (const GumDarwinTermPointersDetails * details,
                         GumEmitArmContext * ctx)
{
  GumThumbWriter * tw = ctx->tw;
  gconstpointer next_label = GSIZE_TO_POINTER (details->address);

  gum_thumb_writer_put_ldr_reg_address (tw, ARM_REG_R4, details->address +
      ((details->count - 1) * 4));
  gum_thumb_writer_put_ldr_reg_address (tw, ARM_REG_R5, details->count);

  gum_thumb_writer_put_label (tw, next_label);

  gum_thumb_writer_put_ldr_reg_reg (tw, ARM_REG_R0, ARM_REG_R4);
  gum_thumb_writer_put_blx_reg (tw, ARM_REG_R0);

  gum_thumb_writer_put_sub_reg_reg_imm (tw, ARM_REG_R4, ARM_REG_R4, 4);
  gum_thumb_writer_put_sub_reg_reg_imm (tw, ARM_REG_R5, ARM_REG_R5, 1);
  gum_thumb_writer_put_cmp_reg_imm (tw, ARM_REG_R5, 0);
  gum_thumb_writer_put_bne_label (tw, next_label);

  return TRUE;
}

static void
gum_emit_arm64_runtime (GumDarwinMapper * self,
                        gpointer output_buffer,
                        GumAddress pc,
                        gsize * size)
{
  GumDarwinModule * module = self->module;
  const GumDarwinTlvParameters * tlv = &self->tlv;
  GumArm64Writer aw;
  GumEmitArm64Context ctx;
  GumAddress process_threaded_items, threaded_symbols, threaded_regions;
  GSList * children_reversed;

  gum_arm64_writer_init (&aw, output_buffer);
  aw.pc = pc;

  ctx.mapper = self;
  ctx.aw = &aw;

  if (self->parent == NULL)
  {
    /* atexit stub */
    gum_arm64_writer_put_ldr_reg_u64 (&aw, ARM64_REG_X0, 0);
    gum_arm64_writer_put_ret (&aw);
  }

  if (self->threaded_regions != NULL)
  {
    process_threaded_items = aw.pc;
    gum_arm64_writer_put_bytes (&aw,
        (const guint8 *) gum_threaded_bind_processor_code,
        sizeof (gum_threaded_bind_processor_code));

    threaded_symbols = aw.pc;
    gum_arm64_writer_put_bytes (&aw,
        (const guint8 *) self->threaded_symbols->data,
        self->threaded_symbols->len * sizeof (GumAddress));

    threaded_regions = aw.pc;
    gum_arm64_writer_put_bytes (&aw,
        (const guint8 *) self->threaded_regions->data,
        self->threaded_regions->len * sizeof (GumAddress));
  }

  self->constructor_offset = gum_arm64_writer_offset (&aw);
  gum_arm64_writer_put_push_reg_reg (&aw, ARM64_REG_FP, ARM64_REG_LR);
  gum_arm64_writer_put_mov_reg_reg (&aw, ARM64_REG_FP, ARM64_REG_SP);
  gum_arm64_writer_put_push_reg_reg (&aw, ARM64_REG_X19, ARM64_REG_X20);
  gum_arm64_writer_put_push_reg_reg (&aw, ARM64_REG_X21, ARM64_REG_X22);

  g_slist_foreach (self->children,
      (GFunc) gum_emit_arm64_child_constructor_call, &ctx);
  if (self->threaded_regions != NULL)
  {
    gum_arm64_writer_put_call_address_with_arguments (&aw,
        process_threaded_items,
        6,
        GUM_ARG_ADDRESS, module->preferred_address,
        GUM_ARG_ADDRESS, gum_darwin_module_get_slide (module),
        GUM_ARG_ADDRESS, (GumAddress) self->threaded_symbols->len,
        GUM_ARG_ADDRESS, threaded_symbols,
        GUM_ARG_ADDRESS, (GumAddress) self->threaded_regions->len,
        GUM_ARG_ADDRESS, threaded_regions);
  }
  gum_darwin_module_enumerate_chained_fixups (module,
      (GumFoundDarwinChainedFixupsFunc) gum_emit_arm64_chained_fixup_call,
      &ctx);
  gum_darwin_module_enumerate_binds (module,
      (GumFoundDarwinBindFunc) gum_emit_arm64_resolve_if_needed, &ctx);
  gum_darwin_module_enumerate_lazy_binds (module,
      (GumFoundDarwinBindFunc) gum_emit_arm64_resolve_if_needed, &ctx);

  if (tlv->num_descriptors != 0)
    gum_emit_arm64_tlv_init_code (&ctx);

  gum_darwin_module_enumerate_init_pointers (module,
      (GumFoundDarwinInitPointersFunc) gum_emit_arm64_init_pointer_calls, &ctx);
  gum_darwin_module_enumerate_init_offsets (module,
      (GumFoundDarwinInitOffsetsFunc) gum_emit_arm64_init_offset_calls, &ctx);

  gum_arm64_writer_put_pop_reg_reg (&aw, ARM64_REG_X21, ARM64_REG_X22);
  gum_arm64_writer_put_pop_reg_reg (&aw, ARM64_REG_X19, ARM64_REG_X20);
  gum_arm64_writer_put_pop_reg_reg (&aw, ARM64_REG_FP, ARM64_REG_LR);
  gum_arm64_writer_put_ret (&aw);

  self->destructor_offset = gum_arm64_writer_offset (&aw);
  gum_arm64_writer_put_push_reg_reg (&aw, ARM64_REG_FP, ARM64_REG_LR);
  gum_arm64_writer_put_mov_reg_reg (&aw, ARM64_REG_FP, ARM64_REG_SP);
  gum_arm64_writer_put_push_reg_reg (&aw, ARM64_REG_X19, ARM64_REG_X20);
  gum_arm64_writer_put_push_reg_reg (&aw, ARM64_REG_X21, ARM64_REG_X22);

  gum_darwin_module_enumerate_term_pointers (module,
      (GumFoundDarwinTermPointersFunc) gum_emit_arm64_term_calls, &ctx);
  children_reversed = g_slist_reverse (g_slist_copy (self->children));
  g_slist_foreach (children_reversed,
      (GFunc) gum_emit_arm64_child_destructor_call, &ctx);
  g_slist_free (children_reversed);

  if (tlv->num_descriptors != 0)
  {
    gum_arm64_writer_put_ldr_reg_address (&aw, ARM64_REG_X21,
        self->pthread_key);
    gum_arm64_writer_put_ldr_reg_reg (&aw, ARM64_REG_X21, ARM64_REG_X21);

    gum_arm64_writer_put_call_address_with_arguments (&aw,
        self->pthread_key_delete, 1,
        GUM_ARG_REGISTER, ARM64_REG_X21);
  }

  gum_arm64_writer_put_pop_reg_reg (&aw, ARM64_REG_X21, ARM64_REG_X22);
  gum_arm64_writer_put_pop_reg_reg (&aw, ARM64_REG_X19, ARM64_REG_X20);
  gum_arm64_writer_put_pop_reg_reg (&aw, ARM64_REG_FP, ARM64_REG_LR);
  gum_arm64_writer_put_ret (&aw);

  gum_arm64_writer_flush (&aw);
  *size = gum_arm64_writer_offset (&aw);
  gum_arm64_writer_clear (&aw);
}

static void
gum_emit_arm64_child_constructor_call (GumDarwinMapper * child,
                                       GumEmitArm64Context * ctx)
{
  GumArm64Writer * aw = ctx->aw;

  gum_arm64_writer_put_ldr_reg_address (aw, ARM64_REG_X0,
      gum_darwin_mapper_constructor (child));
  gum_arm64_writer_put_blr_reg (aw, ARM64_REG_X0);
}

static void
gum_emit_arm64_child_destructor_call (GumDarwinMapper * child,
                                      GumEmitArm64Context * ctx)
{
  GumArm64Writer * aw = ctx->aw;

  gum_arm64_writer_put_ldr_reg_address (aw, ARM64_REG_X0,
      gum_darwin_mapper_destructor (child));
  gum_arm64_writer_put_blr_reg (aw, ARM64_REG_X0);
}

static gboolean
gum_emit_arm64_chained_fixup_call (
    const GumDarwinChainedFixupsDetails * details,
    GumEmitArm64Context * ctx)
{
  GumDarwinMapper * mapper = ctx->mapper;
  GumDarwinModule * module = mapper->module;

  gum_arm64_writer_put_call_address_with_arguments (ctx->aw,
      mapper->process_chained_fixups, 4,
      GUM_ARG_ADDRESS, details->vm_address,
      GUM_ARG_ADDRESS, module->base_address,
      GUM_ARG_ADDRESS, module->preferred_address,
      GUM_ARG_ADDRESS, mapper->chained_symbols_vector);

  return TRUE;
}

static gboolean
gum_emit_arm64_resolve_if_needed (const GumDarwinBindDetails * details,
                                  GumEmitArm64Context * ctx)
{
  GumDarwinMapper * self = ctx->mapper;
  GumArm64Writer * aw = ctx->aw;
  gboolean bind_target_stored_by_chain_processor;
  GumDarwinMapping * dependency;
  GumDarwinSymbolValue value;
  gboolean success;
  GumAddress entry;

  if (details->type != GUM_DARWIN_BIND_POINTER)
    return TRUE;

  bind_target_stored_by_chain_processor = self->threaded_symbols != NULL;
  if (bind_target_stored_by_chain_processor)
    return TRUE;

  dependency = gum_darwin_mapper_get_dependency_by_ordinal (self,
      details->library_ordinal, NULL);
  if (dependency == NULL)
    return TRUE;
  success = gum_darwin_mapper_resolve_symbol (self, dependency->module,
      details->symbol_name, &value);
  if (!success || value.resolver == 0)
    return TRUE;

  entry = self->module->base_address + details->segment->vm_address +
      details->offset;

  gum_arm64_writer_put_ldr_reg_address (aw, ARM64_REG_X1, value.resolver);
  gum_arm64_writer_put_blr_reg_no_auth (aw, ARM64_REG_X1);
  gum_arm64_writer_put_ldr_reg_address (aw, ARM64_REG_X1, details->addend);
  gum_arm64_writer_put_add_reg_reg_reg (aw, ARM64_REG_X0, ARM64_REG_X0,
      ARM64_REG_X1);
  gum_arm64_writer_put_ldr_reg_address (aw, ARM64_REG_X1, entry);
  gum_arm64_writer_put_str_reg_reg_offset (aw, ARM64_REG_X0, ARM64_REG_X1, 0);

  return TRUE;
}

static gboolean
gum_emit_arm64_init_pointer_calls (const GumDarwinInitPointersDetails * details,
                                   GumEmitArm64Context * ctx)
{
  GumDarwinMapper * self = ctx->mapper;
  GumArm64Writer * aw = ctx->aw;
  gconstpointer next_label = GSIZE_TO_POINTER (details->address);

  gum_arm64_writer_put_ldr_reg_address (aw, ARM64_REG_X19, details->address);
  gum_arm64_writer_put_ldr_reg_address (aw, ARM64_REG_X20, details->count);
  gum_arm64_writer_put_ldr_reg_address (aw, ARM64_REG_X21, self->empty_strv);
  gum_arm64_writer_put_ldr_reg_address (aw, ARM64_REG_X22, self->apple_strv);

  gum_arm64_writer_put_label (aw, next_label);

  /* init (argc, argv, envp, apple, result) */
  gum_arm64_writer_put_mov_reg_reg (aw, ARM64_REG_X0, ARM64_REG_XZR);
  gum_arm64_writer_put_mov_reg_reg (aw, ARM64_REG_X1, ARM64_REG_X21);
  gum_arm64_writer_put_mov_reg_reg (aw, ARM64_REG_X2, ARM64_REG_X21);
  gum_arm64_writer_put_mov_reg_reg (aw, ARM64_REG_X3, ARM64_REG_X22);
  gum_arm64_writer_put_mov_reg_reg (aw, ARM64_REG_X4, ARM64_REG_XZR);
  gum_arm64_writer_put_ldr_reg_reg_offset (aw, ARM64_REG_X5, ARM64_REG_X19, 0);
  gum_arm64_writer_put_blr_reg_no_auth (aw, ARM64_REG_X5);

  gum_arm64_writer_put_add_reg_reg_imm (aw, ARM64_REG_X19, ARM64_REG_X19, 8);
  gum_arm64_writer_put_sub_reg_reg_imm (aw, ARM64_REG_X20, ARM64_REG_X20, 1);
  gum_arm64_writer_put_cbnz_reg_label (aw, ARM64_REG_X20, next_label);

  return TRUE;
}

static gboolean
gum_emit_arm64_init_offset_calls (const GumDarwinInitOffsetsDetails * details,
                                  GumEmitArm64Context * ctx)
{
  GumDarwinMapper * self = ctx->mapper;
  GumArm64Writer * aw = ctx->aw;
  gconstpointer next_label = GSIZE_TO_POINTER (details->address);

  gum_arm64_writer_put_ldr_reg_address (aw, ARM64_REG_X19, details->address);
  gum_arm64_writer_put_ldr_reg_address (aw, ARM64_REG_X20, details->count);
  gum_arm64_writer_put_ldr_reg_address (aw, ARM64_REG_X21, self->empty_strv);
  gum_arm64_writer_put_ldr_reg_address (aw, ARM64_REG_X22, self->apple_strv);

  gum_arm64_writer_put_label (aw, next_label);

  /* init (argc, argv, envp, apple, result) */
  gum_arm64_writer_put_mov_reg_reg (aw, ARM64_REG_X0, ARM64_REG_XZR);
  gum_arm64_writer_put_mov_reg_reg (aw, ARM64_REG_X1, ARM64_REG_X21);
  gum_arm64_writer_put_mov_reg_reg (aw, ARM64_REG_X2, ARM64_REG_X21);
  gum_arm64_writer_put_mov_reg_reg (aw, ARM64_REG_X3, ARM64_REG_X22);
  gum_arm64_writer_put_mov_reg_reg (aw, ARM64_REG_X4, ARM64_REG_XZR);
  gum_arm64_writer_put_ldr_reg_address (aw, ARM64_REG_X5,
      self->module->base_address);
  gum_arm64_writer_put_ldr_reg_reg_offset (aw, ARM64_REG_W6, ARM64_REG_X19, 0);
  gum_arm64_writer_put_add_reg_reg_reg (aw, ARM64_REG_X5, ARM64_REG_X5,
      ARM64_REG_X6);
  gum_arm64_writer_put_blr_reg_no_auth (aw, ARM64_REG_X5);

  gum_arm64_writer_put_add_reg_reg_imm (aw, ARM64_REG_X19, ARM64_REG_X19, 4);
  gum_arm64_writer_put_sub_reg_reg_imm (aw, ARM64_REG_X20, ARM64_REG_X20, 1);
  gum_arm64_writer_put_cbnz_reg_label (aw, ARM64_REG_X20, next_label);

  return TRUE;
}

static gboolean
gum_emit_arm64_term_calls (const GumDarwinTermPointersDetails * details,
                           GumEmitArm64Context * ctx)
{
  GumArm64Writer * aw = ctx->aw;
  gconstpointer next_label = GSIZE_TO_POINTER (details->address);

  gum_arm64_writer_put_ldr_reg_address (aw, ARM64_REG_X19, details->address +
      ((details->count - 1) * 8));
  gum_arm64_writer_put_ldr_reg_address (aw, ARM64_REG_X20, details->count);

  gum_arm64_writer_put_label (aw, next_label);

  gum_arm64_writer_put_ldr_reg_reg_offset (aw, ARM64_REG_X0,
      ARM64_REG_X19, 0);
  gum_arm64_writer_put_blr_reg_no_auth (aw, ARM64_REG_X0);

  gum_arm64_writer_put_sub_reg_reg_imm (aw, ARM64_REG_X19, ARM64_REG_X19, 8);
  gum_arm64_writer_put_sub_reg_reg_imm (aw, ARM64_REG_X20, ARM64_REG_X20, 1);
  gum_arm64_writer_put_cbnz_reg_label (aw, ARM64_REG_X20, next_label);

  return TRUE;
}

static void
gum_emit_arm64_tlv_init_code (GumEmitArm64Context * ctx)
{
  GumDarwinMapper * self = ctx->mapper;
  GumDarwinModule * module = self->module;
  gsize pointer_size = module->pointer_size;
  const GumDarwinTlvParameters * tlv = &self->tlv;
  GumAddress tlv_section = module->base_address + tlv->descriptors_offset;
  GumArm64Writer * aw = ctx->aw;
  gconstpointer next_label = GSIZE_TO_POINTER (aw->code + 1);
  gconstpointer has_key_label = GSIZE_TO_POINTER (aw->code + 2);

  gum_arm64_writer_put_call_address_with_arguments (aw,
      self->pthread_key_create, 2,
      GUM_ARG_ADDRESS, self->pthread_key,
      GUM_ARG_ADDRESS, GUM_ADDRESS (0) /* destructor */);

  gum_arm64_writer_put_ldr_reg_address (aw, ARM64_REG_X19, self->pthread_key);
  gum_arm64_writer_put_ldr_reg_reg (aw, ARM64_REG_X19, ARM64_REG_X19);
  gum_arm64_writer_put_lsl_reg_imm (aw, ARM64_REG_X19, ARM64_REG_X19,
      (pointer_size == 8) ? 3 : 2);
  gum_arm64_writer_put_mrs (aw, ARM64_REG_X20, GUM_ARM64_SYSREG_TPIDRRO_EL0);
  gum_arm64_writer_put_and_reg_reg_imm (aw, ARM64_REG_X20, ARM64_REG_X20,
      (guint64) -8);
  gum_arm64_writer_put_add_reg_reg_reg (aw, ARM64_REG_X19, ARM64_REG_X19,
      ARM64_REG_X20);
  gum_arm64_writer_put_ldr_reg_address (aw, ARM64_REG_X20, self->tlv_area);
  gum_arm64_writer_put_str_reg_reg (aw, ARM64_REG_X20, ARM64_REG_X19);

  gum_arm64_writer_put_ldr_reg_u64 (aw, ARM64_REG_X20, tlv_section);
  gum_arm64_writer_put_ldr_reg_address (aw, ARM64_REG_X21,
      tlv->num_descriptors);

  gum_arm64_writer_put_label (aw, next_label);

  gum_arm64_writer_put_ldr_reg_u64 (aw, ARM64_REG_X19,
      gum_strip_code_address (self->tlv_get_addr_addr));
  gum_arm64_writer_put_str_reg_reg (aw, ARM64_REG_X19, ARM64_REG_X20);
  gum_arm64_writer_put_add_reg_reg_imm (aw, ARM64_REG_X20, ARM64_REG_X20,
      pointer_size);

  gum_arm64_writer_put_ldr_reg_reg (aw, ARM64_REG_X19, ARM64_REG_X20);
  gum_arm64_writer_put_cbnz_reg_label (aw, ARM64_REG_X19, has_key_label);

  gum_arm64_writer_put_ldr_reg_address (aw, ARM64_REG_X19, self->pthread_key);
  gum_arm64_writer_put_ldr_reg_reg (aw, ARM64_REG_X19, ARM64_REG_X19);
  gum_arm64_writer_put_str_reg_reg (aw, ARM64_REG_X19, ARM64_REG_X20);

  gum_arm64_writer_put_label (aw, has_key_label);
  gum_arm64_writer_put_add_reg_reg_imm (aw, ARM64_REG_X20, ARM64_REG_X20,
      2 * pointer_size);

  gum_arm64_writer_put_sub_reg_reg_imm (aw, ARM64_REG_X21, ARM64_REG_X21, 1);
  gum_arm64_writer_put_cbnz_reg_label (aw, ARM64_REG_X21, next_label);
}

#endif

static gboolean
gum_accumulate_chained_fixups_size (
    const GumDarwinChainedFixupsDetails * details,
    gpointer user_data)
{
  GumAccumulateFootprintContext * ctx = user_data;
  GumDarwinMapper * self = ctx->mapper;
  gsize pointer_size = self->module->pointer_size;
  const GumChainedFixupsHeader * fixups_header;

  fixups_header = gum_darwin_mapper_data_from_offset (self,
      details->file_offset, pointer_size);
  if (fixups_header == NULL)
    return TRUE;

  ctx->chained_fixups_count++;
  ctx->chained_imports_count += fixups_header->imports_count;

  ctx->total += GUM_MAPPER_CHAINED_FIXUP_CALL_SIZE;

  return TRUE;
}

static gboolean
gum_accumulate_bind_footprint_size (const GumDarwinBindDetails * details,
                                    gpointer user_data)
{
  GumAccumulateFootprintContext * ctx = user_data;

  switch (details->type)
  {
    case GUM_DARWIN_BIND_POINTER:
      gum_accumulate_bind_pointer_footprint_size (ctx, details);
      break;
    case GUM_DARWIN_BIND_THREADED_TABLE:
      gum_accumulate_bind_threaded_table_footprint_size (ctx, details);
      break;
    case GUM_DARWIN_BIND_THREADED_ITEMS:
      gum_accumulate_bind_threaded_items_footprint_size (ctx, details);
      break;
    default:
      break;
  }

  return TRUE;
}

static void
gum_accumulate_bind_pointer_footprint_size (
    GumAccumulateFootprintContext * ctx,
    const GumDarwinBindDetails * details)
{
  GumDarwinMapper * self = ctx->mapper;
  GumDarwinMapping * dependency;
  GumDarwinSymbolValue value;

  dependency = gum_darwin_mapper_get_dependency_by_ordinal (self,
      details->library_ordinal, NULL);
  if (dependency == NULL)
    return;

  if (gum_darwin_mapper_resolve_symbol (self, dependency->module,
      details->symbol_name, &value))
  {
    if (value.resolver != 0)
      ctx->total += GUM_MAPPER_RESOLVER_SIZE;
  }
}

static void
gum_accumulate_bind_threaded_table_footprint_size (
    GumAccumulateFootprintContext * ctx,
    const GumDarwinBindDetails * details)
{
#if defined (HAVE_ARM) || defined (HAVE_ARM64)
  ctx->total += sizeof (gum_threaded_bind_processor_code);
  ctx->total += details->threaded_table_size * sizeof (GumAddress);
#endif
}

static void
gum_accumulate_bind_threaded_items_footprint_size (
    GumAccumulateFootprintContext * ctx,
    const GumDarwinBindDetails * details)
{
  ctx->threaded_regions_count++;

  ctx->total += sizeof (GumAddress);
}

static gboolean
gum_accumulate_init_pointers_footprint_size (
    const GumDarwinInitPointersDetails * details,
    gpointer user_data)
{
  GumAccumulateFootprintContext * ctx = user_data;

  ctx->total += GUM_MAPPER_INIT_SIZE;

  return TRUE;
}

static gboolean
gum_accumulate_init_offsets_footprint_size (
    const GumDarwinInitOffsetsDetails * details,
    gpointer user_data)
{
  GumAccumulateFootprintContext * ctx = user_data;

  ctx->total += GUM_MAPPER_INIT_SIZE;

  return TRUE;
}

static gboolean
gum_accumulate_term_footprint_size (
    const GumDarwinTermPointersDetails * details,
    gpointer user_data)
{
  GumAccumulateFootprintContext * ctx = user_data;

  ctx->total += GUM_MAPPER_TERM_SIZE;

  return TRUE;
}

static gpointer
gum_darwin_mapper_data_from_offset (GumDarwinMapper * self,
                                    guint64 offset,
                                    guint size)
{
  GumDarwinModuleImage * image = self->image;
  guint64 source_offset = image->source_offset;

  if (source_offset != 0)
  {
    if (offset < source_offset)
      return NULL;
    if (offset + size > source_offset + image->shared_offset +
        image->shared_size)
      return NULL;
  }
  else
  {
    if (offset + size > image->size)
      return NULL;
  }

  return image->data + (offset - source_offset);
}

static GumDarwinMapping *
gum_darwin_mapper_get_dependency_by_ordinal (GumDarwinMapper * self,
                                             gint ordinal,
                                             GError ** error)
{
  GumDarwinMapping * result;

  switch (ordinal)
  {
    case GUM_DARWIN_BIND_SELF:
      result = gum_darwin_mapper_get_dependency_by_name (self,
          self->module->name, error);
      break;
    case GUM_DARWIN_BIND_MAIN_EXECUTABLE:
    case GUM_DARWIN_BIND_FLAT_LOOKUP:
    case GUM_DARWIN_BIND_WEAK_LOOKUP:
      goto invalid_ordinal;
    default:
    {
      gint i = ordinal - 1;

      if (i >= 0 && i < self->dependencies->len)
        result = g_ptr_array_index (self->dependencies, i);
      else
        goto invalid_ordinal;

      break;
    }
  }

  return result;

invalid_ordinal:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_DATA,
        "Malformed dependency ordinal");
    return NULL;
  }
}

static GumDarwinMapping *
gum_darwin_mapper_get_dependency_by_name (GumDarwinMapper * self,
                                          const gchar * name,
                                          GError ** error)
{
  GumDarwinModuleResolver * resolver = self->resolver;
  GumDarwinMapping * mapping;

  if (self->parent != NULL)
    return gum_darwin_mapper_get_dependency_by_name (self->parent, name, error);

  mapping = g_hash_table_lookup (self->mappings, name);

  if (mapping == NULL)
  {
    GumDarwinModule * module =
        gum_darwin_module_resolver_find_module_by_name (resolver, name);
    if (module != NULL)
    {
      mapping = gum_darwin_mapper_add_existing_mapping (self, module);
      g_object_unref (module);
    }
  }

  if (mapping == NULL)
  {
    gchar * full_name;
    GumDarwinMapper * mapper;

    if (resolver->sysroot != NULL)
      full_name = g_strconcat (resolver->sysroot, name, NULL);
    else
      full_name = g_strdup (name);

    mapper = gum_darwin_mapper_new_from_file_with_parent (self, full_name,
        self->resolver, error);
    if (mapper != NULL)
    {
      mapping = g_hash_table_lookup (self->mappings, full_name);
      g_assert (mapping != NULL);

      if (resolver->sysroot != NULL)
        gum_darwin_mapper_add_alias_mapping (self, name, mapping);
    }

    g_free (full_name);
  }

  return mapping;
}

static gboolean
gum_darwin_mapper_resolve_import (GumDarwinMapper * self,
                                  gint library_ordinal,
                                  const gchar * symbol_name,
                                  gboolean is_weak,
                                  GumDarwinSymbolValue * value,
                                  GError ** error)
{
  gboolean success;
  GumDarwinMapping * dependency;

  if (library_ordinal == GUM_DARWIN_BIND_FLAT_LOOKUP)
  {
    dependency = NULL;

    value->address = gum_strip_code_address (
        gum_darwin_module_resolver_find_dynamic_address (self->resolver,
          gum_symbol_name_from_darwin (symbol_name)));
    value->resolver = 0;

    success = value->address != 0;
  }
  else
  {
    dependency = gum_darwin_mapper_get_dependency_by_ordinal (self,
        library_ordinal, error);
    if (dependency == NULL)
      goto module_not_found;

    success = gum_darwin_mapper_resolve_symbol (self, dependency->module,
        symbol_name, value);
    if (!success && !is_weak && self->resolver->sysroot != NULL &&
        g_str_has_suffix (symbol_name, "$INODE64"))
    {
      gchar * plain_name;

      plain_name = g_strndup (symbol_name, strlen (symbol_name) - 8);
      success = gum_darwin_mapper_resolve_symbol (self, dependency->module,
          plain_name, value);
      g_free (plain_name);
    }
  }

  if (!success && !is_weak)
    goto symbol_not_found;

  return TRUE;

module_not_found:
  {
    return FALSE;
  }
symbol_not_found:
  {
    if (dependency != NULL)
    {
      g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_FOUND,
          "Unable to bind, “%s” not found in “%s”",
          gum_symbol_name_from_darwin (symbol_name),
          dependency->module->name);
    }
    else
    {
      g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_FOUND,
          "Unable to bind, “%s” cannot be resolved through flat lookup",
          gum_symbol_name_from_darwin (symbol_name));
    }
    return FALSE;
  }
}

static gboolean
gum_darwin_mapper_resolve_symbol (GumDarwinMapper * self,
                                  GumDarwinModule * module,
                                  const gchar * name,
                                  GumDarwinSymbolValue * value)
{
  GumDarwinExportDetails details;

  if (self->parent != NULL)
  {
    return gum_darwin_mapper_resolve_symbol (self->parent, module, name, value);
  }

  if (strcmp (name, "_atexit") == 0 ||
      strcmp (name, "_atexit_b") == 0 ||
      strcmp (name, "___cxa_atexit") == 0 ||
      strcmp (name, "___cxa_thread_atexit") == 0 ||
      strcmp (name, "__tlv_atexit") == 0)
  {
    /*
     * We pretend we install the handler by resolving to a dummy function that
     * does nothing. Memory for handlers isn't released, so we shouldn't let
     * our libraries register them. In our case atexit is only for debugging
     * purposes anyway (GLib installs a handler to print statistics when
     * debugging is enabled).
     */
    if (self->runtime_address != 0)
    {
      value->address = self->runtime_address + self->runtime_header_size;
      if (self->module->cpu_type == GUM_CPU_ARM)
        value->address |= 1;
    }
    else
    {
      /* Resolving before mapped; we will handle it later. */
      value->address = 0xdeadbeef;
    }
    value->resolver = 0;
    return TRUE;
  }

  if (GUM_MEMORY_RANGE_INCLUDES (&self->shared_cache_range,
        module->base_address))
  {
    GumDarwinModule * local_module;
    GumExportDetails d;

    local_module = gum_darwin_module_resolver_find_module_by_name (
        self->local_resolver, module->name);
    if (local_module != NULL &&
        gum_darwin_module_resolver_find_export_by_mangled_name (
          self->local_resolver, local_module, name, &d))
    {
      value->address = d.address;
    }
    else
    {
      value->address = 0;
    }
    g_clear_object (&local_module);

#ifdef HAVE_ARM64
    if (value->address != 0)
    {
      /*
       * XXX: Symbols with a resolver, such as strcmp() on macOS Sequoia, have
       *      an invalid signature. Asking the CPU to strip the ptrauth bits
       *      in such a case thus results in more junk being added.
       */
      if (value->address >> 47 == 0x100)
        value->address &= 0x7fffffffffffff;
      else
        value->address = gum_strip_code_address (value->address);
    }
#endif

    if (value->address != 0 &&
        !GUM_MEMORY_RANGE_INCLUDES (&self->shared_cache_range, value->address))
    {
      value->address = 0;
    }

    value->resolver = 0;

    if (value->address != 0)
      return TRUE;
  }

  if (!gum_darwin_module_resolve_export (module, name, &details))
  {
    if (gum_darwin_module_get_lacks_exports_for_reexports (module))
    {
      GPtrArray * reexports = module->reexports;
      guint i;

      for (i = 0; i != reexports->len; i++)
      {
        GumDarwinMapping * target;

        target = gum_darwin_mapper_get_dependency_by_name (self,
            g_ptr_array_index (reexports, i), NULL);
        if (target == NULL)
          continue;

        if (gum_darwin_mapper_resolve_symbol (self, target->module, name,
            value))
        {
          return TRUE;
        }
      }
    }

    return FALSE;
  }

  if ((details.flags & GUM_DARWIN_EXPORT_REEXPORT) != 0)
  {
    const gchar * target_name;
    GumDarwinMapping * target;

    target_name = gum_darwin_module_get_dependency_by_ordinal (module,
        details.reexport_library_ordinal);

    target = gum_darwin_mapper_get_dependency_by_name (self, target_name, NULL);
    if (target == NULL)
      return FALSE;

    return gum_darwin_mapper_resolve_symbol (self, target->module,
        details.reexport_symbol, value);
  }

  switch (details.flags & GUM_DARWIN_EXPORT_KIND_MASK)
  {
    case GUM_DARWIN_EXPORT_REGULAR:
      if ((details.flags & GUM_DARWIN_EXPORT_STUB_AND_RESOLVER) != 0)
      {
        /* XXX: we ignore interposing */
        value->address = module->base_address + details.stub;
        value->resolver = module->base_address + details.resolver;
        return TRUE;
      }
      value->address = module->base_address + details.offset;
      value->resolver = 0;
      return TRUE;
    case GUM_DARWIN_EXPORT_THREAD_LOCAL:
      value->address = module->base_address + details.offset;
      value->resolver = 0;
      return TRUE;
    case GUM_DARWIN_EXPORT_ABSOLUTE:
      value->address = details.offset;
      value->resolver = 0;
      return TRUE;
    default:
      return FALSE;
  }
}

static GumDarwinMapping *
gum_darwin_mapper_add_existing_mapping (GumDarwinMapper * self,
                                        GumDarwinModule * module)
{
  GumDarwinMapping * mapping;

  mapping = g_slice_new (GumDarwinMapping);
  mapping->module = g_object_ref (module);
  mapping->mapper = NULL;

  g_hash_table_insert (self->mappings, g_strdup (module->name), mapping);

  return mapping;
}

static GumDarwinMapping *
gum_darwin_mapper_add_pending_mapping (GumDarwinMapper * self,
                                       const gchar * name,
                                       GumDarwinMapper * mapper)
{
  GumDarwinMapping * mapping;

  mapping = g_slice_new (GumDarwinMapping);
  mapping->module = g_object_ref (mapper->module);
  mapping->mapper = mapper;

  g_hash_table_insert (self->mappings, g_strdup (name), mapping);

  return mapping;
}

static GumDarwinMapping *
gum_darwin_mapper_add_alias_mapping (GumDarwinMapper * self,
                                     const gchar * name,
                                     const GumDarwinMapping * to)
{
  GumDarwinMapping * mapping;

  mapping = g_slice_dup (GumDarwinMapping, to);
  g_object_ref (mapping->module);

  g_hash_table_insert (self->mappings, g_strdup (name), mapping);

  return mapping;
}

static gboolean
gum_darwin_mapper_resolve_chained_imports (
    const GumDarwinChainedFixupsDetails * details,
    gpointer user_data)
{
  GumMapContext * ctx = user_data;
  GumDarwinMapper * self = ctx->mapper;
  gsize pointer_size = self->module->pointer_size;
  const GumChainedFixupsHeader * fixups_header;
  const gchar * symbols;
  uint32_t count, i;

  fixups_header = gum_darwin_mapper_data_from_offset (self,
      details->file_offset, pointer_size);
  if (fixups_header == NULL)
    goto invalid_data;

  symbols = (const gchar *) fixups_header + fixups_header->symbols_offset;
  count = fixups_header->imports_count;

  g_clear_pointer (&self->chained_symbols, g_array_unref);
  self->chained_symbols = g_array_sized_new (FALSE, FALSE, pointer_size, count);

  switch (fixups_header->imports_format)
  {
    case GUM_CHAINED_IMPORT:
    {
      const GumChainedImport * imports =
          ((const void *) fixups_header + fixups_header->imports_offset);

      for (i = 0; i != count; i++)
      {
        const GumChainedImport * import = &imports[i];
        gint library_ordinal = (gint8) import->lib_ordinal;

        if (!gum_darwin_mapper_append_chained_symbol (self, library_ordinal,
              symbols + import->name_offset, import->weak_import, 0,
              ctx->error))
        {
          goto propagate_error;
        }
      }

      break;
    }
    case GUM_CHAINED_IMPORT_ADDEND:
    {
      const GumChainedImportAddend * imports =
          ((const void *) fixups_header + fixups_header->imports_offset);

      for (i = 0; i != count; i++)
      {
        const GumChainedImportAddend * import = &imports[i];
        gint library_ordinal = (gint8) import->lib_ordinal;

        if (!gum_darwin_mapper_append_chained_symbol (self, library_ordinal,
              symbols + import->name_offset, import->weak_import,
              import->addend, ctx->error))
        {
          goto propagate_error;
        }
      }

      break;
    }
    case GUM_CHAINED_IMPORT_ADDEND64:
    {
      const GumChainedImportAddend64 * imports =
          ((const void *) fixups_header + fixups_header->imports_offset);

      for (i = 0; i != count; i++)
      {
        const GumChainedImportAddend64 * import = &imports[i];
        gint library_ordinal = (gint16) import->lib_ordinal;

        if (!gum_darwin_mapper_append_chained_symbol (self, library_ordinal,
              symbols + import->name_offset, import->weak_import,
              import->addend, ctx->error))
        {
          goto propagate_error;
        }
      }

      break;
    }
  }

  return TRUE;

invalid_data:
  {
    ctx->success = FALSE;
    g_set_error (ctx->error, GUM_ERROR, GUM_ERROR_INVALID_DATA,
        "Malformed chained fixups");
    return FALSE;
  }
propagate_error:
  {
    ctx->success = FALSE;
    return FALSE;
  }
}

static gboolean
gum_darwin_mapper_append_chained_symbol (GumDarwinMapper * self,
                                         gint library_ordinal,
                                         const gchar * symbol_name,
                                         gboolean is_weak,
                                         gint64 addend,
                                         GError ** error)
{
  GumDarwinSymbolValue value;

  if (!gum_darwin_mapper_resolve_import (self, library_ordinal, symbol_name,
        is_weak, &value, error))
  {
    return FALSE;
  }

  if (value.address != 0)
    value.address += addend;

  g_array_append_val (self->chained_symbols, value.address);

  return TRUE;
}

static gboolean
gum_darwin_mapper_rebase (const GumDarwinRebaseDetails * details,
                          gpointer user_data)
{
  GumMapContext * ctx = user_data;
  GumDarwinMapper * self = ctx->mapper;
  gsize pointer_size = self->module->pointer_size;
  gpointer entry;

  if (details->offset >= details->segment->file_size)
    goto invalid_data;

  entry = gum_darwin_mapper_data_from_offset (self,
      details->segment->file_offset + details->offset, pointer_size);
  if (entry == NULL)
    goto invalid_data;

  switch (details->type)
  {
    case GUM_DARWIN_REBASE_POINTER:
    case GUM_DARWIN_REBASE_TEXT_ABSOLUTE32:
      if (pointer_size == 4)
        *((guint32 *) entry) += (guint32) details->slide;
      else
        *((guint64 *) entry) += (guint64) details->slide;
      break;
    case GUM_DARWIN_REBASE_TEXT_PCREL32:
    default:
      goto invalid_data;
  }

  return TRUE;

invalid_data:
  {
    ctx->success = FALSE;
    g_set_error (ctx->error, GUM_ERROR, GUM_ERROR_INVALID_DATA,
        "Malformed rebase entry");
    return FALSE;
  }
}

static gboolean
gum_darwin_mapper_bind (const GumDarwinBindDetails * details,
                        gpointer user_data)
{
  GumMapContext * ctx = user_data;
  GumDarwinMapper * self = ctx->mapper;

  switch (details->type)
  {
    case GUM_DARWIN_BIND_POINTER:
      ctx->success = gum_darwin_mapper_bind_pointer (self, details, ctx->error);
      break;
    case GUM_DARWIN_BIND_THREADED_TABLE:
      ctx->success = gum_darwin_mapper_bind_table (self, details, ctx->error);
      break;
    case GUM_DARWIN_BIND_THREADED_ITEMS:
      ctx->success = gum_darwin_mapper_bind_items (self, details, ctx->error);
      break;
    default:
      goto invalid_data;
  }

  return ctx->success;

invalid_data:
  {
    ctx->success = FALSE;
    g_set_error (ctx->error, GUM_ERROR, GUM_ERROR_INVALID_DATA,
        "Malformed bind entry");
    return FALSE;
  }
}

static gboolean
gum_darwin_mapper_bind_pointer (GumDarwinMapper * self,
                                const GumDarwinBindDetails * bind,
                                GError ** error)
{
  GumDarwinSymbolValue value;

  if (!gum_darwin_mapper_resolve_import (self, bind->library_ordinal,
        bind->symbol_name,
        (bind->symbol_flags & GUM_DARWIN_BIND_WEAK_IMPORT) != 0,
        &value, error))
  {
    return FALSE;
  }

  if (value.address != 0)
    value.address += bind->addend;

  if (self->threaded_symbols != NULL)
  {
    g_array_append_val (self->threaded_symbols, value.address);
  }
  else
  {
    gsize pointer_size = self->module->pointer_size;
    gpointer entry;

    entry = gum_darwin_mapper_data_from_offset (self,
        bind->segment->file_offset + bind->offset, pointer_size);
    if (entry == NULL)
      goto invalid_data;

    if (pointer_size == 4)
      *((guint32 *) entry) = value.address;
    else
      *((guint64 *) entry) = value.address;
  }

  return TRUE;

invalid_data:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_DATA,
        "Malformed bind entry");
    return FALSE;
  }
}

static gboolean
gum_darwin_mapper_bind_table (GumDarwinMapper * self,
                              const GumDarwinBindDetails * bind,
                              GError ** error)
{
  g_clear_pointer (&self->threaded_symbols, g_array_unref);
  g_clear_pointer (&self->threaded_regions, g_array_unref);
  self->threaded_symbols = g_array_sized_new (FALSE, FALSE, sizeof (GumAddress),
      bind->threaded_table_size);
  self->threaded_regions = g_array_sized_new (FALSE, FALSE, sizeof (GumAddress),
      256);

  return TRUE;
}

static gboolean
gum_darwin_mapper_bind_items (GumDarwinMapper * self,
                              const GumDarwinBindDetails * bind,
                              GError ** error)
{
  GArray * threaded_regions = self->threaded_regions;
  GumAddress region_start;

  if (threaded_regions == NULL)
    goto invalid_data;

  region_start =
      self->module->base_address + bind->segment->vm_address + bind->offset;

  g_array_append_val (threaded_regions, region_start);

  return TRUE;

invalid_data:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_DATA,
        "Malformed bind items");
    return FALSE;
  }
}

static void
gum_darwin_mapping_free (GumDarwinMapping * self)
{
  g_object_unref (self->module);
  g_slice_free (GumDarwinMapping, self);
}

static gboolean
gum_find_tlv_get_addr (const GumDarwinSectionDetails * details,
                       gpointer user_data)
{
  GumMapContext * ctx = user_data;
  GumDarwinMapper * self = ctx->mapper;

  /* macOS 26+ uses __dyld_apis instead of __dyld4. */
  if (strcmp (details->section_name, "__dyld_apis") != 0 &&
      strcmp (details->section_name, "__dyld4") != 0)
    return TRUE;

  if (self->module->pointer_size == 4)
  {
    self->tlv_get_addr_addr =
        ((GumLibdyldDyld4Section32 *) GSIZE_TO_POINTER (details->vm_address))
        ->tlv_get_addr_addr;
  }
  else
  {
    self->tlv_get_addr_addr =
        ((GumLibdyldDyld4Section64 *) GSIZE_TO_POINTER (details->vm_address))
        ->tlv_get_addr_addr;
  }

  ctx->success = TRUE;

  return FALSE;
}
