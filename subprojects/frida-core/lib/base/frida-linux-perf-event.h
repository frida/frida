#ifndef __FRIDA_LINUX_PERF_EVENT_H__
#define __FRIDA_LINUX_PERF_EVENT_H__

#include <glib.h>

G_BEGIN_DECLS

#define FRIDA_PERF_EVENT_COUNT_SW_CPU_CLOCK  0

typedef struct _FridaPerfEventAttr FridaPerfEventAttr;
typedef guint32 FridaPerfEventType;

struct _FridaPerfEventAttr
{
  FridaPerfEventType event_type;
  guint32 size;
  guint64 config;

  union
  {
    guint64 sample_period;
    guint64 sample_freq;
  };

  guint64 sample_type;
  guint64 read_format;

  guint64 flags;

  guint32 wakeup_events;
  guint32 bp_type;

  union
  {
    guint64 bp_addr;
    guint64 config1;
  };

  union
  {
    guint64 bp_len;
    guint64 config2;
  };
};

enum _FridaPerfEventType
{
  FRIDA_PERF_EVENT_TYPE_HARDWARE,
  FRIDA_PERF_EVENT_TYPE_SOFTWARE,
};

G_END_DECLS

#endif
