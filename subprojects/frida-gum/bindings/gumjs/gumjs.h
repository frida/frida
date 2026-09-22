/*
 * Copyright (C) 2018 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUMJS_H__
#define __GUMJS_H__

#include <gum/gum.h>

G_BEGIN_DECLS

GUM_API void gumjs_prepare_to_fork (void);
GUM_API void gumjs_recover_from_fork_in_parent (void);
GUM_API void gumjs_recover_from_fork_in_child (void);

G_END_DECLS

#endif
