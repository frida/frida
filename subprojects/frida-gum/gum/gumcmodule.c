/*
 * Copyright (C) 2019-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2025 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumcmodule.h"

#include <stdio.h>
#include <string.h>
#include <gio/gio.h>
#include <gum/gum-init.h>
#include <gum/gum.h>
#include <json-glib/json-glib.h>
#ifdef HAVE_DARWIN
# include <gum/gumdarwinmapper.h>
#endif

#ifdef HAVE_TINYCC
static GumCModule * gum_tcc_cmodule_new (const gchar * source,
    const GumCModuleOptions * options, GError ** error);
#endif
#ifndef G_OS_NONE
static GumCModule * gum_gcc_cmodule_new (const gchar * source, GBytes * binary,
    const GumCModuleOptions * options, GError ** error) G_GNUC_UNUSED;
#endif
#ifdef HAVE_DARWIN
static GumCModule * gum_darwin_cmodule_new (const gchar * source,
    GBytes * binary, const GumCModuleOptions * options, GError ** error);
#endif

typedef struct _GumCModulePrivate GumCModulePrivate;

typedef void (* GumCModuleInitFunc) (void);
typedef void (* GumCModuleFinalizeFunc) (void);
typedef void (* GumCModuleDestructFunc) (void);

struct _GumCModulePrivate
{
  GumMemoryRange range;
  GumCModuleFinalizeFunc finalize;
  GumCModuleDestructFunc destruct;
};

static void gum_cmodule_finalize (GObject * object);
static void gum_emit_standard_define (const GumCDefineDetails * details,
    gpointer user_data);
static void gum_cmodule_add_define (GumCModule * self, const gchar * name,
    const gchar * value);
static gboolean gum_cmodule_link_pre (GumCModule * self, gsize * size,
    GString ** error_messages);
static gboolean gum_cmodule_link_at (GumCModule * self, gpointer base,
    GString ** error_messages);
static void gum_cmodule_link_post (GumCModule * self);
static void gum_emit_builtin_define (const gchar * name, const gchar * value,
    GumFoundCDefineFunc func, gpointer user_data);
static void gum_emit_builtin_define_str (const gchar * name,
    const gchar * value, GumFoundCDefineFunc func, gpointer user_data);

#ifndef G_OS_NONE
static void gum_csymbol_details_destroy (GumCSymbolDetails * details);

static gboolean gum_populate_include_dir (const gchar * path, GError ** error);
static void gum_rmtree (GFile * file);
static gboolean gum_call_tool (const gchar * cwd, const gchar * const * argv,
    gchar ** output, gint * exit_status, GError ** error);
#endif
static void gum_append_error (GString ** messages, const char * msg);

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GumCModule, gum_cmodule, G_TYPE_OBJECT);

#include "gumcmodule-runtime.h"

static void
gum_cmodule_class_init (GumCModuleClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gum_cmodule_finalize;
}

static void
gum_cmodule_init (GumCModule * cmodule)
{
}

static void
gum_cmodule_finalize (GObject * object)
{
  GumCModule * self;
  GumCModulePrivate * priv;
  const GumMemoryRange * r;

  self = GUM_CMODULE (object);
  priv = gum_cmodule_get_instance_private (self);
  r = &priv->range;

  if (r->base_address != 0)
  {
    if (priv->finalize != NULL)
      priv->finalize ();

    if (priv->destruct != NULL)
      priv->destruct ();

    gum_cloak_remove_range (r);

    gum_memory_free (GSIZE_TO_POINTER (r->base_address), r->size);
  }

  gum_cmodule_drop_metadata (self);

  G_OBJECT_CLASS (gum_cmodule_parent_class)->finalize (object);
}

GumCModule *
gum_cmodule_new (const gchar * source,
                 GBytes * binary,
                 const GumCModuleOptions * options,
                 GError ** error)
{
  GumCModuleToolchain toolchain = options->toolchain;

  if (toolchain == GUM_CMODULE_TOOLCHAIN_ANY)
  {
#ifdef HAVE_TINYCC
    toolchain = GUM_CMODULE_TOOLCHAIN_INTERNAL;
#else
    toolchain = GUM_CMODULE_TOOLCHAIN_EXTERNAL;
#endif
  }

  if (binary != NULL)
    toolchain = GUM_CMODULE_TOOLCHAIN_EXTERNAL;

  switch (toolchain)
  {
    case GUM_CMODULE_TOOLCHAIN_INTERNAL:
#ifdef HAVE_TINYCC
      return gum_tcc_cmodule_new (source, options, error);
#else
      g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_SUPPORTED,
          "Internal toolchain is not available in this build configuration");
      return NULL;
#endif
    case GUM_CMODULE_TOOLCHAIN_EXTERNAL:
#ifdef HAVE_DARWIN
      return gum_darwin_cmodule_new (source, binary, options, error);
#elif !defined (G_OS_NONE)
      return gum_gcc_cmodule_new (source, binary, options, error);
#else
      g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_SUPPORTED,
          "External toolchain is not supported by the Barebone backend");
      return NULL;
#endif
    default:
      g_assert_not_reached ();
  }
}

const GumMemoryRange *
gum_cmodule_get_range (GumCModule * self)
{
  GumCModulePrivate * priv = gum_cmodule_get_instance_private (self);

  return &priv->range;
}

static void
gum_cmodule_add_standard_defines (GumCModule * self)
{
  gum_cmodule_enumerate_builtin_defines (gum_emit_standard_define, self);
}

static void
gum_emit_standard_define (const GumCDefineDetails * details,
                          gpointer user_data)
{
  GumCModule * self = user_data;

  gum_cmodule_add_define (self, details->name, details->value);
}

static void
gum_cmodule_add_define (GumCModule * self,
                        const gchar * name,
                        const gchar * value)
{
  GUM_CMODULE_GET_CLASS (self)->add_define (self, name, value);
}

void
gum_cmodule_add_symbol (GumCModule * self,
                        const gchar * name,
                        gconstpointer value)
{
  GUM_CMODULE_GET_CLASS (self)->add_symbol (self, name, value);
}

gboolean
gum_cmodule_link (GumCModule * self,
                  GError ** error)
{
  gboolean success = FALSE;
  GumCModulePrivate * priv;
  GString * error_messages;
  gsize size, page_size;
  gboolean remap_supported;
  GumPageProtection protection;
  gpointer base;

  priv = gum_cmodule_get_instance_private (self);

  error_messages = NULL;
  if (!gum_cmodule_link_pre (self, &size, &error_messages))
    goto beach;

  page_size = gum_query_page_size ();
  size = GUM_ALIGN_SIZE (size, page_size);

  remap_supported = gum_memory_can_remap_writable ();
  protection = remap_supported ? GUM_PAGE_RX : GUM_PAGE_RW;

  base = gum_memory_allocate (NULL, size, page_size, protection);

  if (gum_cmodule_link_at (self, base, &error_messages))
  {
    GumMemoryRange * r = &priv->range;
    GumCModuleInitFunc init;

    r->base_address = GUM_ADDRESS (base);
    r->size = size;

    gum_cloak_add_range (r);

    init = GUM_POINTER_TO_FUNCPTR (GumCModuleInitFunc,
        gum_cmodule_find_symbol_by_name (self, "init"));
    if (init != NULL)
      init ();

    priv->finalize = GUM_POINTER_TO_FUNCPTR (GumCModuleFinalizeFunc,
        gum_cmodule_find_symbol_by_name (self, "finalize"));

    success = TRUE;
  }
  else
  {
    gum_memory_free (base, size);
  }

beach:
  gum_cmodule_link_post (self);

  if (error_messages != NULL)
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT,
        "Linking failed: %s", error_messages->str);
    g_string_free (error_messages, TRUE);
  }

  return success;
}

static gboolean
gum_cmodule_link_pre (GumCModule * self,
                      gsize * size,
                      GString ** error_messages)
{
  return GUM_CMODULE_GET_CLASS (self)->link_pre (self, size, error_messages);
}

static gboolean
gum_cmodule_link_at (GumCModule * self,
                     gpointer base,
                     GString ** error_messages)
{
  return GUM_CMODULE_GET_CLASS (self)->link_at (self, base, error_messages);
}

static void
gum_cmodule_link_post (GumCModule * self)
{
  GUM_CMODULE_GET_CLASS (self)->link_post (self);
}

void
gum_cmodule_enumerate_builtin_defines (GumFoundCDefineFunc func,
                                       gpointer user_data)
{
#if defined (HAVE_I386)
  gum_emit_builtin_define ("HAVE_I386", NULL, func, user_data);
#elif defined (HAVE_ARM)
  gum_emit_builtin_define ("HAVE_ARM", NULL, func, user_data);
#elif defined (HAVE_ARM64)
  gum_emit_builtin_define ("HAVE_ARM64", NULL, func, user_data);
#elif defined (HAVE_MIPS)
  gum_emit_builtin_define ("HAVE_MIPS", NULL, func, user_data);
#endif

  gum_emit_builtin_define_str ("G_GINT16_MODIFIER", G_GINT16_MODIFIER,
      func, user_data);
  gum_emit_builtin_define_str ("G_GINT32_MODIFIER", G_GINT32_MODIFIER,
      func, user_data);
  gum_emit_builtin_define_str ("G_GINT64_MODIFIER", G_GINT64_MODIFIER,
      func, user_data);
  gum_emit_builtin_define_str ("G_GSIZE_MODIFIER", G_GSIZE_MODIFIER,
      func, user_data);
  gum_emit_builtin_define_str ("G_GSSIZE_MODIFIER", G_GSSIZE_MODIFIER,
      func, user_data);

  gum_emit_builtin_define ("GLIB_SIZEOF_VOID_P",
      G_STRINGIFY (GLIB_SIZEOF_VOID_P), func, user_data);
}

static void
gum_emit_builtin_define (const gchar * name,
                         const gchar * value,
                         GumFoundCDefineFunc func,
                         gpointer user_data)
{
  GumCDefineDetails d = { name, value };

  func (&d, user_data);
}

static void
gum_emit_builtin_define_str (const gchar * name,
                             const gchar * value,
                             GumFoundCDefineFunc func,
                             gpointer user_data)
{
  gchar * raw_value;

  raw_value = g_strconcat ("\"", value, "\"", NULL);

  gum_emit_builtin_define (name, raw_value, func, user_data);

  g_free (raw_value);
}

void
gum_cmodule_enumerate_builtin_headers (GumFoundCHeaderFunc func,
                                       gpointer user_data)
{
  guint i;

  for (i = 0; i != G_N_ELEMENTS (gum_cmodule_headers); i++)
  {
    const GumCHeaderDetails * h = &gum_cmodule_headers[i];

    func (h, user_data);
  }
}

void
gum_cmodule_enumerate_symbols (GumCModule * self,
                               GumFoundCSymbolFunc func,
                               gpointer user_data)
{
  GUM_CMODULE_GET_CLASS (self)->enumerate_symbols (self, func, user_data);
}

gpointer
gum_cmodule_find_symbol_by_name (GumCModule * self,
                                 const gchar * name)
{
  return GUM_CMODULE_GET_CLASS (self)->find_symbol_by_name (self, name);
}

void
gum_cmodule_drop_metadata (GumCModule * self)
{
  GUM_CMODULE_GET_CLASS (self)->drop_metadata (self);
}

#ifdef HAVE_TINYCC

#include <libtcc.h>

#define GUM_TYPE_TCC_CMODULE (gum_tcc_cmodule_get_type ())
G_DECLARE_FINAL_TYPE (GumTccCModule, gum_tcc_cmodule, GUM, TCC_CMODULE,
    GumCModule)

typedef struct _GumTccApplyContext GumTccApplyContext;
typedef struct _GumEnumerateSymbolsContext GumEnumerateSymbolsContext;

struct _GumTccCModule
{
  GumCModule parent;

  TCCState * state;
  gsize size;
};

struct _GumTccApplyContext
{
  GumTccCModule * module;
  gpointer executable_base;
  gboolean success;
};

struct _GumEnumerateSymbolsContext
{
  GumFoundCSymbolFunc func;
  gpointer user_data;
};

static void gum_tcc_cmodule_add_define (GumCModule * cm, const gchar * name,
    const gchar * value);
static void gum_tcc_cmodule_add_symbol (GumCModule * cm, const gchar * name,
    gconstpointer value);
static gboolean gum_tcc_cmodule_link_pre (GumCModule * cm, gsize * size,
    GString ** error_messages);
static gboolean gum_tcc_cmodule_link_at (GumCModule * cm, gpointer base,
    GString ** error_messages);
static void gum_tcc_cmodule_relocate_views (gpointer writable_base,
    gpointer user_data);
static void gum_tcc_cmodule_link_post (GumCModule * cm);
static void gum_tcc_cmodule_enumerate_symbols (GumCModule * cm,
    GumFoundCSymbolFunc func, gpointer user_data);
static gpointer gum_tcc_cmodule_find_symbol_by_name (GumCModule * cm,
    const gchar * name);
static void gum_tcc_cmodule_drop_metadata (GumCModule * cm);
static void gum_emit_symbol (void * ctx, const char * name, const void * val);
static void gum_append_tcc_error (void * opaque, const char * msg);
static void gum_emit_symbol (void * ctx, const char * name, const void * val);
static const char * gum_tcc_cmodule_load_header (void * opaque,
    const char * path, int * len);
static void * gum_tcc_cmodule_resolve_symbol (void * opaque, const char * name);

static void gum_add_abi_symbols (TCCState * state);
static const gchar * gum_undecorate_name (const gchar * name);

G_DEFINE_TYPE (GumTccCModule, gum_tcc_cmodule, GUM_TYPE_CMODULE)

static void
gum_tcc_cmodule_class_init (GumTccCModuleClass * klass)
{
  GumCModuleClass * cmodule_class = GUM_CMODULE_CLASS (klass);

  cmodule_class->add_define = gum_tcc_cmodule_add_define;
  cmodule_class->add_symbol = gum_tcc_cmodule_add_symbol;
  cmodule_class->link_pre = gum_tcc_cmodule_link_pre;
  cmodule_class->link_at = gum_tcc_cmodule_link_at;
  cmodule_class->link_post = gum_tcc_cmodule_link_post;
  cmodule_class->enumerate_symbols = gum_tcc_cmodule_enumerate_symbols;
  cmodule_class->find_symbol_by_name = gum_tcc_cmodule_find_symbol_by_name;
  cmodule_class->drop_metadata = gum_tcc_cmodule_drop_metadata;
}

static void
gum_tcc_cmodule_init (GumTccCModule * cmodule)
{
}

static GumCModule *
gum_tcc_cmodule_new (const gchar * source,
                     const GumCModuleOptions * options,
                     GError ** error)
{
  GumCModule * result;
  GumTccCModule * cmodule;
  TCCState * state;
  GString * error_messages;
  gchar * combined_source;

  result = g_object_new (GUM_TYPE_TCC_CMODULE, NULL);
  cmodule = GUM_TCC_CMODULE (result);

  state = tcc_new ();
  cmodule->state = state;

  error_messages = NULL;
  tcc_set_error_func (state, &error_messages, gum_append_tcc_error);

  tcc_set_cpp_load_func (state, cmodule, gum_tcc_cmodule_load_header);
  tcc_set_linker_resolve_func (state, cmodule, gum_tcc_cmodule_resolve_symbol);
  tcc_set_options (state,
      "-Wall "
      "-Werror "
      "-isystem /frida "
      "-isystem /frida/capstone "
      "-nostdinc "
      "-nostdlib"
  );

  gum_cmodule_add_standard_defines (result);
#ifdef HAVE_WINDOWS
  gum_cmodule_add_define (result, "extern", "__attribute__ ((dllimport))");
#endif

  tcc_set_output_type (state, TCC_OUTPUT_MEMORY);

  combined_source = g_strconcat ("#line 1 \"module.c\"\n", source, NULL);

  tcc_compile_string (state, combined_source);

  g_free (combined_source);

  tcc_set_error_func (state, NULL, NULL);

  if (error_messages != NULL)
    goto propagate_error;

  gum_add_abi_symbols (state);

  return result;

propagate_error:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT,
        "Compilation failed: %s", error_messages->str);
    g_string_free (error_messages, TRUE);

    g_object_unref (result);

    return NULL;
  }
}

static void
gum_tcc_cmodule_add_define (GumCModule * cm,
                            const gchar * name,
                            const gchar * value)
{
  GumTccCModule * self = GUM_TCC_CMODULE (cm);

  tcc_define_symbol (self->state, name, value);
}

static void
gum_tcc_cmodule_add_symbol (GumCModule * cm,
                            const gchar * name,
                            gconstpointer value)
{
  GumTccCModule * self = GUM_TCC_CMODULE (cm);

  tcc_add_symbol (self->state, name, value);
}

static gboolean
gum_tcc_cmodule_link_pre (GumCModule * cm,
                          gsize * size,
                          GString ** error_messages)
{
  GumTccCModule * self = GUM_TCC_CMODULE (cm);
  TCCState * state = self->state;
  int res;

  tcc_set_error_func (state, error_messages, gum_append_tcc_error);

  res = tcc_relocate (state, NULL);
  if (res == -1)
    return FALSE;

  self->size = res;

  *size = res;
  return TRUE;
}

static gboolean
gum_tcc_cmodule_link_at (GumCModule * cm,
                         gpointer base,
                         GString ** error_messages)
{
  GumTccCModule * self = GUM_TCC_CMODULE (cm);

  if (gum_memory_can_remap_writable ())
  {
    GumTccApplyContext ctx;

    ctx.module = self;
    ctx.executable_base = base;
    ctx.success = FALSE;

    gum_memory_patch_code (base, self->size, gum_tcc_cmodule_relocate_views,
        &ctx);

    return ctx.success;
  }
  else
  {
    if (tcc_relocate (self->state, base) == -1)
      return FALSE;

    gum_memory_mark_code (base, self->size);

    return TRUE;
  }
}

static void
gum_tcc_cmodule_relocate_views (gpointer writable_base,
                                gpointer user_data)
{
  GumTccApplyContext * ctx = user_data;
  size_t diff_to_exec =
      (guint8 *) ctx->executable_base - (guint8 *) writable_base;

  ctx->success =
      tcc_relocate_ex (ctx->module->state, writable_base, diff_to_exec) != -1;
}

static void
gum_tcc_cmodule_link_post (GumCModule * cm)
{
  GumTccCModule * self = GUM_TCC_CMODULE (cm);

  tcc_set_error_func (self->state, NULL, NULL);
}

static void
gum_append_tcc_error (void * opaque,
                      const char * msg)
{
  GString ** messages = opaque;

  gum_append_error (messages, msg);
}

static void
gum_tcc_cmodule_enumerate_symbols (GumCModule * cm,
                                   GumFoundCSymbolFunc func,
                                   gpointer user_data)
{
  GumTccCModule * self = GUM_TCC_CMODULE (cm);
  GumEnumerateSymbolsContext ctx;

  ctx.func = func;
  ctx.user_data = user_data;

  tcc_list_symbols (self->state, &ctx, gum_emit_symbol);
}

static void
gum_emit_symbol (void * ctx,
                 const char * name,
                 const void * val)
{
  GumEnumerateSymbolsContext * sc = ctx;
  GumCSymbolDetails d;

  d.name = gum_undecorate_name (name);
  d.address = (gpointer) val;

  sc->func (&d, sc->user_data);
}

static gpointer
gum_tcc_cmodule_find_symbol_by_name (GumCModule * cm,
                                     const gchar * name)
{
  GumTccCModule * self = GUM_TCC_CMODULE (cm);

  return tcc_get_symbol (self->state, name);
}

static void
gum_tcc_cmodule_drop_metadata (GumCModule * cm)
{
  GumTccCModule * self = GUM_TCC_CMODULE (cm);

  g_clear_pointer (&self->state, tcc_delete);
}

static const char *
gum_tcc_cmodule_load_header (void * opaque,
                             const char * path,
                             int * len)
{
  const gchar * name;
  guint i;

  name = path;
  if (g_str_has_prefix (name, "/frida/"))
    name += 7;

  for (i = 0; i != G_N_ELEMENTS (gum_cmodule_headers); i++)
  {
    const GumCHeaderDetails * h = &gum_cmodule_headers[i];
    if (strcmp (h->name, name) == 0)
    {
      *len = h->size;
      return h->data;
    }
  }

  return NULL;
}

static void *
gum_tcc_cmodule_resolve_symbol (void * opaque,
                                const char * name)
{
  return g_hash_table_lookup (gum_cmodule_get_symbols (),
      gum_undecorate_name (name));
}

#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4

static long long gum_divdi3 (long long a, long long b);
static long long gum_moddi3 (long long a, long long b);
static long gum_fixdfdi (double value);

static void
gum_add_abi_symbols (TCCState * state)
{
  tcc_add_symbol (state, "__divdi3", GUM_FUNCPTR_TO_POINTER (gum_divdi3));
  tcc_add_symbol (state, "__moddi3", GUM_FUNCPTR_TO_POINTER (gum_moddi3));
  tcc_add_symbol (state, "__fixdfdi", GUM_FUNCPTR_TO_POINTER (gum_fixdfdi));
}

static long long
gum_divdi3 (long long a,
            long long b)
{
  return a / b;
}

static long long
gum_moddi3 (long long a,
            long long b)
{
  return a % b;
}

static long
gum_fixdfdi (double value)
{
  return value;
}

#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8 && \
    !defined (_MSC_VER) && !defined (__MINGW32__)

extern void * __va_arg (void * ap, int arg_type, int size, int align);

static void
gum_add_abi_symbols (TCCState * state)
{
  tcc_add_symbol (state, "__va_arg", __va_arg);
}

#elif defined (HAVE_ARM)

#define GUM_DECLARE_HELPER(name) \
    extern void __aeabi_ ## name (void);
#define GUM_DECLARE_HELPER_FALLBACK(name, ...) \
    static void gum_aeabi_ ## name (__VA_ARGS__);
#define GUM_REGISTER_HELPER(name) \
    tcc_add_symbol (state, G_STRINGIFY (__aeabi_ ## name), __aeabi_ ## name)
#define GUM_REGISTER_HELPER_FALLBACK(name) \
    GUM_REGISTER_HELPER_FALLBACK_ALIASED (name, name)
#define GUM_REGISTER_HELPER_FALLBACK_ALIASED(name, impl) \
    tcc_add_symbol (state, G_STRINGIFY (__aeabi_ ## name), gum_aeabi_ ## impl)

#ifdef HAVE_AEABI_MEMORY_BUILTINS
GUM_DECLARE_HELPER (memmove)
GUM_DECLARE_HELPER (memmove4)
GUM_DECLARE_HELPER (memmove8)
GUM_DECLARE_HELPER (memset)
#else
GUM_DECLARE_HELPER_FALLBACK (memmove, void *, const void *, size_t)
GUM_DECLARE_HELPER_FALLBACK (memset, void *, size_t, int)
#endif
#ifndef HAVE_DARWIN
GUM_DECLARE_HELPER (f2ulz)
GUM_DECLARE_HELPER (f2lz)
GUM_DECLARE_HELPER (d2ulz)
GUM_DECLARE_HELPER (d2lz)
GUM_DECLARE_HELPER (ul2f)
GUM_DECLARE_HELPER (l2f)
GUM_DECLARE_HELPER (ul2d)
GUM_DECLARE_HELPER (l2d)
GUM_DECLARE_HELPER (ldivmod)
GUM_DECLARE_HELPER (uldivmod)
GUM_DECLARE_HELPER (llsl)
GUM_DECLARE_HELPER (llsr)
GUM_DECLARE_HELPER (lasr)
GUM_DECLARE_HELPER (idiv)
GUM_DECLARE_HELPER (uidiv)
GUM_DECLARE_HELPER (idivmod)
GUM_DECLARE_HELPER (uidivmod)
#endif

static void
gum_add_abi_symbols (TCCState * state)
{
#ifdef HAVE_AEABI_MEMORY_BUILTINS
  GUM_REGISTER_HELPER (memmove);
  GUM_REGISTER_HELPER (memmove4);
  GUM_REGISTER_HELPER (memmove8);
  GUM_REGISTER_HELPER (memset);
#else
  GUM_REGISTER_HELPER_FALLBACK (memmove);
  GUM_REGISTER_HELPER_FALLBACK_ALIASED (memmove4, memmove);
  GUM_REGISTER_HELPER_FALLBACK_ALIASED (memmove8, memmove);
  GUM_REGISTER_HELPER_FALLBACK (memset);
#endif
#ifndef HAVE_DARWIN
  GUM_REGISTER_HELPER (f2ulz);
  GUM_REGISTER_HELPER (f2lz);
  GUM_REGISTER_HELPER (d2ulz);
  GUM_REGISTER_HELPER (d2lz);
  GUM_REGISTER_HELPER (ul2f);
  GUM_REGISTER_HELPER (l2f);
  GUM_REGISTER_HELPER (ul2d);
  GUM_REGISTER_HELPER (l2d);
  GUM_REGISTER_HELPER (ldivmod);
  GUM_REGISTER_HELPER (uldivmod);
  GUM_REGISTER_HELPER (llsl);
  GUM_REGISTER_HELPER (llsr);
  GUM_REGISTER_HELPER (lasr);
  GUM_REGISTER_HELPER (idiv);
  GUM_REGISTER_HELPER (uidiv);
  GUM_REGISTER_HELPER (idivmod);
  GUM_REGISTER_HELPER (uidivmod);
#endif
}

#ifndef HAVE_AEABI_MEMORY_BUILTINS

static void
gum_aeabi_memmove (void * dst,
                   const void * src,
                   size_t n)
{
  memmove (dst, src, n);
}

static void
gum_aeabi_memset (void * s,
                  size_t n,
                  int c)
{
  memset (s, c, n);
}

#endif

#else

static void
gum_add_abi_symbols (TCCState * state)
{
}

#endif

static const gchar *
gum_undecorate_name (const gchar * name)
{
#ifdef HAVE_DARWIN
  return name + 1;
#else
  return name;
#endif
}

#endif /* HAVE_TINYCC */

