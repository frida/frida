/*
 * Copyright (C) 2008-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2021 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_BACKTRACER_H__
#define __GUM_BACKTRACER_H__

#include <gum/gumdefs.h>
#include <gum/gumreturnaddress.h>

G_BEGIN_DECLS

#define GUM_TYPE_BACKTRACER (gum_backtracer_get_type ())
G_DECLARE_INTERFACE (GumBacktracer, gum_backtracer, GUM, BACKTRACER, GObject)

struct _GumBacktracerInterface
{
  GTypeInterface parent;

  void (* generate) (GumBacktracer * self, const GumCpuContext * cpu_context,
      GumReturnAddressArray * return_addresses, guint limit);
};

GUM_API GumBacktracer * gum_backtracer_make_accurate (void);
GUM_API GumBacktracer * gum_backtracer_make_fuzzy (void);

GUM_API void gum_backtracer_generate (GumBacktracer * self,
    const GumCpuContext * cpu_context,
    GumReturnAddressArray * return_addresses);
GUM_API void gum_backtracer_generate_with_limit (GumBacktracer * self,
    const GumCpuContext * cpu_context,
    GumReturnAddressArray * return_addresses, guint limit);

G_END_DECLS

#endif
