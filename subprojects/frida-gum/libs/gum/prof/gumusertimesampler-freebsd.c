/*
 * Copyright (C) 2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumusertimesampler.h"

#include <sys/sysctl.h>
#include <sys/user.h>

enum
{
  PROP_0,
  PROP_THREAD_ID,
};

struct _GumUserTimeSampler
{
  GObject parent;

  guint process_id;
  GumThreadId thread_id;
};

static void gum_user_time_sampler_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_user_time_sampler_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gum_user_time_sampler_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static GumSample gum_user_time_sampler_sample (GumSampler * sampler);

G_DEFINE_TYPE_EXTENDED (GumUserTimeSampler,
                        gum_user_time_sampler,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_SAMPLER,
                        gum_user_time_sampler_iface_init))

static void
gum_user_time_sampler_class_init (GumUserTimeSamplerClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gum_user_time_sampler_get_property;
  object_class->set_property = gum_user_time_sampler_set_property;

  g_object_class_install_property (object_class, PROP_THREAD_ID,
      g_param_spec_uint64 ("thread-id", "ThreadID", "Thread ID to sample", 0,
      G_MAXUINT64, 0, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS));
}

static void
gum_user_time_sampler_iface_init (gpointer g_iface,
                                  gpointer iface_data)
{
  GumSamplerInterface * iface = g_iface;

  iface->sample = gum_user_time_sampler_sample;
}

static void
gum_user_time_sampler_init (GumUserTimeSampler * self)
{
  self->process_id = gum_process_get_id ();
}

static void
gum_user_time_sampler_get_property (GObject * object,
                                    guint property_id,
                                    GValue * value,
                                    GParamSpec * pspec)
{
  GumUserTimeSampler * self = GUM_USER_TIME_SAMPLER (object);

  switch (property_id)
  {
    case PROP_THREAD_ID:
      g_value_set_uint64 (value, self->thread_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
gum_user_time_sampler_set_property (GObject * object,
                                    guint property_id,
                                    const GValue * value,
                                    GParamSpec * pspec)
{
  GumUserTimeSampler * self = GUM_USER_TIME_SAMPLER (object);

  switch (property_id)
  {
    case PROP_THREAD_ID:
      self->thread_id = g_value_get_uint64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

GumSampler *
gum_user_time_sampler_new (void)
{
  return g_object_new (GUM_TYPE_USER_TIME_SAMPLER,
      "thread-id", (guint64) gum_process_get_current_thread_id (),
      NULL);
}

GumSampler *
gum_user_time_sampler_new_with_thread_id (GumThreadId thread_id)
{
  return g_object_new (GUM_TYPE_USER_TIME_SAMPLER,
      "thread-id", (guint64) thread_id,
      NULL);
}

gboolean
gum_user_time_sampler_is_available (GumUserTimeSampler * self)
{
  return TRUE;
}

static GumSample
gum_user_time_sampler_sample (GumSampler * sampler)
{
  GumUserTimeSampler * self;
  GumSample utime = 0;
  int mib[4];
  size_t size;
  guint n, i;
  struct kinfo_proc * threads = NULL;

  self = GUM_USER_TIME_SAMPLER (sampler);

  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PID | KERN_PROC_INC_THREAD;
  mib[3] = self->process_id;

  size = 0;
  if (sysctl (mib, G_N_ELEMENTS (mib), NULL, &size, NULL, 0) != 0)
    goto beach;

  while (TRUE)
  {
    size_t previous_size;
    gboolean still_too_small;

    threads = g_realloc (threads, size);

    previous_size = size;
    if (sysctl (mib, G_N_ELEMENTS (mib), threads, &size, NULL, 0) == 0)
      break;

    still_too_small = errno == ENOMEM && size == previous_size;
    if (!still_too_small)
      goto beach;

    size += size / 10;
  }

  n = size / sizeof (struct kinfo_proc);
  for (i = 0; i != n; i++)
  {
    struct kinfo_proc * p = &threads[i];

    if (p->ki_tid == self->thread_id)
    {
      utime = p->ki_runtime;
      break;
    }
  }

beach:
  g_free (threads);

  return utime;
}