#ifndef G_OS_NONE

#define GUM_TYPE_GCC_CMODULE (gum_gcc_cmodule_get_type ())
G_DECLARE_FINAL_TYPE (GumGccCModule, gum_gcc_cmodule, GUM, GCC_CMODULE,
    GumCModule)

typedef struct _GumLdsPrinter GumLdsPrinter;

struct _GumGccCModule
{
  GumCModule parent;

  gchar * workdir;
  GPtrArray * argv;
  GArray * symbols;
};

struct _GumLdsPrinter
{
  FILE * file;
  gpointer base;
};

static void gum_gcc_cmodule_add_define (GumCModule * cm, const gchar * name,
    const gchar * value);
static void gum_gcc_cmodule_add_symbol (GumCModule * cm, const gchar * name,
    gconstpointer value);
static gboolean gum_gcc_cmodule_link_pre (GumCModule * cm, gsize * size,
    GString ** error_messages);
static gboolean gum_gcc_cmodule_link_at (GumCModule * cm, gpointer base,
    GString ** error_messages);
static void gum_gcc_cmodule_link_post (GumCModule * cm);
static gboolean gum_gcc_cmodule_do_link (GumCModule * cm, gpointer base,
    gpointer * contents, gsize * size, GString ** error_messages);
static gboolean gum_gcc_cmodule_call_ld (GumGccCModule * self, gpointer base,
    GError ** error);
