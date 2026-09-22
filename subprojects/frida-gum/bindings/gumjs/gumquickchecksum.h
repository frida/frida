/*
 * Copyright (C) 2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_CHECKSUM_H__
#define __GUM_QUICK_CHECKSUM_H__

#include "gumquickcore.h"

G_BEGIN_DECLS

typedef struct _GumQuickChecksum GumQuickChecksum;

struct _GumQuickChecksum
{
  GumQuickCore * core;

  JSClassID checksum_class;
};

G_GNUC_INTERNAL void _gum_quick_checksum_init (GumQuickChecksum * self,
    JSValue ns, GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_checksum_dispose (GumQuickChecksum * self);
G_GNUC_INTERNAL void _gum_quick_checksum_finalize (GumQuickChecksum * self);

G_END_DECLS

#endif
