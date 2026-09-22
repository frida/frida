/*
 * Copyright (C) 2014 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_PRINTF_H__
#define __GUM_PRINTF_H__

#include <glib.h>

G_BEGIN_DECLS

gint gum_vsnprintf (gchar * str, gsize size, const gchar * format,
    va_list args);
gint gum_snprintf (gchar * str, gsize size, const gchar * format, ...);
gint gum_vasprintf (gchar ** ret, const gchar * format, va_list ap);
gint gum_asprintf (gchar ** ret, const gchar * format, ...);

G_END_DECLS

#endif
