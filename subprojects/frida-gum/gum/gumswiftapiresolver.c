/*
 * Copyright (C) 2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023 Håvard Sørbø <havard@hsorbo.no>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

/**
 * GumSwiftApiResolver:
 *
 * Resolves APIs by searching currently loaded Swift modules.
 *
 * See [iface@Gum.ApiResolver] for more information.
 */

#include "gumswiftapiresolver.h"

#include "gummodulemap.h"
#include "gumprocess.h"

#include <capstone.h>
#include <string.h>

#define GUM_DESCRIPTOR_FLAGS_KIND(flags) \
    (flags & 0x1f)
#define GUM_DESCRIPTOR_FLAGS_KIND_FLAGS(flags) \
    (flags >> 16)
#define GUM_DESCRIPTOR_FLAGS_IS_GENERIC(flags) \
    ((flags & GUM_DESCRIPTOR_IS_GENERIC) != 0)
#define GUM_DESCRIPTOR_FLAGS_IS_UNIQUE(flags) \
    ((flags & GUM_DESCRIPTOR_IS_UNIQUE) != 0)

#define GUM_ANONYMOUS_DESCRIPTOR_FLAGS_HAS_MANGLED_NAME(flags) \
    ((flags & GUM_ANONYMOUS_DESCRIPTOR_HAS_MANGLED_NAME) != 0)

#define GUM_TYPE_FLAGS_METADATA_INITIALIZATION_MASK(flags) \
    (flags & 3)
#define GUM_TYPE_FLAGS_CLASS_HAS_VTABLE(flags) \
    ((flags & GUM_CLASS_HAS_VTABLE) != 0)
#define GUM_TYPE_FLAGS_CLASS_HAS_OVERRIDE_TABLE(flags) \
    ((flags & GUM_CLASS_HAS_OVERRIDE_TABLE) != 0)
#define GUM_TYPE_FLAGS_CLASS_HAS_RESILIENT_SUPERCLASS(flags) \
    ((flags & GUM_CLASS_HAS_RESILIENT_SUPERCLASS) != 0)

#define GUM_GENERIC_DESCRIPTOR_FLAGS_HAS_TYPE_PACKS(flags) \
    ((flags & GUM_GENERIC_DESCRIPTOR_HAS_TYPE_PACKS) != 0)

#define GUM_METHOD_DESCRIPTOR_IS_ASYNC(desc) \
    (((desc)->flags & GUM_METHOD_ASYNC) != 0)

#define GUM_ALIGN(ptr, type) \
    GUM_ALIGN_POINTER (type *, ptr, G_ALIGNOF (type))

typedef struct _GumModuleMetadata GumModuleMetadata;
typedef struct _GumFunctionMetadata GumFunctionMetadata;
typedef gsize (* GumSwiftDemangle) (const gchar * name, gchar * output,
    gsize length);

typedef struct _GumClass GumClass;

typedef guint GumContextDescriptorKind;
typedef struct _GumContextDescriptor GumContextDescriptor;
typedef struct _GumModuleContextDescriptor GumModuleContextDescriptor;
typedef struct _GumExtensionContextDescriptor GumExtensionContextDescriptor;
typedef struct _GumTypeContextDescriptor GumTypeContextDescriptor;
typedef struct _GumClassDescriptor GumClassDescriptor;
typedef struct _GumGenericContextDescriptorHeader
    GumGenericContextDescriptorHeader;
typedef struct _GumGenericParamDescriptor GumGenericParamDescriptor;
typedef struct _GumGenericRequirementDescriptor GumGenericRequirementDescriptor;
typedef struct _GumTypeGenericContextDescriptorHeader
    GumTypeGenericContextDescriptorHeader;
typedef struct _GumGenericPackShapeHeader GumGenericPackShapeHeader;
typedef struct _GumGenericPackShapeDescriptor GumGenericPackShapeDescriptor;
typedef guint16 GumGenericPackKind;
typedef struct _GumResilientSuperclass GumResilientSuperclass;
typedef struct _GumSingletonMetadataInitialization
    GumSingletonMetadataInitialization;
typedef struct _GumForeignMetadataInitialization
    GumForeignMetadataInitialization;
typedef struct _GumVTableDescriptorHeader GumVTableDescriptorHeader;
typedef struct _GumMethodDescriptor GumMethodDescriptor;
typedef struct _GumOverrideTableHeader GumOverrideTableHeader;
typedef struct _GumMethodOverrideDescriptor GumMethodOverrideDescriptor;

typedef gint32 GumRelativeDirectPtr;
typedef gint32 GumRelativeIndirectPtr;
typedef gint32 GumRelativeIndirectablePtr;

struct _GumSwiftApiResolver
{
  GObject parent;

  GRegex * query_pattern;

  GHashTable * modules;
  GumModuleMap * all_modules;
};

struct _GumModuleMetadata
{
  gint ref_count;

  GumModule * module;

  GArray * functions;
  GHashTable * vtables;
  GumSwiftApiResolver * resolver;
};

struct _GumFunctionMetadata
{
  gchar * name;
  GumAddress address;
};

struct _GumClass
{
  gchar * name;

  const GumMethodDescriptor * methods;
  guint num_methods;

  const GumMethodOverrideDescriptor * overrides;
  guint num_overrides;
};

