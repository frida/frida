/*
 * Copyright (C) 2015-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2020 Marcus Mengs <mame8282@googlemail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumdefs.h"
#include "gumffi.h"

#include <stdint.h>

typedef struct _GumFFITypeMapping GumFFITypeMapping;
typedef struct _GumFFIABIMapping GumFFIABIMapping;

struct _GumFFITypeMapping
{
  const gchar * name;
  ffi_type * type;
};

struct _GumFFIABIMapping
{
  const gchar * name;
  ffi_abi abi;
};

/* Based on the analogous macro in libffi's types.c */
#define GUM_DEFINE_FFI_TYPE(name, type, id)          \
    struct GumFFIStructAlign_##name                  \
    {                                                \
      char c;                                        \
      type x;                                        \
    };                                               \
                                                     \
    ffi_type gum_ffi_type_##name =                   \
    {                                                \
      sizeof (type),                                 \
      offsetof (struct GumFFIStructAlign_##name, x), \
      id, NULL                                       \
    };

#if defined (SIZE_WIDTH)
# if SIZE_WIDTH == 64
GUM_DEFINE_FFI_TYPE (size_t, guint64, FFI_TYPE_UINT64)
GUM_DEFINE_FFI_TYPE (ssize_t, gint64, FFI_TYPE_SINT64)
# elif SIZE_WIDTH == 32
GUM_DEFINE_FFI_TYPE (size_t, guint32, FFI_TYPE_UINT32)
GUM_DEFINE_FFI_TYPE (ssize_t, gint32, FFI_TYPE_SINT32)
# elif SIZE_WIDTH == 16
GUM_DEFINE_FFI_TYPE (size_t, guint16, FFI_TYPE_UINT16)
GUM_DEFINE_FFI_TYPE (ssize_t, gint16, FFI_TYPE_SINT16)
# endif
#elif defined (SIZE_MAX)
# if SIZE_MAX == UINT64_MAX
GUM_DEFINE_FFI_TYPE (size_t, guint64, FFI_TYPE_UINT64)
GUM_DEFINE_FFI_TYPE (ssize_t, gint64, FFI_TYPE_SINT64)
# elif SIZE_MAX == UINT32_MAX
GUM_DEFINE_FFI_TYPE (size_t, guint32, FFI_TYPE_UINT32)
GUM_DEFINE_FFI_TYPE (ssize_t, gint32, FFI_TYPE_SINT32)
# elif SIZE_MAX == UINT16_MAX
GUM_DEFINE_FFI_TYPE (size_t, guint16, FFI_TYPE_UINT16)
GUM_DEFINE_FFI_TYPE (ssize_t, gint16, FFI_TYPE_SINT16)
# else
#  error "size_t size not supported"
# endif
#else
# error "size_t detection missing"
#endif

static void gum_ffi_copy_struct_fields (const ffi_type * type,
    gconstpointer src, gpointer dst);

static const GumFFITypeMapping gum_ffi_type_mappings[] =
{
  { "void", &ffi_type_void },
  { "pointer", &ffi_type_pointer },
  { "int", &ffi_type_sint },
  { "uint", &ffi_type_uint },
  { "long", &ffi_type_slong },
  { "ulong", &ffi_type_ulong },
  { "char", &ffi_type_schar },
  { "uchar", &ffi_type_uchar },
  { "size_t", &gum_ffi_type_size_t },
  { "ssize_t", &gum_ffi_type_ssize_t },
  { "float", &ffi_type_float },
  { "double", &ffi_type_double },
  { "int8", &ffi_type_sint8 },
  { "uint8", &ffi_type_uint8 },
  { "int16", &ffi_type_sint16 },
  { "uint16", &ffi_type_uint16 },
  { "int32", &ffi_type_sint32 },
  { "uint32", &ffi_type_uint32 },
  { "int64", &ffi_type_sint64 },
  { "uint64", &ffi_type_uint64 },
  { "bool", &ffi_type_schar }
};

static const GumFFIABIMapping gum_ffi_abi_mappings[] =
{
  { "default", GUM_DEFAULT_FFI_ABI },
#if defined (X86_WIN64)
  { "win64", FFI_WIN64 },
#elif defined (X86_ANY) && GLIB_SIZEOF_VOID_P == 8
  { "unix64", FFI_UNIX64 },
  { "win64", FFI_EFI64 },
#elif defined (X86_ANY) && GLIB_SIZEOF_VOID_P == 4
  { "sysv", FFI_SYSV },
  { "stdcall", FFI_STDCALL },
  { "thiscall", FFI_THISCALL },
  { "fastcall", FFI_FASTCALL },
# if defined (X86_WIN32)
  { "mscdecl", FFI_MS_CDECL },
# endif
#elif defined (ARM)
  { "sysv", FFI_SYSV },
# if GLIB_SIZEOF_VOID_P == 4
  { "vfp", FFI_VFP },
# endif
#endif
};

void
gum_ffi_arg_to_ret (const ffi_type * type,
                    GumFFIArg * arg,
                    GumFFIRet * ret)
{
  if (type == &ffi_type_void)
  {
    ret->v_pointer = arg->v_pointer;
  }
  else if (type == &ffi_type_pointer)
  {
    ret->v_pointer = arg->v_pointer;
  }
  else if (type == &ffi_type_sint8)
  {
    ret->v_sint8 = arg->v_sint8;
  }
  else if (type == &ffi_type_uint8)
  {
    ret->v_uint8 = arg->v_uint8;
  }
  else if (type == &ffi_type_sint16)
  {
    ret->v_sint16 = arg->v_sint16;
  }
  else if (type == &ffi_type_uint16)
  {
    ret->v_uint16 = arg->v_uint16;
  }
  else if (type == &ffi_type_sint32)
  {
    ret->v_sint32 = arg->v_sint32;
  }
  else if (type == &ffi_type_uint32)
  {
    ret->v_uint32 = arg->v_uint32;
  }
  else if (type == &ffi_type_sint64)
  {
    ret->v_sint64 = arg->v_sint64;
  }
  else if (type == &ffi_type_uint64)
  {
    ret->v_uint64 = arg->v_uint64;
  }
  else if (type == &gum_ffi_type_size_t)
  {
    switch (type->size)
    {
      case 8:
        ret->v_uint64 = arg->v_uint64;
        break;
      case 4:
        ret->v_uint32 = arg->v_uint32;
        break;
      case 2:
        ret->v_uint16 = arg->v_uint16;
        break;
      default:
        g_assert_not_reached ();
    }
  }
  else if (type == &gum_ffi_type_ssize_t)
  {
    switch (type->size)
    {
      case 8:
        ret->v_sint64 = arg->v_sint64;
        break;
      case 4:
        ret->v_sint32 = arg->v_sint32;
        break;
      case 2:
        ret->v_sint16 = arg->v_sint16;
        break;
      default:
        g_assert_not_reached ();
    }
  }
  else if (type == &ffi_type_float)
  {
    ret->v_float = arg->v_float;
  }
  else if (type == &ffi_type_double)
  {
    ret->v_double = arg->v_double;
  }
  else if (type->type == FFI_TYPE_STRUCT)
  {
    gum_ffi_copy_struct_fields (type, arg, ret);
  }
  else
  {
    g_assert_not_reached ();
  }
}

static void
gum_ffi_copy_struct_fields (const ffi_type * type,
                            gconstpointer src,
                            gpointer dst)
{
  ffi_type ** field_type;
  gsize offset = 0;

  for (field_type = type->elements; *field_type != NULL; field_type++)
  {
    const ffi_type * t = *field_type;

    offset = GUM_ALIGN_SIZE (offset, t->alignment);

    if (t->type == FFI_TYPE_STRUCT)
    {
      gum_ffi_copy_struct_fields (t, (const guint8 *) src + offset,
          (guint8 *) dst + offset);
    }
    else
    {
      memcpy ((guint8 *) dst + offset, (const guint8 *) src + offset, t->size);
    }

    offset += t->size;
  }
}

gboolean
gum_ffi_try_get_type_by_name (const gchar * name,
                              ffi_type ** type)
{
  guint i;

  for (i = 0; i != G_N_ELEMENTS (gum_ffi_type_mappings); i++)
  {
    const GumFFITypeMapping * m = &gum_ffi_type_mappings[i];

    if (strcmp (m->name, name) == 0)
    {
      *type = m->type;
      return TRUE;
    }
  }

  return FALSE;
}

gboolean
gum_ffi_try_get_abi_by_name (const gchar * name,
                             ffi_abi * abi)
{
  guint i;

  for (i = 0; i != G_N_ELEMENTS (gum_ffi_abi_mappings); i++)
  {
    const GumFFIABIMapping * m = &gum_ffi_abi_mappings[i];

    if (strcmp (m->name, name) == 0)
    {
      *abi = m->abi;
      return TRUE;
    }
  }

  return FALSE;
}

ffi_type *
gum_ffi_maybe_promote_variadic (ffi_type * type)
{
  if (type->size < sizeof (int))
  {
    if (type == &ffi_type_sint8 || type == &ffi_type_sint16)
      return &ffi_type_sint32;

    if (type == &ffi_type_uint8 || type == &ffi_type_uint16)
      return &ffi_type_uint32;
  }

  if (type == &ffi_type_float)
    return &ffi_type_double;

  return type;
}
