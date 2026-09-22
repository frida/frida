/*
 * Copyright (C) 2008-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_X86_BACKTRACER_H__
#define __GUM_X86_BACKTRACER_H__

#include <gum/gumbacktracer.h>

G_BEGIN_DECLS

#define GUM_TYPE_X86_BACKTRACER (gum_x86_backtracer_get_type ())
G_DECLARE_FINAL_TYPE (GumX86Backtracer, gum_x86_backtracer, GUM, X86_BACKTRACER,
                      GObject)

GUM_API GumBacktracer * gum_x86_backtracer_new (void);

G_END_DECLS

#endif
