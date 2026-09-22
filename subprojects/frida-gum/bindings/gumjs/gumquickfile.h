/*
 * Copyright (C) 2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_FILE_H__
#define __GUM_QUICK_FILE_H__

#include "gumquickcore.h"

G_BEGIN_DECLS

typedef struct _GumQuickFile GumQuickFile;

struct _GumQuickFile
{
  GumQuickCore * core;

  JSClassID file_class;
};

G_GNUC_INTERNAL void _gum_quick_file_init (GumQuickFile * self,
    JSValue ns, GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_file_dispose (GumQuickFile * self);
G_GNUC_INTERNAL void _gum_quick_file_finalize (GumQuickFile * self);

G_END_DECLS

#endif
