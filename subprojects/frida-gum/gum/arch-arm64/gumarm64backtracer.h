/*
 * Copyright (C) 2015-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_ARM64_BACKTRACER_H__
#define __GUM_ARM64_BACKTRACER_H__

#include <gum/gumbacktracer.h>

G_BEGIN_DECLS

#define GUM_TYPE_ARM64_BACKTRACER (gum_arm64_backtracer_get_type ())
G_DECLARE_FINAL_TYPE (GumArm64Backtracer, gum_arm64_backtracer, GUM,
                      ARM64_BACKTRACER, GObject)

GUM_API GumBacktracer * gum_arm64_backtracer_new (void);

G_END_DECLS

#endif
