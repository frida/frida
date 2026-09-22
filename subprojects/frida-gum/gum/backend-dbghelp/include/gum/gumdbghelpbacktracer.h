/*
 * Copyright (C) 2008-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_DBGHELP_BACKTRACER_H__
#define __GUM_DBGHELP_BACKTRACER_H__

#include "gumdbghelp.h"

#include <gum/gum.h>

G_BEGIN_DECLS

#define GUM_TYPE_DBGHELP_BACKTRACER (gum_dbghelp_backtracer_get_type ())
G_DECLARE_FINAL_TYPE (GumDbghelpBacktracer, gum_dbghelp_backtracer, GUM,
                      DBGHELP_BACKTRACER, GObject)

GUM_API GumBacktracer * gum_dbghelp_backtracer_new (GumDbghelpImpl * dbghelp);

G_END_DECLS

#endif
