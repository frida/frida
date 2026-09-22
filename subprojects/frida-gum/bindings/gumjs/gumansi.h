/*
 * Copyright (C) 2010-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_ANSI_H__
#define __GUM_ANSI_H__

#include <glib.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL gchar * _gum_ansi_string_to_utf8 (const gchar * str_ansi,
    gint length);
G_GNUC_INTERNAL gchar * _gum_ansi_string_from_utf8 (const gchar * str_utf8);

G_END_DECLS

#endif