enum _GumContextDescriptorKind
{
  GUM_CONTEXT_DESCRIPTOR_MODULE,
  GUM_CONTEXT_DESCRIPTOR_EXTENSION,
  GUM_CONTEXT_DESCRIPTOR_ANONYMOUS,
  GUM_CONTEXT_DESCRIPTOR_PROTOCOL,
  GUM_CONTEXT_DESCRIPTOR_OPAQUE_TYPE,

  GUM_CONTEXT_DESCRIPTOR_TYPE_FIRST = 16,

  GUM_CONTEXT_DESCRIPTOR_CLASS = GUM_CONTEXT_DESCRIPTOR_TYPE_FIRST,
  GUM_CONTEXT_DESCRIPTOR_STRUCT = GUM_CONTEXT_DESCRIPTOR_TYPE_FIRST + 1,
  GUM_CONTEXT_DESCRIPTOR_ENUM = GUM_CONTEXT_DESCRIPTOR_TYPE_FIRST + 2,

  GUM_CONTEXT_DESCRIPTOR_TYPE_LAST = 31,
};

enum _GumContextDescriptorFlags
{
  GUM_DESCRIPTOR_IS_GENERIC = (1 << 7),
  GUM_DESCRIPTOR_IS_UNIQUE  = (1 << 6),
};

enum _GumAnonymousContextDescriptorFlags
{
  GUM_ANONYMOUS_DESCRIPTOR_HAS_MANGLED_NAME = (1 << 0),
};

enum _GumTypeContextDescriptorFlags
{
  GUM_CLASS_HAS_VTABLE               = (1 << 15),
  GUM_CLASS_HAS_OVERRIDE_TABLE       = (1 << 14),
  GUM_CLASS_HAS_RESILIENT_SUPERCLASS = (1 << 13),
};

enum _GumTypeMetadataInitializationKind
{
  GUM_METADATA_INITIALIZATION_NONE,
  GUM_METADATA_INITIALIZATION_SINGLETON,
  GUM_METADATA_INITIALIZATION_FOREIGN,
};

struct _GumContextDescriptor
{
  guint32 flags;
  GumRelativeIndirectablePtr parent;
};

struct _GumModuleContextDescriptor
{
  GumContextDescriptor context;
  GumRelativeDirectPtr name;
};

struct _GumExtensionContextDescriptor
{
  GumContextDescriptor context;
  GumRelativeDirectPtr extended_context;
};

struct _GumTypeContextDescriptor
{
  GumContextDescriptor context;
  GumRelativeDirectPtr name;
  GumRelativeDirectPtr access_function_ptr;
  GumRelativeDirectPtr fields;
};

struct _GumClassDescriptor
{
  GumTypeContextDescriptor type_context;
  GumRelativeDirectPtr superclass_type;
  guint32 metadata_negative_size_in_words_or_resilient_metadata_bounds;
  guint32 metadata_positive_size_in_words_or_extra_class_flags;
  guint32 num_immediate_members;
  guint32 num_fields;
  guint32 field_offset_vector_offset;
};

struct _GumGenericContextDescriptorHeader
{
  guint16 num_params;
  guint16 num_requirements;
  guint16 num_key_arguments;
  guint16 flags;
};

enum _GumGenericContextDescriptorFlags
{
  GUM_GENERIC_DESCRIPTOR_HAS_TYPE_PACKS = (1 << 0),
};

struct _GumGenericParamDescriptor
{
  guint8 value;
};

struct _GumGenericRequirementDescriptor
{
  guint32 flags;
  GumRelativeDirectPtr param;
  GumRelativeDirectPtr type_or_protocol_or_conformance_or_layout;
};

struct _GumTypeGenericContextDescriptorHeader
{
  GumRelativeDirectPtr instantiation_cache;
  GumRelativeDirectPtr default_instantiation_pattern;
  GumGenericContextDescriptorHeader base;
};

struct _GumGenericPackShapeHeader
{
  guint16 num_packs;
  guint16 num_shape_classes;
};

struct _GumGenericPackShapeDescriptor
{
  GumGenericPackKind kind;
  guint16 index;
  guint16 shape_class;
  guint16 unused;
};

enum _GumGenericPackKind
{
  GUM_GENERIC_PACK_METADATA,
  GUM_GENERIC_PACK_WITNESS_TABLE,
};

struct _GumResilientSuperclass
{
  GumRelativeDirectPtr superclass;
};

struct _GumSingletonMetadataInitialization
{
  GumRelativeDirectPtr initialization_cache;
  GumRelativeDirectPtr incomplete_metadata_or_resilient_pattern;
  GumRelativeDirectPtr completion_function;
};

struct _GumForeignMetadataInitialization
{
  GumRelativeDirectPtr completion_function;
};

struct _GumVTableDescriptorHeader
{
  guint32 vtable_offset;
  guint32 vtable_size;
};

struct _GumMethodDescriptor
{
  guint32 flags;
  GumRelativeDirectPtr impl;
};

enum _GumMethodDescriptorFlags
{
  GUM_METHOD_ASYNC = (1 << 6),
};

struct _GumOverrideTableHeader
{
  guint32 num_entries;
};

struct _GumMethodOverrideDescriptor
{
  GumRelativeIndirectablePtr class;
  GumRelativeIndirectablePtr method;
  GumRelativeDirectPtr impl;
};

