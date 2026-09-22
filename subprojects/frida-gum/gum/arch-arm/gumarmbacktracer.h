/*
 * Copyright (C) 2013-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_ARM_BACKTRACER_H__
#define __GUM_ARM_BACKTRACER_H__

#include <gum/gumbacktracer.h>

G_BEGIN_DECLS

#define GUM_TYPE_ARM_BACKTRACER (gum_arm_backtracer_get_type ())
G_DECLARE_FINAL_TYPE (GumArmBacktracer, gum_arm_backtracer, GUM, ARM_BACKTRACER,
                      GObject)

GUM_API GumBacktracer * gum_arm_backtracer_new (void);

G_END_DECLS

#endif