static gboolean gum_gcc_cmodule_call_objcopy (GumGccCModule * self,
    GError ** error);
static void gum_write_linker_script (FILE * file, gpointer base,
    GHashTable * api_symbols, GArray * user_symbols);
static void gum_print_lds_assignment (gpointer key, gpointer value,
    gpointer user_data);
static void gum_gcc_cmodule_enumerate_symbols (GumCModule * cm,
    GumFoundCSymbolFunc func, gpointer user_data);
static gpointer gum_gcc_cmodule_find_symbol_by_name (GumCModule * cm,
    const gchar * name);
static void gum_store_address_if_name_matches (
    const GumCSymbolDetails * details, gpointer user_data);
static void gum_gcc_cmodule_drop_metadata (GumCModule * cm);

G_DEFINE_TYPE (GumGccCModule, gum_gcc_cmodule, GUM_TYPE_CMODULE)

static void
gum_gcc_cmodule_class_init (GumGccCModuleClass * klass)
{
  GumCModuleClass * cmodule_class = GUM_CMODULE_CLASS (klass);

  cmodule_class->add_define = gum_gcc_cmodule_add_define;
  cmodule_class->add_symbol = gum_gcc_cmodule_add_symbol;
  cmodule_class->link_pre = gum_gcc_cmodule_link_pre;
  cmodule_class->link_at = gum_gcc_cmodule_link_at;
  cmodule_class->link_post = gum_gcc_cmodule_link_post;
  cmodule_class->enumerate_symbols = gum_gcc_cmodule_enumerate_symbols;
  cmodule_class->find_symbol_by_name = gum_gcc_cmodule_find_symbol_by_name;
  cmodule_class->drop_metadata = gum_gcc_cmodule_drop_metadata;
}