static void gum_swift_api_resolver_iface_init (gpointer g_iface,
    gpointer iface_data);
static GumModuleMetadata * gum_swift_api_resolver_register_module (
    GumSwiftApiResolver * self, GumModule * module);
static void gum_swift_api_resolver_dispose (GObject * object);
static void gum_swift_api_resolver_finalize (GObject * object);
static void gum_swift_api_resolver_enumerate_matches (
    GumApiResolver * resolver, const gchar * query, GumFoundApiFunc func,
    gpointer user_data, GError ** error);

static void gum_module_metadata_unref (GumModuleMetadata * module);
static GArray * gum_module_metadata_get_functions (GumModuleMetadata * self);
static gboolean gum_module_metadata_collect_export (
    const GumExportDetails * details, gpointer user_data);
static gboolean gum_module_metadata_collect_section (
    const GumSectionDetails * details, gpointer user_data);
static void gum_module_metadata_collect_class (GumModuleMetadata * self,
    const GumTypeContextDescriptor * type);
static void gum_module_metadata_maybe_ingest_thunk (GumModuleMetadata * self,
    const gchar * name, GumAddress address);
#ifdef HAVE_ARM64
static gchar * gum_extract_class_name (const gchar * full_name);
static const gchar * gum_find_character_backwards (const gchar * starting_point,
    char needle, const gchar * start);
#endif

static void gum_function_metadata_free (GumFunctionMetadata * function);

static void gum_class_parse (GumClass * klass, const GumClassDescriptor * cd);
static void gum_class_clear (GumClass * klass);

static gconstpointer gum_resolve_method_implementation (
    const GumRelativeDirectPtr * impl, const GumMethodDescriptor * method);

static gchar * gum_compute_context_descriptor_name (
    const GumContextDescriptor * cd);
static void gum_append_demangled_context_name (GString * result,
    const gchar * mangled_name);

static void gum_skip_generic_type_trailers (gconstpointer * trailer_ptr,
    const GumTypeContextDescriptor * t);
static void gum_skip_generic_parts (gconstpointer * trailer_ptr,
    const GumGenericContextDescriptorHeader * h);
static void gum_skip_resilient_superclass_trailer (gconstpointer * trailer_ptr,
    const GumTypeContextDescriptor * t);
static void gum_skip_metadata_initialization_trailers (
    gconstpointer * trailer_ptr, const GumTypeContextDescriptor * t);

static gconstpointer gum_resolve_relative_direct_ptr (
    const GumRelativeDirectPtr * delta);
static gconstpointer gum_resolve_relative_indirect_ptr (
    const GumRelativeIndirectPtr * delta);
static gconstpointer gum_resolve_relative_indirectable_ptr (
    const GumRelativeIndirectablePtr * delta);

static gchar * gum_demangle (const gchar * name);

G_DEFINE_TYPE_EXTENDED (GumSwiftApiResolver,
                        gum_swift_api_resolver,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_API_RESOLVER,
                            gum_swift_api_resolver_iface_init))

static GumSwiftDemangle gum_demangle_impl;

static void
gum_swift_api_resolver_class_init (GumSwiftApiResolverClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_swift_api_resolver_dispose;
  object_class->finalize = gum_swift_api_resolver_finalize;

  gum_demangle_impl = GUM_POINTER_TO_FUNCPTR (GumSwiftDemangle,
      gum_module_find_global_export_by_name (
        "swift_demangle_getDemangledName"));
}

static void
gum_swift_api_resolver_iface_init (gpointer g_iface,
                                   gpointer iface_data)
{
  GumApiResolverInterface * iface = g_iface;

  iface->enumerate_matches = gum_swift_api_resolver_enumerate_matches;
}

static void
gum_swift_api_resolver_init (GumSwiftApiResolver * self)
{
  GPtrArray * entries;
  guint i;

  self->query_pattern = g_regex_new ("functions:(.+)!([^\\n\\r\\/]+)(\\/i)?",
      0, 0, NULL);

  self->modules = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) gum_module_metadata_unref);

  self->all_modules = gum_module_map_new ();

  entries = gum_module_map_get_values (self->all_modules);
  for (i = 0; i != entries->len; i++)
  {
    GumModule * m = g_ptr_array_index (entries, i);

    gum_swift_api_resolver_register_module (self, m);
  }
}

static GumModuleMetadata *
gum_swift_api_resolver_register_module (GumSwiftApiResolver * self,
                                        GumModule * module)
{
  GumModuleMetadata * meta;

  meta = g_slice_new0 (GumModuleMetadata);
  meta->ref_count = 2;
  meta->module = module;
  meta->functions = NULL;
  meta->vtables = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, (GDestroyNotify) g_ptr_array_unref);
  meta->resolver = self;

  g_hash_table_insert (self->modules, (gpointer) gum_module_get_name (module),
      meta);
  g_hash_table_insert (self->modules, (gpointer) gum_module_get_path (module),
      meta);

  return meta;
}

static void
gum_swift_api_resolver_dispose (GObject * object)
{
  GumSwiftApiResolver * self = GUM_SWIFT_API_RESOLVER (object);

  g_clear_object (&self->all_modules);

  g_clear_pointer (&self->modules, g_hash_table_unref);

  G_OBJECT_CLASS (gum_swift_api_resolver_parent_class)->dispose (object);
}

