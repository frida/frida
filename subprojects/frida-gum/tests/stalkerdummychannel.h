/*
 * Copyright (C) 2019 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __STALKER_DUMMY_CHANNEL_H__
#define __STALKER_DUMMY_CHANNEL_H__

#include <gum/gumstalker.h>

G_BEGIN_DECLS

typedef struct _StalkerDummyChannel StalkerDummyChannel;
typedef guint StalkerDummyState;

struct _StalkerDummyChannel
{
  volatile StalkerDummyState state;
  GumThreadId thread_id;
  GMutex mutex;
  GCond cond;
};

void sdc_init (StalkerDummyChannel * self);
void sdc_finalize (StalkerDummyChannel * self);

GumThreadId sdc_await_thread_id (StalkerDummyChannel * self);
void sdc_put_thread_id (StalkerDummyChannel * self, GumThreadId thread_id);

#define SDC_DECLARE_LOCKSTEP(name)                                       \
    void sdc_await_ ##name## _confirmation (StalkerDummyChannel * self); \
    void sdc_put_ ##name## _confirmation (StalkerDummyChannel * self);

SDC_DECLARE_LOCKSTEP (follow)
SDC_DECLARE_LOCKSTEP (run)
SDC_DECLARE_LOCKSTEP (unfollow)
SDC_DECLARE_LOCKSTEP (flush)
SDC_DECLARE_LOCKSTEP (finish)

G_END_DECLS

#endif