static void
gum_gcc_cmodule_init (GumGccCModule * self)
{
  self->argv = g_ptr_array_new_with_free_func (g_free);

  self->symbols = g_array_new (FALSE, FALSE, sizeof (GumCSymbolDetails));
  g_array_set_clear_func (self->symbols,
      (GDestroyNotify) gum_csymbol_details_destroy);
}

static GumCModule *
gum_gcc_cmodule_new (const gchar * source,
                     GBytes * binary,
                     const GumCModuleOptions * options,
                     GError ** error)
{
  GumCModule * result = NULL;
  GumGccCModule * cmodule;
  gboolean success = FALSE;
  gchar * source_path = NULL;
  gchar * output = NULL;
  gint exit_status;

  if (binary != NULL)
    goto binary_loading_unsupported;

  result = g_object_new (GUM_TYPE_GCC_CMODULE, NULL);
  cmodule = GUM_GCC_CMODULE (result);

  cmodule->workdir = g_dir_make_tmp ("cmodule-XXXXXX", error);
  if (cmodule->workdir == NULL)
    goto beach;

  source_path = g_build_filename (cmodule->workdir, "module.c", NULL);

  if (!g_file_set_contents (source_path, source, -1, error))
    goto beach;

  if (!gum_populate_include_dir (cmodule->workdir, error))
    goto beach;

  g_ptr_array_add (cmodule->argv, g_strdup ("gcc"));
  g_ptr_array_add (cmodule->argv, g_strdup ("-c"));
  g_ptr_array_add (cmodule->argv, g_strdup ("-Wall"));
  g_ptr_array_add (cmodule->argv, g_strdup ("-Werror"));
  g_ptr_array_add (cmodule->argv, g_strdup ("-O2"));
  g_ptr_array_add (cmodule->argv, g_strdup ("-fno-pic"));
#ifdef HAVE_I386
  g_ptr_array_add (cmodule->argv, g_strdup ("-mcmodel=large"));
#endif
  g_ptr_array_add (cmodule->argv, g_strdup ("-nostdlib"));
  g_ptr_array_add (cmodule->argv, g_strdup ("-isystem"));
  g_ptr_array_add (cmodule->argv, g_strdup ("."));
  g_ptr_array_add (cmodule->argv, g_strdup ("-isystem"));
  g_ptr_array_add (cmodule->argv, g_strdup ("capstone"));
  gum_cmodule_add_standard_defines (result);
  g_ptr_array_add (cmodule->argv, g_strdup ("module.c"));
  g_ptr_array_add (cmodule->argv, NULL);

  if (!gum_call_tool (cmodule->workdir,
      (const gchar * const *) cmodule->argv->pdata, &output, &exit_status,
      error))
  {
    goto beach;
  }

  if (exit_status != 0)
    goto compilation_failed;

  success = TRUE;
  goto beach;

binary_loading_unsupported:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_SUPPORTED,
        "Binary loading is not yet supported on this platform");
    goto beach;
  }