static void
gum_swift_api_resolver_finalize (GObject * object)
{
  GumSwiftApiResolver * self = GUM_SWIFT_API_RESOLVER (object);

  g_regex_unref (self->query_pattern);

  G_OBJECT_CLASS (gum_swift_api_resolver_parent_class)->finalize (object);
}

/**
 * gum_swift_api_resolver_new:
 *
 * Creates a new resolver that searches exports and imports of currently loaded
 * modules.
 *
 * Returns: (transfer full): the newly created resolver instance
 */
GumApiResolver *
gum_swift_api_resolver_new (void)
{
  return g_object_new (GUM_TYPE_SWIFT_API_RESOLVER, NULL);
}

static void
gum_swift_api_resolver_enumerate_matches (GumApiResolver * resolver,
                                          const gchar * query,
                                          GumFoundApiFunc func,
                                          gpointer user_data,
                                          GError ** error)
{
  GumSwiftApiResolver * self = GUM_SWIFT_API_RESOLVER (resolver);
  GMatchInfo * query_info;
  gboolean ignore_case;
  gchar * module_query, * func_query;
  GPatternSpec * module_spec, * func_spec;
  GHashTableIter module_iter;
  GHashTable * seen_modules;
  gboolean carry_on;
  GumModuleMetadata * module;

  if (gum_demangle_impl == NULL)
    goto unsupported_runtime;

  g_regex_match (self->query_pattern, query, 0, &query_info);
  if (!g_match_info_matches (query_info))
    goto invalid_query;

  ignore_case = g_match_info_get_match_count (query_info) >= 5;

  module_query = g_match_info_fetch (query_info, 1);
  func_query = g_match_info_fetch (query_info, 2);

  g_match_info_free (query_info);

  if (ignore_case)
  {
    gchar * str;

    str = g_utf8_strdown (module_query, -1);
    g_free (module_query);
    module_query = str;

    str = g_utf8_strdown (func_query, -1);
    g_free (func_query);
    func_query = str;
  }

  module_spec = g_pattern_spec_new (module_query);
  func_spec = g_pattern_spec_new (func_query);

  g_hash_table_iter_init (&module_iter, self->modules);
  seen_modules = g_hash_table_new (NULL, NULL);
  carry_on = TRUE;

  while (carry_on &&
      g_hash_table_iter_next (&module_iter, NULL, (gpointer *) &module))
  {
    const gchar * module_name, * module_path;
    const gchar * normalized_module_name, * normalized_module_path;
    gchar * module_name_copy = NULL;
    gchar * module_path_copy = NULL;

    if (g_hash_table_contains (seen_modules, module))
      continue;
    g_hash_table_add (seen_modules, module);

    module_name = gum_module_get_name (module->module);
    module_path = gum_module_get_path (module->module);

    if (ignore_case)
    {
      module_name_copy = g_utf8_strdown (module_name, -1);
      normalized_module_name = module_name_copy;

      module_path_copy = g_utf8_strdown (module_path, -1);
      normalized_module_path = module_path_copy;
    }
    else
    {
      normalized_module_name = module_name;
      normalized_module_path = module_path;
    }

    if (g_pattern_spec_match_string (module_spec, normalized_module_name) ||
        g_pattern_spec_match_string (module_spec, normalized_module_path))
    {
      GArray * functions;
      guint i;

      functions = gum_module_metadata_get_functions (module);

      for (i = 0; carry_on && i != functions->len; i++)
      {
        const GumFunctionMetadata * f =
            &g_array_index (functions, GumFunctionMetadata, i);

        if (g_pattern_spec_match_string (func_spec, f->name))
        {
          GumApiDetails details;

          details.name = g_strconcat (
              module_path,
              "!",
              f->name,
              NULL);
          details.address = f->address;
          details.size = GUM_API_SIZE_NONE;

          carry_on = func (&details, user_data);

          g_free ((gpointer) details.name);
        }
      }
    }

    g_free (module_path_copy);
    g_free (module_name_copy);
  }

  g_hash_table_unref (seen_modules);

  g_pattern_spec_free (func_spec);
  g_pattern_spec_free (module_spec);

  g_free (func_query);
  g_free (module_query);

  return;

unsupported_runtime:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_SUPPORTED,
        "unsupported Swift runtime; please file a bug");
  }
invalid_query:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT,
        "invalid query; format is: *someModule*!SomeClassPrefix*.*secret*()");
  }
}

static void
gum_module_metadata_unref (GumModuleMetadata * module)
{
  module->ref_count--;
  if (module->ref_count == 0)
  {
    if (module->vtables != NULL)
      g_hash_table_unref (module->vtables);

    if (module->functions != NULL)
      g_array_unref (module->functions);

    g_slice_free (GumModuleMetadata, module);
  }
}

static GArray *
gum_module_metadata_get_functions (GumModuleMetadata * self)
{
  if (self->functions == NULL)
  {
    self->functions = g_array_new (FALSE, FALSE, sizeof (GumFunctionMetadata));
    g_array_set_clear_func (self->functions,
        (GDestroyNotify) gum_function_metadata_free);

    gum_module_enumerate_exports (self->module,
        gum_module_metadata_collect_export, self);
    gum_module_enumerate_sections (self->module,
        gum_module_metadata_collect_section, self);
  }

  return self->functions;
}

