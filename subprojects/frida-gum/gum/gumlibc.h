/*
 * Copyright (C) 2015-2021 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_LIBC_H__
#define __GUM_LIBC_H__

#include <gum/gumdefs.h>

G_BEGIN_DECLS

GUM_API gpointer gum_memset (gpointer dst, gint c, gsize n);
GUM_API gpointer gum_memcpy (gpointer dst, gconstpointer src, gsize n);
GUM_API gpointer gum_memmove (gpointer dst, gconstpointer src, gsize n);

G_END_DECLS

#endif