compilation_failed:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT,
        "Compilation failed: %s", output);
    goto beach;
  }
beach:
  {
    g_free (output);
    g_free (source_path);
    if (!success)
      g_clear_object (&result);

    return result;
  }
}

static void
gum_gcc_cmodule_add_define (GumCModule * cm,
                            const gchar * name,
                            const gchar * value)
{
  GumGccCModule * self = GUM_GCC_CMODULE (cm);
  gchar * arg;

  arg = (value == NULL)
      ? g_strconcat ("-D", name, NULL)
      : g_strconcat ("-D", name, "=", value, NULL);

  g_ptr_array_add (self->argv, arg);
}

static void
gum_gcc_cmodule_add_symbol (GumCModule * cm,
                            const gchar * name,
                            gconstpointer value)
{
  GumGccCModule * self = GUM_GCC_CMODULE (cm);
  GumCSymbolDetails d;

  d.name = g_strdup (name);
  d.address = (gpointer) value;

  g_array_append_val (self->symbols, d);
}

static gboolean
gum_gcc_cmodule_link_pre (GumCModule * cm,
                          gsize * size,
                          GString ** error_messages)
{
  gpointer contents;

  if (!gum_gcc_cmodule_do_link (cm, 0, &contents, size, error_messages))
    return FALSE;

  g_free (contents);

  return TRUE;
}

static gboolean
gum_gcc_cmodule_link_at (GumCModule * cm,
                         gpointer base,
                         GString ** error_messages)
{
  gpointer contents;
  gsize size;

  if (!gum_gcc_cmodule_do_link (cm, base, &contents, &size, error_messages))
    return FALSE;

  memcpy (base, contents, size);
  gum_memory_mark_code (base, size);

  g_free (contents);

  return TRUE;
}

static void
gum_gcc_cmodule_link_post (GumCModule * cm)
{
}

static gboolean
gum_gcc_cmodule_do_link (GumCModule * cm,
                         gpointer base,
                         gpointer * contents,
                         gsize * size,
                         GString ** error_messages)
{
  GumGccCModule * self = GUM_GCC_CMODULE (cm);
  gboolean success = FALSE;
  GError * error = NULL;
  gchar * module_path = NULL;

  if (!gum_gcc_cmodule_call_ld (self, base, &error))
    goto propagate_error;

  if (!gum_gcc_cmodule_call_objcopy (self, &error))
    goto propagate_error;

  module_path = g_build_filename (self->workdir, "module", NULL);

  if (!g_file_get_contents (module_path, (gchar **) contents, size, &error))
    goto propagate_error;

  success = TRUE;
  goto beach;

propagate_error:
  {
    gum_append_error (error_messages, error->message);
    g_error_free (error);
    goto beach;
  }
beach:
  {
    g_free (module_path);

    return success;
  }
}

