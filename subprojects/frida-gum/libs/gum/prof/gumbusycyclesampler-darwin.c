/*
 * Copyright (C) 2011-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumbusycyclesampler.h"

#include <mach/mach.h>

struct _GumBusyCycleSampler
{
  GObject parent;
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
}

GumSampler *
gum_busy_cycle_sampler_new (void)
{
  GumBusyCycleSampler * sampler;

  sampler = g_object_new (GUM_TYPE_BUSY_CYCLE_SAMPLER, NULL);

  return GUM_SAMPLER (sampler);
}

gboolean
gum_busy_cycle_sampler_is_available (GumBusyCycleSampler * self)
{
  return TRUE;
}

static GumSample
gum_busy_cycle_sampler_sample (GumSampler * sampler)
{
  mach_port_t port;
  thread_basic_info_data_t info;
  mach_msg_type_number_t info_count = THREAD_BASIC_INFO_COUNT;
  G_GNUC_UNUSED kern_return_t kr;

  port = mach_thread_self ();
  kr = thread_info (port, THREAD_BASIC_INFO,
      (thread_info_t) &info, &info_count);
  g_assert (kr == KERN_SUCCESS);
  mach_port_deallocate (mach_task_self (), port);

  /*
   * We could convert this to actual cycles, but doing so would be a waste
   * of time, because GumSample is an abstract unit anyway.
   */
  return ((GumSample) info.user_time.seconds * G_USEC_PER_SEC) +
      info.user_time.microseconds;
}
