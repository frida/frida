/*
 * Copyright (C) 2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_CODE_WRITER_H__
#define __GUM_QUICK_CODE_WRITER_H__

#include "gumquickcore.h"

G_BEGIN_DECLS

typedef struct _GumQuickCodeWriter GumQuickCodeWriter;

struct _GumQuickCodeWriter
{
  GumQuickCore * core;

#include "gumquickcodewriter-fields.inc"
};

G_GNUC_INTERNAL void _gum_quick_code_writer_init (GumQuickCodeWriter * self,
    JSValue ns, GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_code_writer_dispose (GumQuickCodeWriter * self);
G_GNUC_INTERNAL void _gum_quick_code_writer_finalize (
    GumQuickCodeWriter * self);

#include "gumquickcodewriter-methods.inc"

G_END_DECLS

#endif
