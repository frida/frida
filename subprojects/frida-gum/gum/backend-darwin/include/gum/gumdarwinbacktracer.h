/*
 * Copyright (C) 2015-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_DARWIN_BACKTRACER_H__
#define __GUM_DARWIN_BACKTRACER_H__

#include <gum/gumbacktracer.h>

G_BEGIN_DECLS

#define GUM_TYPE_DARWIN_BACKTRACER (gum_darwin_backtracer_get_type ())
G_DECLARE_FINAL_TYPE (GumDarwinBacktracer, gum_darwin_backtracer, GUM,
                      DARWIN_BACKTRACER, GObject)

GUM_API GumBacktracer * gum_darwin_backtracer_new (void);

G_END_DECLS

#endif
