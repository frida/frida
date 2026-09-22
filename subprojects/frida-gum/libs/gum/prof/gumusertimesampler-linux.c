/*
 * Copyright (C) 2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumusertimesampler.h"

#include <unistd.h>

enum
{
  PROP_0,
  PROP_THREAD_ID,
};

struct _GumUserTimeSampler
{
  GObject parent;

  GumThreadId thread_id;
  gchar * stat_path;
  long ticks_per_sec;
};

static void gum_user_time_sampler_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_user_time_sampler_constructed (GObject * object);
static void gum_user_time_sampler_finalize (GObject * object);
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

  object_class->constructed = gum_user_time_sampler_constructed;
  object_class->finalize = gum_user_time_sampler_finalize;
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
}

static void
gum_user_time_sampler_constructed (GObject * object)
{
  GumUserTimeSampler * self = GUM_USER_TIME_SAMPLER (object);

  self->stat_path = g_strdup_printf (
      "/proc/self/task/%" G_GSIZE_MODIFIER "u/stat",
      self->thread_id);
  self->ticks_per_sec = sysconf (_SC_CLK_TCK);
}

static void
gum_user_time_sampler_finalize (GObject * object)
{
  GumUserTimeSampler * self = GUM_USER_TIME_SAMPLER (object);

  g_free (self->stat_path);

  G_OBJECT_CLASS (gum_user_time_sampler_parent_class)->finalize (object);
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
  gchar * stat_contents = NULL;
  gchar ** fields = NULL;
  guint64 clock_ticks;

  self = GUM_USER_TIME_SAMPLER (sampler);

  if (!g_file_get_contents (self->stat_path, &stat_contents, NULL, NULL))
    goto beach;

  fields = g_strsplit (stat_contents, " ", -1);
  if (g_strv_length (fields) < 14)
    goto beach;

  clock_ticks = g_ascii_strtoull (fields[13], NULL, 10);

  utime = (clock_ticks * 1000000) / self->ticks_per_sec;

beach:
  g_strfreev (fields);
  g_free (stat_contents);

  return utime;
}
