/*
 * Copyright (C) 2015-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_UNW_BACKTRACER_H__
#define __GUM_UNW_BACKTRACER_H__

#include <gum/gumbacktracer.h>

G_BEGIN_DECLS

#define GUM_TYPE_UNW_BACKTRACER (gum_unw_backtracer_get_type ())
G_DECLARE_FINAL_TYPE (GumUnwBacktracer, gum_unw_backtracer, GUM, UNW_BACKTRACER,
                      GObject)

GUM_API GumBacktracer * gum_unw_backtracer_new (void);

G_END_DECLS

#endif