static gboolean
gum_module_metadata_collect_export (const GumExportDetails * details,
                                    gpointer user_data)
{
  GumModuleMetadata * self = user_data;
  gchar * name;
  GumFunctionMetadata func;

  if (details->type != GUM_EXPORT_FUNCTION)
    goto skip;

  name = gum_demangle (details->name);
  if (name == NULL)
    goto skip;

  func.name = name;
  func.address = details->address;
  g_array_append_val (self->functions, func);

  gum_module_metadata_maybe_ingest_thunk (self, name,
      gum_strip_code_address (func.address));

skip:
  return TRUE;
}

static gboolean
gum_module_metadata_collect_section (const GumSectionDetails * details,
                                     gpointer user_data)
{
  GumModuleMetadata * module = user_data;
  gsize n, i;
  GumRelativeDirectPtr * types;

  if (strcmp (details->name, "__swift5_types") != 0)
    return TRUE;

  n = details->size / sizeof (gint32);

  types = GSIZE_TO_POINTER (details->address);

  for (i = 0; i != n; i++)
  {
    const GumTypeContextDescriptor * type;
    guint32 descriptor_flags;

    type = gum_resolve_relative_indirectable_ptr (&types[i]);
    descriptor_flags = type->context.flags;

    switch (GUM_DESCRIPTOR_FLAGS_KIND (descriptor_flags))
    {
      case GUM_CONTEXT_DESCRIPTOR_CLASS:
        gum_module_metadata_collect_class (module, type);
        break;
      default:
        break;
    }
  }

  return TRUE;
}

static void
gum_module_metadata_collect_class (GumModuleMetadata * self,
                                   const GumTypeContextDescriptor * type)
{
  GumClass klass;
  guint i;

  gum_class_parse (&klass, (const GumClassDescriptor *) type);

  if (klass.num_methods != 0)
  {
    GPtrArray * vtable;

    vtable = g_hash_table_lookup (self->vtables, klass.name);

    for (i = 0; i != klass.num_methods; i++)
    {
      const GumMethodDescriptor * method = &klass.methods[i];
      gconstpointer impl;
      GumFunctionMetadata func;

      impl = gum_resolve_method_implementation (&method->impl, method);
      if (impl == NULL)
        continue;

      func.name = NULL;
      if (vtable != NULL && i < vtable->len)
        func.name = g_strdup (g_ptr_array_index (vtable, i));
      if (func.name == NULL)
        func.name = g_strdup_printf ("%s.vtable[%u]", klass.name, i);

      func.address = GUM_ADDRESS (impl);

      g_array_append_val (self->functions, func);
    }
  }

  for (i = 0; i != klass.num_overrides; i++)
  {
    const GumMethodOverrideDescriptor * od = &klass.overrides[i];
    GumClass parent_class;
    const GumMethodDescriptor * parent_method;
    guint vtable_index;
    gconstpointer impl;
    GPtrArray * parent_vtable;
    GumFunctionMetadata func;

    gum_class_parse (&parent_class,
        gum_resolve_relative_indirectable_ptr (&od->class));
    parent_method = gum_resolve_relative_indirectable_ptr (&od->method);
    vtable_index = parent_method - parent_class.methods;

    impl = gum_resolve_method_implementation (&od->impl, parent_method);
    if (impl == NULL)
      continue;

    parent_vtable = g_hash_table_lookup (self->vtables, parent_class.name);

    func.name = NULL;
    if (parent_vtable != NULL && vtable_index < parent_vtable->len)
    {
      const gchar * name = g_ptr_array_index (parent_vtable, vtable_index);
      if (name != NULL)
      {
        func.name = g_strconcat (
            klass.name,
            name + strlen (parent_class.name),
            NULL);
      }
    }
    if (func.name == NULL)
      func.name = g_strdup_printf ("%s.overrides[%u]", klass.name, i);

    func.address = GUM_ADDRESS (impl);

    g_array_append_val (self->functions, func);

    gum_class_clear (&parent_class);
  }

  gum_class_clear (&klass);
}

#ifdef HAVE_ARM64

