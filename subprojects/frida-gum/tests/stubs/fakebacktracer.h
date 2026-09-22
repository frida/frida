/*
 * Copyright (C) 2008-2018 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __FAKE_BACKTRACER_H__
#define __FAKE_BACKTRACER_H__

#include <glib-object.h>
#include <gum/gum.h>

G_BEGIN_DECLS

#define GUM_TYPE_FAKE_BACKTRACER (gum_fake_backtracer_get_type ())
G_DECLARE_FINAL_TYPE (GumFakeBacktracer, gum_fake_backtracer, GUM,
    FAKE_BACKTRACER, GObject)

struct _GumFakeBacktracer
{
  GObject parent;

  const GumReturnAddress * ret_addrs;
  guint num_ret_addrs;
};

GumBacktracer * gum_fake_backtracer_new (const GumReturnAddress * ret_addrs,
    guint num_ret_addrs);

G_END_DECLS

#endif
