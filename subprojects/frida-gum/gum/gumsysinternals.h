/*
 * Copyright (C) 2010-2014 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_SYS_INTERNALS_H__
#define __GUM_SYS_INTERNALS_H__

#include <glib.h>

#ifdef G_OS_WIN32

# if GLIB_SIZEOF_VOID_P == 4
#  define GUM_TEB_OFFSET_SELF 0x0018
#  define GUM_TEB_OFFSET_TID  0x0024
# else
#  define GUM_TEB_OFFSET_SELF 0x0030
#  define GUM_TEB_OFFSET_TID  0x0048
# endif

#endif

#endif