static void
gum_module_metadata_maybe_ingest_thunk (GumModuleMetadata * self,
                                        const gchar * name,
                                        GumAddress address)
{
  csh capstone;
  const uint8_t * code;
  size_t size;
  cs_insn * insn;
  gint vtable_index, vtable_offsets[18];
  gboolean end_of_thunk;
  guint i;

  if (!g_str_has_prefix (name, "dispatch thunk of "))
    return;

  gum_cs_arch_register_native ();
  cs_open (GUM_DEFAULT_CS_ARCH, GUM_DEFAULT_CS_MODE, &capstone);
  cs_option (capstone, CS_OPT_DETAIL, CS_OPT_ON);

  code = GSIZE_TO_POINTER (address);
  size = 1024;

  insn = cs_malloc (capstone);

  vtable_index = -1;
  for (i = 0; i != G_N_ELEMENTS (vtable_offsets); i++)
    vtable_offsets[i] = -1;
  end_of_thunk = FALSE;

  while (vtable_index == -1 && !end_of_thunk &&
      cs_disasm_iter (capstone, &code, &size, &address, insn))
  {
    const cs_arm64_op * ops = insn->detail->arm64.operands;

#define GUM_REG_IS_TRACKED(reg) (reg >= ARM64_REG_X0 && reg <= ARM64_REG_X17)
#define GUM_REG_INDEX(reg) (reg - ARM64_REG_X0)

    switch (insn->id)
    {
      case ARM64_INS_LDR:
      {
        arm64_reg dst = ops[0].reg;
        const arm64_op_mem * src = &ops[1].mem;

        if (GUM_REG_IS_TRACKED (dst))
        {
          if (!(src->base == ARM64_REG_X20 && src->disp == 0))
          {
            /*
             * ldr x3, [x16, #0xd0]!
             * ...
             * braa x3, x16
             */
            vtable_offsets[GUM_REG_INDEX (dst)] = src->disp;
          }
        }

        break;
      }
      case ARM64_INS_MOV:
      {
        arm64_reg dst = ops[0].reg;
        const cs_arm64_op * src = &ops[1];

        /*
         * mov x17, #0x3b0
         * add x16, x16, x17
         * ldr x7, [x16]
         * ...
         * braa x7, x16
         */
        if (src->type == ARM64_OP_IMM && GUM_REG_IS_TRACKED (dst))
          vtable_offsets[GUM_REG_INDEX (dst)] = src->imm;

        break;
      }
      case ARM64_INS_ADD:
      {
        arm64_reg dst = ops[0].reg;
        arm64_reg left = ops[1].reg;
        const cs_arm64_op * right = &ops[2];
        gint offset;

        if (left == dst)
        {
          if (right->type == ARM64_OP_REG &&
              GUM_REG_IS_TRACKED (right->reg) &&
              (offset = vtable_offsets[GUM_REG_INDEX (right->reg)]) != -1)
          {
            vtable_index = offset / sizeof (gpointer);
          }

          if (right->type == ARM64_OP_IMM)
          {
            vtable_index = right->imm / sizeof (gpointer);
          }
        }

        break;
      }
      case ARM64_INS_BR:
      case ARM64_INS_BRAA:
      case ARM64_INS_BRAAZ:
      case ARM64_INS_BRAB:
      case ARM64_INS_BRABZ:
      case ARM64_INS_BLR:
      case ARM64_INS_BLRAA:
      case ARM64_INS_BLRAAZ:
      case ARM64_INS_BLRAB:
      case ARM64_INS_BLRABZ:
      {
        arm64_reg target = ops[0].reg;
        gint offset;

        switch (insn->id)
        {
          case ARM64_INS_BR:
          case ARM64_INS_BRAA:
          case ARM64_INS_BRAAZ:
          case ARM64_INS_BRAB:
          case ARM64_INS_BRABZ:
            end_of_thunk = TRUE;
            break;
          default:
            break;
        }

        if (GUM_REG_IS_TRACKED (target) &&
            (offset = vtable_offsets[GUM_REG_INDEX (target)]) != -1)
        {
          vtable_index = offset / sizeof (gpointer);
        }

        break;
      }
      case ARM64_INS_RET:
      case ARM64_INS_RETAA:
      case ARM64_INS_RETAB:
        end_of_thunk = TRUE;
        break;
    }

#undef GUM_REG_IS_TRACKED
#undef GUM_REG_INDEX
  }

  cs_free (insn, 1);

  cs_close (&capstone);

  if (vtable_index != -1)
  {
    const gchar * full_name;
    gchar * class_name;
    GPtrArray * vtable;

    full_name = name + strlen ("dispatch thunk of ");
    class_name = gum_extract_class_name (full_name);
    if (class_name == NULL)
      return;

    vtable = g_hash_table_lookup (self->vtables, class_name);
    if (vtable == NULL)
    {
      vtable = g_ptr_array_new_full (64, g_free);
      g_hash_table_insert (self->vtables, g_steal_pointer (&class_name),
          vtable);
    }

    if (vtable_index >= vtable->len)
      g_ptr_array_set_size (vtable, vtable_index + 1);
    g_free (g_ptr_array_index (vtable, vtable_index));
    g_ptr_array_index (vtable, vtable_index) = g_strdup (full_name);

    g_free (class_name);
  }
}

static gchar *
gum_extract_class_name (const gchar * full_name)
{
  const gchar * ch;

  ch = strstr (full_name, " : ");
  if (ch != NULL)
  {
    ch = gum_find_character_backwards (ch, '.', full_name);
    if (ch == NULL)
      return NULL;
  }
  else
  {
    const gchar * start;

    start = g_str_has_prefix (full_name, "(extension in ")
        ? full_name + strlen ("(extension in ")
        : full_name;

    ch = strchr (start, '(');
    if (ch == NULL)
      return NULL;
  }

  ch = gum_find_character_backwards (ch, '.', full_name);
  if (ch == NULL)
    return NULL;

  return g_strndup (full_name, ch - full_name);
}

static const gchar *
gum_find_character_backwards (const gchar * starting_point,
                              char needle,
                              const gchar * start)
{
  const gchar * ch = starting_point;

  while (ch != start)
  {
    ch--;
    if (*ch == needle)
      return ch;
  }

  return NULL;
}

#else

static void
gum_module_metadata_maybe_ingest_thunk (GumModuleMetadata * self,
                                        const gchar * name,
                                        GumAddress address)
{
}

#endif

static void
gum_function_metadata_free (GumFunctionMetadata * function)
{
  g_free (function->name);
}

