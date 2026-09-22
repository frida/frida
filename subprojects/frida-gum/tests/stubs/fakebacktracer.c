/*
 * Copyright (C) 2008-2018 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2021 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "fakebacktracer.h"
#include <string.h>

static void gum_fake_backtracer_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_fake_backtracer_generate (GumBacktracer * backtracer,
    const GumCpuContext * cpu_context,
    GumReturnAddressArray * return_addresses, guint limit);

G_DEFINE_TYPE_EXTENDED (GumFakeBacktracer,
                        gum_fake_backtracer,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_BACKTRACER,
                                               gum_fake_backtracer_iface_init))

static void
gum_fake_backtracer_class_init (GumFakeBacktracerClass * klass)
{
}

static void
gum_fake_backtracer_iface_init (gpointer g_iface,
                                gpointer iface_data)
{
  GumBacktracerInterface * iface = g_iface;

  iface->generate = gum_fake_backtracer_generate;
}

static void
gum_fake_backtracer_init (GumFakeBacktracer * self)
{
}

GumBacktracer *
gum_fake_backtracer_new (const GumReturnAddress * ret_addrs,
                         guint num_ret_addrs)
{
  GumFakeBacktracer * backtracer;

  backtracer = g_object_new (GUM_TYPE_FAKE_BACKTRACER, NULL);
  backtracer->ret_addrs = ret_addrs;
  backtracer->num_ret_addrs = num_ret_addrs;

  return GUM_BACKTRACER (backtracer);
}

static void
gum_fake_backtracer_generate (GumBacktracer * backtracer,
                              const GumCpuContext * cpu_context,
                              GumReturnAddressArray * return_addresses,
                              guint limit)
{
  GumFakeBacktracer * self = GUM_FAKE_BACKTRACER (backtracer);
  guint depth = MIN (limit, self->num_ret_addrs);

  memcpy (return_addresses->items, self->ret_addrs,
      depth * sizeof (GumReturnAddress));
  return_addresses->len = depth;
}