static gboolean
gum_gcc_cmodule_call_ld (GumGccCModule * self,
                         gpointer base,
                         GError ** error)
{
  gboolean success = FALSE;
  gchar * linker_script_path;
  FILE * file;
  const gchar * argv[] = {
    "gcc",
    "-nostdlib",
    "-Wl,--build-id=none",
    "-Wl,--script=module.lds",
    "module.o",
    NULL
  };
  gchar * output = NULL;
  gint exit_status;

  linker_script_path = g_build_filename (self->workdir, "module.lds", NULL);

  file = fopen (linker_script_path, "w");
  if (file == NULL)
    goto fopen_failed;
  gum_write_linker_script (file, base, gum_cmodule_get_symbols (),
      self->symbols);
  fclose (file);

  if (!gum_call_tool (self->workdir, argv, &output, &exit_status, error))
    goto beach;

  if (exit_status != 0)
    goto ld_failed;

  success = TRUE;
  goto beach;

fopen_failed:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT,
        "Failed to create %s", linker_script_path);
    goto beach;
  }
ld_failed:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT,
        "ld failed: %s", output);
    goto beach;
  }
beach:
  {
    g_free (output);
    g_free (linker_script_path);

    return success;
  }
}

static gboolean
gum_gcc_cmodule_call_objcopy (GumGccCModule * self,
                              GError ** error)
{
  gboolean success = FALSE;
  const gchar * argv[] = {
    "objcopy",
    "-O", "binary",
    "--only-section=.frida",
    "a.out",
    "module",
    NULL
  };
  gchar * output;
  gint exit_status;

  if (!gum_call_tool (self->workdir, argv, &output, &exit_status, error))
    return FALSE;

  if (exit_status != 0)
    goto objcopy_failed;

  success = TRUE;
  goto beach;

objcopy_failed:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT,
        "objcopy failed: %s", output);
    goto beach;
  }
beach:
  {
    g_free (output);

    return success;
  }
}

static void
gum_write_linker_script (FILE * file,
                         gpointer base,
                         GHashTable * api_symbols,
                         GArray * user_symbols)
{
  GumLdsPrinter printer = {
    .file = file,
    .base = base,
  };
  guint i;

  g_hash_table_foreach (api_symbols, gum_print_lds_assignment, &printer);

  for (i = 0; i != user_symbols->len; i++)
  {
    GumCSymbolDetails * d = &g_array_index (user_symbols, GumCSymbolDetails, i);

    gum_print_lds_assignment ((gpointer) d->name, d->address, &printer);
  }

  fprintf (printer.file,
      "SECTIONS {\n"
      "  .frida 0x%zx: {\n"
      "    *(.text*)\n"
      "    *(.data)\n"
      "    *(.bss)\n"
      "    *(COMMON)\n"
      "    *(.rodata*)\n"
      "  }\n"
      "  /DISCARD/ : { *(*) }\n"
      "}\n",
      GPOINTER_TO_SIZE (base));
}

static void
gum_print_lds_assignment (gpointer key,
                          gpointer value,
                          gpointer user_data)
{
  GumLdsPrinter * printer = user_data;

  fprintf (printer->file, "%s = 0x%zx;\n",
      (gchar *) key,
      (printer->base != NULL) ? GPOINTER_TO_SIZE (value) : 0);
}

static void
gum_gcc_cmodule_enumerate_symbols (GumCModule * cm,
                                   GumFoundCSymbolFunc func,
                                   gpointer user_data)
{
  GumGccCModule * self = GUM_GCC_CMODULE (cm);
  const gchar * argv[] = { "nm", "a.out", NULL };
  gchar * output = NULL;
  gint exit_status;
  gchar * line_start;

  if (!gum_call_tool (self->workdir, argv, &output, &exit_status, NULL))
    goto beach;

  if (exit_status != 0)
    goto beach;

  line_start = output;
  while (TRUE)
  {
    gchar * line_end;
    guint64 address;
    gchar * endptr;

    line_end = strchr (line_start, '\n');
    if (line_end == NULL)
      break;
    *line_end = '\0';

    address = g_ascii_strtoull (line_start, &endptr, 16);
    if (endptr != line_start)
    {
      GumCSymbolDetails d;

      d.address = GSIZE_TO_POINTER (address);
      d.name = endptr + 3;

      func (&d, user_data);
    }

    line_start = line_end + 1;
  }

beach:
  g_free (output);
}

static gpointer
gum_gcc_cmodule_find_symbol_by_name (GumCModule * cm,
                                     const gchar * name)
{
  GumCSymbolDetails ctx;

  ctx.name = name;
  ctx.address = NULL;
  gum_cmodule_enumerate_symbols (cm, gum_store_address_if_name_matches, &ctx);

  return ctx.address;
}

static void
gum_store_address_if_name_matches (const GumCSymbolDetails * details,
                                   gpointer user_data)
{
  GumCSymbolDetails * ctx = user_data;

  if (strcmp (details->name, ctx->name) == 0)
    ctx->address = details->address;
}

static void
gum_gcc_cmodule_drop_metadata (GumCModule * cm)
{
  GumGccCModule * self = GUM_GCC_CMODULE (cm);

  g_clear_pointer (&self->symbols, g_array_unref);

  g_clear_pointer (&self->argv, g_ptr_array_unref);

  if (self->workdir != NULL)
  {
    GFile * workdir_file = g_file_new_for_path (self->workdir);

    gum_rmtree (workdir_file);
    g_object_unref (workdir_file);

    g_free (self->workdir);
    self->workdir = NULL;
  }
}

#ifdef HAVE_DARWIN

#define GUM_TYPE_DARWIN_CMODULE (gum_darwin_cmodule_get_type ())
G_DECLARE_FINAL_TYPE (GumDarwinCModule, gum_darwin_cmodule, GUM, DARWIN_CMODULE,
    GumCModule)

typedef struct _GumEnumerateExportsContext GumEnumerateExportsContext;

struct _GumDarwinCModule
{
  GumCModule parent;

  gchar * name;
  GBytes * binary;
  gchar * workdir;
  GPtrArray * argv;
  GHashTable * symbols;

  GumDarwinModuleResolver * resolver;
  GumDarwinMapper * mapper;
  GumDarwinModule * module;
};

struct _GumEnumerateExportsContext
{
  GumFoundCSymbolFunc func;
  gpointer user_data;

  GumDarwinCModule * self;
};

static void gum_darwin_cmodule_add_define (GumCModule * cm, const gchar * name,
    const gchar * value);
static void gum_darwin_cmodule_add_symbol (GumCModule * cm, const gchar * name,
    gconstpointer value);
static gboolean gum_darwin_cmodule_link_pre (GumCModule * cm, gsize * size,
    GString ** error_messages);
static gboolean gum_darwin_cmodule_link_at (GumCModule * cm, gpointer base,
    GString ** error_messages);