static void
gum_class_parse (GumClass * klass,
                 const GumClassDescriptor * cd)
{
  const GumTypeContextDescriptor * type;
  gconstpointer trailer;
  guint16 type_flags;

  memset (klass, 0, sizeof (GumClass));

  type = &cd->type_context;

  klass->name = gum_compute_context_descriptor_name (&type->context);

  trailer = cd + 1;

  gum_skip_generic_type_trailers (&trailer, type);

  gum_skip_resilient_superclass_trailer (&trailer, type);

  gum_skip_metadata_initialization_trailers (&trailer, type);

  type_flags = GUM_DESCRIPTOR_FLAGS_KIND_FLAGS (type->context.flags);

  if (GUM_TYPE_FLAGS_CLASS_HAS_VTABLE (type_flags))
  {
    const GumVTableDescriptorHeader * vth;
    const GumMethodDescriptor * methods;

    vth = GUM_ALIGN (trailer, GumVTableDescriptorHeader);
    methods = GUM_ALIGN ((const GumMethodDescriptor *) (vth + 1),
        GumMethodDescriptor);

    klass->methods = methods;
    klass->num_methods = vth->vtable_size;

    trailer = methods + vth->vtable_size;
  }

  if (GUM_TYPE_FLAGS_CLASS_HAS_OVERRIDE_TABLE (type_flags))
  {
    const GumOverrideTableHeader * oth;
    const GumMethodOverrideDescriptor * overrides;

    oth = GUM_ALIGN (trailer, GumOverrideTableHeader);
    overrides = GUM_ALIGN ((const GumMethodOverrideDescriptor *) (oth + 1),
        GumMethodOverrideDescriptor);

    klass->overrides = overrides;
    klass->num_overrides = oth->num_entries;

    trailer = overrides + oth->num_entries;
  }
}

static void
gum_class_clear (GumClass * klass)
{
  g_free (klass->name);
}

static gconstpointer
gum_resolve_method_implementation (const GumRelativeDirectPtr * impl,
                                   const GumMethodDescriptor * method)
{
  gconstpointer address;

  address = gum_resolve_relative_direct_ptr (impl);
  if (address == NULL)
    return NULL;

  if (GUM_METHOD_DESCRIPTOR_IS_ASYNC (method))
    address = gum_resolve_relative_direct_ptr (address);

  return address;
}

static gchar *
gum_compute_context_descriptor_name (const GumContextDescriptor * cd)
{
  GString * name;
  const GumContextDescriptor * cur;
  gboolean reached_toplevel;

  name = g_string_sized_new (16);

  for (cur = cd, reached_toplevel = FALSE;
      cur != NULL && !reached_toplevel;
      cur = gum_resolve_relative_indirectable_ptr (&cur->parent))
  {
    GumContextDescriptorKind kind = GUM_DESCRIPTOR_FLAGS_KIND (cur->flags);

    switch (kind)
    {
      case GUM_CONTEXT_DESCRIPTOR_MODULE:
      {
        const GumModuleContextDescriptor * m =
            (const GumModuleContextDescriptor *) cur;
        if (name->len != 0)
          g_string_prepend_c (name, '.');
        g_string_prepend (name, gum_resolve_relative_direct_ptr (&m->name));
        break;
      }
      case GUM_CONTEXT_DESCRIPTOR_EXTENSION:
      {
        const GumExtensionContextDescriptor * e =
            (const GumExtensionContextDescriptor *) cur;
        GString * part;
        gchar * parent;

        part = g_string_sized_new (64);
        g_string_append (part, "(extension in ");

        parent = gum_compute_context_descriptor_name (
            gum_resolve_relative_indirectable_ptr (&cur->parent));
        g_string_append (part, parent);
        g_free (parent);

        g_string_append (part, "):");

        gum_append_demangled_context_name (part,
            gum_resolve_relative_direct_ptr (&e->extended_context));

        if (name->len != 0)
          g_string_append_c (part, '.');

        g_string_prepend (name, part->str);

        g_string_free (part, TRUE);

        reached_toplevel = TRUE;

        break;
      }
      case GUM_CONTEXT_DESCRIPTOR_ANONYMOUS:
        break;
      default:
        if (kind >= GUM_CONTEXT_DESCRIPTOR_TYPE_FIRST &&
            kind <= GUM_CONTEXT_DESCRIPTOR_TYPE_LAST)
        {
          const GumTypeContextDescriptor * t =
              (const GumTypeContextDescriptor *) cur;
          if (name->len != 0)
            g_string_prepend_c (name, '.');
          g_string_prepend (name, gum_resolve_relative_direct_ptr (&t->name));
          break;
        }

        break;
    }
  }

  return g_string_free (name, FALSE);
}

static void
gum_append_demangled_context_name (GString * result,
                                   const gchar * mangled_name)
{
  switch (mangled_name[0])
  {
    case '\x01':
    {
      const GumContextDescriptor * cd;
      gchar * name;

      cd = gum_resolve_relative_direct_ptr (
          (const GumRelativeDirectPtr *) (mangled_name + 1));
      name = gum_compute_context_descriptor_name (cd);
      g_string_append (result, name);
      g_free (name);

      break;
    }
    case '\x02':
    {
      const GumContextDescriptor * cd;
      gchar * name;

      cd = gum_resolve_relative_indirect_ptr (
          (const GumRelativeIndirectPtr *) (mangled_name + 1));
      name = gum_compute_context_descriptor_name (cd);
      g_string_append (result, name);
      g_free (name);

      break;
    }
    default:
    {
      GString * buf;
      gchar * name;

      buf = g_string_sized_new (32);
      g_string_append (buf, "$s");
      g_string_append (buf, mangled_name);

      name = gum_demangle (buf->str);
      if (name != NULL)
      {
        g_string_append (result, name);
        g_free (name);
      }
      else
      {
        g_string_append (result, "<unsupported mangled name>");
      }

      g_string_free (buf, TRUE);

      break;
    }
  }
}

