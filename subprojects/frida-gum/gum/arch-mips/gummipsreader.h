/*
 * Copyright (C) 2015 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_MIPS_READER_H__
#define __GUM_MIPS_READER_H__

#include "gumdefs.h"

G_BEGIN_DECLS

gpointer gum_mips_reader_try_get_relative_jump_target (gconstpointer address);

G_END_DECLS

#endif
