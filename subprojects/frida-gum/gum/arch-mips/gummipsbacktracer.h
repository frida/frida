/*
 * Copyright (C) 2015-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_MIPS_BACKTRACER_H__
#define __GUM_MIPS_BACKTRACER_H__

#include <gum/gumbacktracer.h>

G_BEGIN_DECLS

#define GUM_TYPE_MIPS_BACKTRACER (gum_mips_backtracer_get_type ())
G_DECLARE_FINAL_TYPE (GumMipsBacktracer, gum_mips_backtracer, GUM,
                      MIPS_BACKTRACER, GObject)

GUM_API GumBacktracer * gum_mips_backtracer_new (void);

G_END_DECLS

#endif