static void gum_darwin_cmodule_link_post (GumCModule * cm);
static void gum_darwin_cmodule_enumerate_symbols (GumCModule * cm,
    GumFoundCSymbolFunc func, gpointer user_data);
static gboolean gum_emit_export (const GumDarwinExportDetails * details,
    gpointer user_data);
static gpointer gum_darwin_cmodule_find_symbol_by_name (GumCModule * cm,
    const gchar * name);
static GumAddress gum_darwin_cmodule_resolve_symbol (const gchar * symbol,
    gpointer user_data);
static void gum_darwin_cmodule_drop_metadata (GumCModule * cm);

G_DEFINE_TYPE (GumDarwinCModule, gum_darwin_cmodule, GUM_TYPE_CMODULE)

static void
gum_darwin_cmodule_class_init (GumDarwinCModuleClass * klass)
{
  GumCModuleClass * cmodule_class = GUM_CMODULE_CLASS (klass);

  cmodule_class->add_define = gum_darwin_cmodule_add_define;
  cmodule_class->add_symbol = gum_darwin_cmodule_add_symbol;
  cmodule_class->link_pre = gum_darwin_cmodule_link_pre;
  cmodule_class->link_at = gum_darwin_cmodule_link_at;
  cmodule_class->link_post = gum_darwin_cmodule_link_post;
  cmodule_class->enumerate_symbols = gum_darwin_cmodule_enumerate_symbols;
  cmodule_class->find_symbol_by_name = gum_darwin_cmodule_find_symbol_by_name;
  cmodule_class->drop_metadata = gum_darwin_cmodule_drop_metadata;
}

static void
gum_darwin_cmodule_init (GumDarwinCModule * self)
{
  self->name = g_strdup ("module.dylib");
  self->symbols = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

static GumCModule *
gum_darwin_cmodule_new (const gchar * source,
                        GBytes * binary,
                        const GumCModuleOptions * options,
                        GError ** error)
{
  GumCModule * result;
  GumDarwinCModule * cmodule;
  gboolean success = FALSE;
  gchar * source_path = NULL;
  gchar * binary_path = NULL;
  gchar * output = NULL;

  result = g_object_new (GUM_TYPE_DARWIN_CMODULE, NULL);
  cmodule = GUM_DARWIN_CMODULE (result);

  if (binary != NULL)
  {
    cmodule->binary = g_bytes_ref (binary);
  }
  else
  {
    const gchar * arch;
    gint exit_status;
    gpointer data;
    gsize size;

#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
    arch = "x86_64";
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
    arch = "i386";
#elif defined (HAVE_ARM64) && defined (HAVE_PTRAUTH)
    arch = "arm64e";
#elif defined (HAVE_ARM64)
    arch = "arm64";
#elif defined (HAVE_ARM)
    arch = "armv7";
#else
# error Unsupported architecture.
#endif

    cmodule->workdir = g_dir_make_tmp ("cmodule-XXXXXX", error);
    if (cmodule->workdir == NULL)
      goto beach;

    source_path = g_build_filename (cmodule->workdir, "module.c", NULL);
    binary_path = g_build_filename (cmodule->workdir, cmodule->name, NULL);

    if (!g_file_set_contents (source_path, source, -1, error))
      goto beach;

    if (!gum_populate_include_dir (cmodule->workdir, error))
      goto beach;

    cmodule->argv = g_ptr_array_new_with_free_func (g_free);
    g_ptr_array_add (cmodule->argv, g_strdup ("clang"));
    g_ptr_array_add (cmodule->argv, g_strdup ("-arch"));
    g_ptr_array_add (cmodule->argv, g_strdup (arch));
    g_ptr_array_add (cmodule->argv, g_strdup ("-dynamiclib"));
    g_ptr_array_add (cmodule->argv, g_strdup ("-Wall"));
    g_ptr_array_add (cmodule->argv, g_strdup ("-Werror"));
    g_ptr_array_add (cmodule->argv, g_strdup ("-O2"));
    g_ptr_array_add (cmodule->argv, g_strdup ("-isystem"));
    g_ptr_array_add (cmodule->argv, g_strdup ("."));
    g_ptr_array_add (cmodule->argv, g_strdup ("-isystem"));
    g_ptr_array_add (cmodule->argv, g_strdup ("capstone"));
    gum_cmodule_add_standard_defines (result);
    g_ptr_array_add (cmodule->argv, g_strdup ("module.c"));
    g_ptr_array_add (cmodule->argv, g_strdup ("-o"));
    g_ptr_array_add (cmodule->argv, g_strdup (cmodule->name));
    g_ptr_array_add (cmodule->argv, g_strdup ("-Wl,-undefined,dynamic_lookup"));
    g_ptr_array_add (cmodule->argv, NULL);

    if (!gum_call_tool (cmodule->workdir,
        (const gchar * const *) cmodule->argv->pdata, &output, &exit_status,
        error))
    {
      goto beach;
    }

    if (exit_status != 0)
      goto compilation_failed;

    if (!g_file_get_contents (binary_path, (gchar **) &data, &size, error))
      goto beach;

    cmodule->binary = g_bytes_new_take (data, size);
  }

  success = TRUE;
  goto beach;

compilation_failed:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT,
        "Compilation failed: %s", output);
    goto beach;
  }
beach:
  {
    g_free (output);
    g_free (binary_path);
    g_free (source_path);
    if (!success)
      g_clear_object (&result);

    return result;
  }
}

static void
gum_darwin_cmodule_add_define (GumCModule * cm,
                               const gchar * name,
                               const gchar * value)
{
  GumDarwinCModule * self = GUM_DARWIN_CMODULE (cm);
  gchar * arg;

  arg = (value == NULL)
      ? g_strconcat ("-D", name, NULL)
      : g_strconcat ("-D", name, "=", value, NULL);

  g_ptr_array_add (self->argv, arg);
}

static void
gum_darwin_cmodule_add_symbol (GumCModule * cm,
                               const gchar * name,
                               gconstpointer value)
{
  GumDarwinCModule * self = GUM_DARWIN_CMODULE (cm);

  g_hash_table_insert (self->symbols, g_strdup (name), (gpointer) value);
}

static gboolean
gum_darwin_cmodule_link_pre (GumCModule * cm,
                             gsize * size,
                             GString ** error_messages)
{
  GumDarwinCModule * self = GUM_DARWIN_CMODULE (cm);
  gboolean success = FALSE;
  GError * error = NULL;

  self->resolver = gum_darwin_module_resolver_new (mach_task_self (), NULL);
  gum_darwin_module_resolver_set_dynamic_lookup_handler (self->resolver,
      gum_darwin_cmodule_resolve_symbol, self, NULL);

  self->mapper = gum_darwin_mapper_new_take_blob (self->name,
      g_steal_pointer (&self->binary), self->resolver, &error);
  if (error != NULL)
    goto propagate_error;

  g_object_get (self->mapper, "module", &self->module, NULL);

  *size = gum_darwin_mapper_size (self->mapper);

  success = TRUE;
  goto beach;

propagate_error:
  {
    gum_append_error (error_messages, error->message);
    g_error_free (error);
    goto beach;
  }
beach:
  {
    return success;
  }
}