static void
gum_skip_generic_type_trailers (gconstpointer * trailer_ptr,
                                const GumTypeContextDescriptor * t)
{
  gconstpointer trailer = *trailer_ptr;

  if (GUM_DESCRIPTOR_FLAGS_IS_GENERIC (t->context.flags))
  {
    const GumTypeGenericContextDescriptorHeader * th;

    th = GUM_ALIGN (trailer, GumTypeGenericContextDescriptorHeader);
    trailer = th + 1;

    gum_skip_generic_parts (&trailer, &th->base);
  }

  *trailer_ptr = trailer;
}

static void
gum_skip_generic_parts (gconstpointer * trailer_ptr,
                        const GumGenericContextDescriptorHeader * h)
{
  gconstpointer trailer = *trailer_ptr;

  if (h->num_params != 0)
  {
    const GumGenericParamDescriptor * params = trailer;
    trailer = params + h->num_params;
  }

  {
    const GumGenericRequirementDescriptor * reqs =
        GUM_ALIGN (trailer, GumGenericRequirementDescriptor);
    trailer = reqs + h->num_requirements;
  }

  if (GUM_GENERIC_DESCRIPTOR_FLAGS_HAS_TYPE_PACKS (h->flags))
  {
    const GumGenericPackShapeHeader * sh =
        GUM_ALIGN (trailer, GumGenericPackShapeHeader);
    trailer = sh + 1;

    if (sh->num_packs != 0)
    {
      const GumGenericPackShapeDescriptor * d =
          GUM_ALIGN (trailer, GumGenericPackShapeDescriptor);
      trailer = d + sh->num_packs;
    }
  }

  *trailer_ptr = trailer;
}

static void
gum_skip_resilient_superclass_trailer (gconstpointer * trailer_ptr,
                                       const GumTypeContextDescriptor * t)
{
  gconstpointer trailer = *trailer_ptr;

  if (GUM_TYPE_FLAGS_CLASS_HAS_RESILIENT_SUPERCLASS (
        GUM_DESCRIPTOR_FLAGS_KIND_FLAGS (t->context.flags)))
  {
    const GumResilientSuperclass * rs =
        GUM_ALIGN (trailer, GumResilientSuperclass);
    trailer = rs + 1;
  }

  *trailer_ptr = trailer;
}

static void
gum_skip_metadata_initialization_trailers (gconstpointer * trailer_ptr,
                                           const GumTypeContextDescriptor * t)
{
  gconstpointer trailer = *trailer_ptr;

  switch (GUM_TYPE_FLAGS_METADATA_INITIALIZATION_MASK (
        GUM_DESCRIPTOR_FLAGS_KIND_FLAGS (t->context.flags)))
  {
    case GUM_METADATA_INITIALIZATION_NONE:
      break;
    case GUM_METADATA_INITIALIZATION_SINGLETON:
    {
      const GumSingletonMetadataInitialization * smi =
          GUM_ALIGN (trailer, GumSingletonMetadataInitialization);
      trailer = smi + 1;
      break;
    }
    case GUM_METADATA_INITIALIZATION_FOREIGN:
    {
      const GumForeignMetadataInitialization * fmi =
          GUM_ALIGN (trailer, GumForeignMetadataInitialization);
      trailer = fmi + 1;
      break;
    }
  }

  *trailer_ptr = trailer;
}

static gconstpointer
gum_resolve_relative_direct_ptr (const GumRelativeDirectPtr * delta)
{
  GumRelativeDirectPtr val = *delta;

  if (val == 0)
    return NULL;

  return (const guint8 *) delta + val;
}

static gconstpointer
gum_resolve_relative_indirect_ptr (const GumRelativeIndirectPtr * delta)
{
  GumRelativeIndirectablePtr val = *delta;
  gconstpointer * target;

  target = (gconstpointer *) ((const guint8 *) delta + val);

  return gum_strip_code_pointer ((gpointer) *target);
}

static gconstpointer
gum_resolve_relative_indirectable_ptr (const GumRelativeIndirectablePtr * delta)
{
  GumRelativeIndirectablePtr val = *delta;
  gconstpointer * target;

  if ((val & 1) == 0)
    return gum_resolve_relative_direct_ptr (delta);

  target = (gconstpointer *) ((const guint8 *) delta + (val & ~1));

  return gum_strip_code_pointer ((gpointer) *target);
}

static gchar *
gum_demangle (const gchar * name)
{
  gchar buf[512];
  gsize n, capacity;
  gchar * dbuf;

  n = gum_demangle_impl (name, buf, sizeof (buf));
  if (n == 0)
    return NULL;

  if (n < sizeof (buf))
    return g_strdup (buf);

  capacity = n + 1;
  dbuf = g_malloc (capacity);
  gum_demangle_impl (name, dbuf, capacity);

  return dbuf;
}
