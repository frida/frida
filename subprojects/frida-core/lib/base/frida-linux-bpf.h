#ifndef __FRIDA_LINUX_BPF_H__
#define __FRIDA_LINUX_BPF_H__

#include <glib.h>

G_BEGIN_DECLS

#define FRIDA_BPF_RINGBUF_HEADER_SIZE 8

typedef guint32 FridaBpfRingbufFlags;

enum _FridaBpfRingbufFlags
{
  FRIDA_BPF_RINGBUF_BUSY    = (1U << 31),
  FRIDA_BPF_RINGBUF_DISCARD = (1U << 30),
};

G_END_DECLS

#endif
