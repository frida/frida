/*
 * Copyright (C) 2015-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2019 Jon Wilson <jonwilson@zepler.net>
 * Copyright (C) 2020 Marcus Mengs <mame8282@googlemail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_FFI_H__
#define __GUM_FFI_H__

#include "gumdefs.h"

#include <ffi.h>
#include <glib-object.h>

#if defined (X86_ANY) && GLIB_SIZEOF_VOID_P == 8 && !defined (X86_WIN64) && \
    GUM_TARGET_ABI_IS_WINDOWS
# define GUM_DEFAULT_FFI_ABI FFI_EFI64
#else
# define GUM_DEFAULT_FFI_ABI FFI_DEFAULT_ABI
#endif

G_BEGIN_DECLS

typedef union _GumFFIArg GumFFIArg;

union _GumFFIArg
{
  gpointer v_pointer;
  gint v_sint;
  guint v_uint;
  glong v_slong;
  gulong v_ulong;
  gchar v_schar;
  guchar v_uchar;
  gfloat v_float;
  gdouble v_double;
  gint8 v_sint8;
  guint8 v_uint8;
  gint16 v_sint16;
  guint16 v_uint16;
  gint32 v_sint32;
  guint32 v_uint32;
  gint64 v_sint64;
  guint64 v_uint64;
};

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

typedef union _GumFFIArg GumFFIRet;

#else

/*
 * On little-endian the low-order bytes of a value appear at the lowest address
 * in memory. To the left in the diagram below. Thus it is trivial and
 * transparent to use a union to zero-extend smaller types into larger types.
 * The low-order bits of the 32-bit value must overlap the low-order bits of the
 * 64-bit value:
 *
 * --------------------------------
 * | 64-bit value                 |
 * --------------------------------
 * | 32-bit value |
 * ----------------
 *
 * On big-endian systems, however, the high-order bytes appear first and hence
 * the low-order bits appear to the right of the diagram below. The 32-bit value
 * must again overlap the low-order bits of the 64-bit value.
 *
 * --------------------------------
 * | 64-bit value                 |
 * --------------------------------
 *                 | 32-bit value |
 *                 ----------------
 *
 * Hence the structures below require padding when compiled for big-endian
 * architectures.
 */

# pragma pack (push, 1)

typedef union _GumFFIRet GumFFIRet;

union _GumFFIRet
{
# if GLIB_SIZEOF_VOID_P == 8
  /* Unpadded 64-bit types */
  gpointer v_pointer;
  gdouble v_double;
  gint64 v_sint64;
  guint64 v_uint64;

  /* Padded 32-bit types */
  struct
  {
    guchar _pad32[4];
    union
    {
      gint v_sint;
      guint v_uint;
      glong v_slong;
      gulong v_ulong;
      gfloat v_float;
      gint32 v_sint32;
      guint32 v_uint32;
    };
  };

  /* Padded 16-bit types */
  struct
  {
    guchar _pad16[6];
    union
    {
      gint16 v_sint16;
      guint16 v_uint16;
    };
  };

  /* Padded 8-bit types */
  struct
  {
    guchar _pad8[7];
    union
    {
      gchar v_schar;
      guchar v_uchar;
      gint8 v_sint8;
      guint8 v_uint8;
    };
  };
# else
  /* Unpadded 64-bit types */
  gdouble v_double;
  gint64 v_sint64;
  guint64 v_uint64;

  /* 32-bit types */
  gpointer v_pointer;
  gint v_sint;
  guint v_uint;
  glong v_slong;
  gulong v_ulong;
  gfloat v_float;
  gint32 v_sint32;
  guint32 v_uint32;

  /* Padded 16-bit types */
  struct
  {
    guint8 _pad16[2];
    union
    {
      gint16 v_sint16;
      guint16 v_uint16;
    };
  };

  /* Padded 8-bit types */
  struct
  {
    guint8 _pad8[3];
    union
    {
      gchar v_schar;
      guchar v_uchar;
      gint8 v_sint8;
      guint8 v_uint8;
    };
  };
# endif
};

# pragma pack (pop)

#endif

extern ffi_type gum_ffi_type_size_t;
extern ffi_type gum_ffi_type_ssize_t;

G_GNUC_INTERNAL void gum_ffi_arg_to_ret (const ffi_type * type, GumFFIArg * arg,
    GumFFIRet * ret);
G_GNUC_INTERNAL gboolean gum_ffi_try_get_type_by_name (const gchar * name,
    ffi_type ** type);
G_GNUC_INTERNAL gboolean gum_ffi_try_get_abi_by_name (const gchar * name,
    ffi_abi * abi);
G_GNUC_INTERNAL ffi_type * gum_ffi_maybe_promote_variadic (ffi_type * type);

G_END_DECLS

#endif
