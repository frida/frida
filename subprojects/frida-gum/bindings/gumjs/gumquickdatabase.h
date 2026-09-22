/*
 * Copyright (C) 2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_DATABASE_H__
#define __GUM_QUICK_DATABASE_H__

#include "gumquickcore.h"
#include "gummemoryvfs.h"

G_BEGIN_DECLS

typedef struct _GumQuickDatabase GumQuickDatabase;

struct _GumQuickDatabase
{
  GumQuickCore * core;

  JSClassID database_class;
  JSClassID statement_class;

  GumMemoryVfs * memory_vfs;
};

G_GNUC_INTERNAL void _gum_quick_database_init (GumQuickDatabase * self,
    JSValue ns, GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_database_dispose (GumQuickDatabase * self);
G_GNUC_INTERNAL void _gum_quick_database_finalize (GumQuickDatabase * self);

G_END_DECLS

#endif
