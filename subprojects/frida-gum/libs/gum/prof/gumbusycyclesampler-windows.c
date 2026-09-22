/*
 * Copyright (C) 2008-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2008 Christian Berentsen <jc.berentsen@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumbusycyclesampler.h"

#define _WIN32_LEAN_AND_MEAN
#undef WINVER
#undef _WIN32_WINNT
#define WINVER 0x0600
#define _WIN32_WINNT 0x0600
#include <windows.h>

typedef BOOL (WINAPI * QueryThreadCycleTimeFunc) (HANDLE ThreadHandle,
    PULONG64 CycleTime);

struct _GumBusyCycleSampler
{
  GObject parent;

  QueryThreadCycleTimeFunc query_thread_cycle_time;
};

static void gum_busy_cycle_sampler_iface_init (gpointer g_iface,
    gpointer iface_data);
static GumSample gum_busy_cycle_sampler_sample (GumSampler * sampler);

G_DEFINE_TYPE_EXTENDED (GumBusyCycleSampler,
                        gum_busy_cycle_sampler,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_SAMPLER,
                            gum_busy_cycle_sampler_iface_init))

static void
gum_busy_cycle_sampler_class_init (GumBusyCycleSamplerClass * klass)
{
}

static void
gum_busy_cycle_sampler_iface_init (gpointer g_iface,
                                   gpointer iface_data)
{
  GumSamplerInterface * iface = g_iface;

  iface->sample = gum_busy_cycle_sampler_sample;
}

static void
gum_busy_cycle_sampler_init (GumBusyCycleSampler * self)
{
  self->query_thread_cycle_time = (QueryThreadCycleTimeFunc) GetProcAddress (
      GetModuleHandleW (L"kernel32.dll"), "QueryThreadCycleTime");
}

GumSampler *
gum_busy_cycle_sampler_new (void)
{
  return g_object_new (GUM_TYPE_BUSY_CYCLE_SAMPLER, NULL);
}

gboolean
gum_busy_cycle_sampler_is_available (GumBusyCycleSampler * self)
{
  return (self->query_thread_cycle_time != NULL);
}

static GumSample
gum_busy_cycle_sampler_sample (GumSampler * sampler)
{
  GumBusyCycleSampler * self = (GumBusyCycleSampler *) sampler;
  GumSample result = 0;

  self->query_thread_cycle_time (GetCurrentThread (), &result);

  return result;
}
