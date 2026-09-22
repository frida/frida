/*
 * Copyright (C) 2015-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumlibc.h"

gpointer
gum_memset (gpointer dst,
            gint c,
            gsize n)
{
  gsize offset;

  for (offset = 0; offset != n; offset++)
    ((guint8 *) dst)[offset] = c;

  return dst;
}

#if defined (_MSC_VER) \
    || !(defined (HAVE_ARM64) && defined (__LP64__) && defined (__ARM_FP))

gpointer
gum_memcpy (gpointer dst,
            gconstpointer src,
            gsize n)
{
  gsize offset;

  for (offset = 0; offset != n; offset++)
    ((guint8 *) dst)[offset] = ((guint8 *) src)[offset];

  return dst;
}

gpointer
gum_memmove (gpointer dst,
             gconstpointer src,
             gsize n)
{
  guint8 * dst_u8 = dst;
  const guint8 * src_u8 = src;
  gsize i;

  if (dst_u8 < src_u8)
  {
    for (i = 0; i != n; i++)
      dst_u8[i] = src_u8[i];
  }
  else if (dst_u8 > src_u8)
  {
    for (i = n; i != 0; i--)
      dst_u8[i - 1] = src_u8[i - 1];
  }

  return dst;
}

#endif
