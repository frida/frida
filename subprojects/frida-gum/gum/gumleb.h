/*
 * Copyright (C) 2015-2021 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_LEB_H__
#define __GUM_LEB_H__

#include <glib.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL gint64 gum_read_sleb128 (const guint8 ** data,
    const guint8 * end);
G_GNUC_INTERNAL guint64 gum_read_uleb128 (const guint8 ** data,
    const guint8 * end);
G_GNUC_INTERNAL void gum_skip_leb128 (const guint8 ** data, const guint8 * end);

G_END_DECLS

#endif