static gboolean
gum_darwin_cmodule_link_at (GumCModule * cm,
                            gpointer base,
                            GString ** error_messages)
{
  GumDarwinCModule * self;
  GumCModulePrivate * priv;
  GError * error = NULL;
  GumDarwinMapperConstructor ctor;

  self = GUM_DARWIN_CMODULE (cm);
  priv = gum_cmodule_get_instance_private (cm);

  gum_darwin_mapper_map (self->mapper, GUM_ADDRESS (base), &error);
  if (error != NULL)
    goto propagate_error;

  ctor = GSIZE_TO_POINTER (gum_darwin_mapper_constructor (self->mapper));
  ctor ();

  priv->destruct =
      GSIZE_TO_POINTER (gum_darwin_mapper_destructor (self->mapper));

  return TRUE;

propagate_error:
  {
    if (g_error_matches (error, GUM_ERROR, GUM_ERROR_NOT_FOUND))
    {
      const gchar * name_start, * name_end;
      gchar * name, * message;

      name_start = g_utf8_find_next_char (strstr (error->message, "“"), NULL);
      name_end = strstr (name_start, "”");

      name = g_strndup (name_start, name_end - name_start);

      message = g_strdup_printf ("undefined reference to `%s'", name);

      gum_append_error (error_messages, message);

      g_free (message);
      g_free (name);
    }
    else
    {
      gum_append_error (error_messages, error->message);
    }

    g_error_free (error);

    return FALSE;
  }
}

static void
gum_darwin_cmodule_link_post (GumCModule * cm)
{
}

static void
gum_darwin_cmodule_enumerate_symbols (GumCModule * cm,
                                      GumFoundCSymbolFunc func,
                                      gpointer user_data)
{
  GumDarwinCModule * self = GUM_DARWIN_CMODULE (cm);
  GumEnumerateExportsContext ctx;

  ctx.func = func;
  ctx.user_data = user_data;

  ctx.self = self;

  gum_darwin_module_enumerate_exports (self->module, gum_emit_export, &ctx);
}

static gboolean
gum_emit_export (const GumDarwinExportDetails * details,
                 gpointer user_data)
{
  GumEnumerateExportsContext * ctx = user_data;
  GumDarwinCModule * self = ctx->self;
  GumExportDetails export;
  GumCSymbolDetails d;

  if (!gum_darwin_module_resolver_resolve_export (self->resolver, self->module,
      details, &export))
  {
    return TRUE;
  }

  d.name = export.name;
  d.address = GSIZE_TO_POINTER (export.address);

  ctx->func (&d, ctx->user_data);

  return TRUE;
}

static gpointer
gum_darwin_cmodule_find_symbol_by_name (GumCModule * cm,
                                        const gchar * name)
{
  GumDarwinCModule * self = GUM_DARWIN_CMODULE (cm);

  return GSIZE_TO_POINTER (gum_darwin_module_resolver_find_export_address (
      self->resolver, self->module, name));
}

static GumAddress
gum_darwin_cmodule_resolve_symbol (const gchar * symbol,
                                   gpointer user_data)
{
  GumDarwinCModule * self = GUM_DARWIN_CMODULE (user_data);
  gpointer address;

  address = g_hash_table_lookup (self->symbols, symbol);
  if (address == NULL)
    address = g_hash_table_lookup (gum_cmodule_get_symbols (), symbol);

  return GUM_ADDRESS (address);
}

static void
gum_darwin_cmodule_drop_metadata (GumCModule * cm)
{
  GumDarwinCModule * self = GUM_DARWIN_CMODULE (cm);

  g_clear_object (&self->module);
  g_clear_object (&self->mapper);
  g_clear_object (&self->resolver);

  g_clear_pointer (&self->symbols, g_hash_table_unref);

  g_clear_pointer (&self->argv, g_ptr_array_unref);

  if (self->workdir != NULL)
  {
    GFile * workdir_file = g_file_new_for_path (self->workdir);

    gum_rmtree (workdir_file);
    g_object_unref (workdir_file);

    g_free (self->workdir);
    self->workdir = NULL;
  }

  g_clear_pointer (&self->binary, g_bytes_unref);

  g_clear_pointer (&self->name, g_free);
}

#endif /* HAVE_DARWIN */

static void
gum_csymbol_details_destroy (GumCSymbolDetails * details)
{
  g_free ((gchar *) details->name);
}

static gboolean
gum_populate_include_dir (const gchar * path,
                          GError ** error)
{
  guint i;

  for (i = 0; i != G_N_ELEMENTS (gum_cmodule_headers); i++)
  {
    const GumCHeaderDetails * h = &gum_cmodule_headers[i];
    gchar * filename, * dirname;
    gboolean written;

    if (h->kind != GUM_CHEADER_FRIDA)
      continue;

    filename = g_build_filename (path, h->name, NULL);
    dirname = g_path_get_dirname (filename);

    g_mkdir_with_parents (dirname, 0700);
    written = g_file_set_contents (filename, h->data, h->size, error);

    g_free (dirname);
    g_free (filename);

    if (!written)
      return FALSE;
  }

  return TRUE;
}

static void
gum_rmtree (GFile * file)
{
  GFileEnumerator * enumerator;

  enumerator = g_file_enumerate_children (file, G_FILE_ATTRIBUTE_STANDARD_NAME,
      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);
  if (enumerator != NULL)
  {
    GFileInfo * info;
    GFile * child;

    while (g_file_enumerator_iterate (enumerator, &info, &child, NULL, NULL) &&
        child != NULL)
    {
      if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
        gum_rmtree (child);
      else
        g_file_delete (child, NULL, NULL);
    }

    g_object_unref (enumerator);
  }

  g_file_delete (file, NULL, NULL);
}

static gboolean
gum_call_tool (const gchar * cwd,
               const gchar * const * argv,
               gchar ** output,
               gint * exit_status,
               GError ** error)
{
  GSubprocessLauncher * launcher;
  GSubprocess * proc;

  launcher = g_subprocess_launcher_new (
      G_SUBPROCESS_FLAGS_STDOUT_PIPE |
      G_SUBPROCESS_FLAGS_STDERR_MERGE);
  g_subprocess_launcher_set_cwd (launcher, cwd);
  proc = g_subprocess_launcher_spawnv (launcher, argv, error);
  g_object_unref (launcher);
  if (proc == NULL)
    goto propagate_error;

  if (!g_subprocess_communicate_utf8 (proc, NULL, NULL, output, NULL, error))
    goto propagate_error;

  *exit_status = g_subprocess_get_exit_status (proc);

  g_object_unref (proc);

  return TRUE;

propagate_error:
  {
    g_clear_object (&proc);

    return FALSE;
  }
}

#endif /* !G_OS_NONE */

static void
gum_append_error (GString ** messages,
                  const char * msg)
{
  if (*messages == NULL)
  {
    *messages = g_string_new (msg);
  }
  else
  {
    g_string_append_c (*messages, '\n');
    g_string_append (*messages, msg);
  }
}
