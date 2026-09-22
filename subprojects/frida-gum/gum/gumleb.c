/*
 * Copyright (C) 2015-2021 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumleb.h"

gint64
gum_read_sleb128 (const guint8 ** data,
                  const guint8 * end)
{
  const guint8 * p = *data;
  gint64 result = 0;
  gint offset = 0;
  guint8 value;

  do
  {
    gint64 chunk;

    if (p == end || offset > 63)
      goto beach;

    value = *p;
    chunk = value & 0x7f;
    result |= (chunk << offset);
    offset += 7;
  }
  while (*p++ & 0x80);

  if ((value & 0x40) != 0)
    result |= G_GINT64_CONSTANT (-1) << offset;

beach:
  *data = p;

  return result;
}

guint64
gum_read_uleb128 (const guint8 ** data,
                  const guint8 * end)
{
  const guint8 * p = *data;
  guint64 result = 0;
  gint offset = 0;

  do
  {
    guint64 chunk;

    if (p == end || offset > 63)
      goto beach;

    chunk = *p & 0x7f;
    result |= (chunk << offset);
    offset += 7;
  }
  while (*p++ & 0x80);

beach:
  *data = p;

  return result;
}

void
gum_skip_leb128 (const guint8 ** data,
                 const guint8 * end)
{
  const guint8 * p = *data;

  while ((*p & 0x80) != 0)
  {
    if (p == end)
      goto beach;

    p++;
  }

  p++;

beach:
  *data = p;
}
