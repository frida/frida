/*
 * Copyright (C) 2019 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "stalkerdummychannel.h"

#define SDC_LOCK() g_mutex_lock (&self->mutex)
#define SDC_UNLOCK() g_mutex_unlock (&self->mutex)

enum _StalkerDummyState
{
  SDC_CREATED = 1,
  SDC_GREETED,
  SDC_FOLLOWED,
  SDC_RAN,
  SDC_UNFOLLOWED,
  SDC_FLUSHED,
  SDC_FINISHED
};

static void sdc_wait_for_state (StalkerDummyChannel * self,
    StalkerDummyState target_state);
static void sdc_wait_for_state_unlocked (StalkerDummyChannel * self,
    StalkerDummyState target_state);
static void sdc_transition_to_state (StalkerDummyChannel * self,
    StalkerDummyState new_state);
static void sdc_transition_to_state_unlocked (StalkerDummyChannel * self,
    StalkerDummyState new_state);

void
sdc_init (StalkerDummyChannel * self)
{
  self->state = SDC_CREATED;
  g_mutex_init (&self->mutex);
  g_cond_init (&self->cond);
}

void
sdc_finalize (StalkerDummyChannel * self)
{
  g_mutex_clear (&self->mutex);
  g_cond_clear (&self->cond);
}

GumThreadId
sdc_await_thread_id (StalkerDummyChannel * self)
{
  GumThreadId thread_id;

  SDC_LOCK ();

  sdc_wait_for_state_unlocked (self, SDC_GREETED);
  thread_id = self->thread_id;

  SDC_UNLOCK ();

  return thread_id;
}

void
sdc_put_thread_id (StalkerDummyChannel * self,
                   GumThreadId thread_id)
{
  SDC_LOCK ();

  self->thread_id = thread_id;
  sdc_transition_to_state_unlocked (self, SDC_GREETED);

  SDC_UNLOCK ();
}

#define SDC_DEFINE_LOCKSTEP(name, state)                           \
    void                                                           \
    sdc_await_ ##name## _confirmation (StalkerDummyChannel * self) \
    {                                                              \
      sdc_wait_for_state (self, SDC_ ##state);                     \
    }                                                              \
                                                                   \
    void                                                           \
    sdc_put_ ##name## _confirmation (StalkerDummyChannel * self)   \
    {                                                              \
      sdc_transition_to_state (self, SDC_ ##state);                \
    }

SDC_DEFINE_LOCKSTEP (follow, FOLLOWED)
SDC_DEFINE_LOCKSTEP (run, RAN)
SDC_DEFINE_LOCKSTEP (unfollow, UNFOLLOWED)
SDC_DEFINE_LOCKSTEP (flush, FLUSHED)
SDC_DEFINE_LOCKSTEP (finish, FINISHED)

static void
sdc_wait_for_state (StalkerDummyChannel * self,
                    StalkerDummyState target_state)
{
  SDC_LOCK ();
  sdc_wait_for_state_unlocked (self, target_state);
  SDC_UNLOCK ();
}

static void
sdc_wait_for_state_unlocked (StalkerDummyChannel * self,
                             StalkerDummyState target_state)
{
  while (self->state != target_state)
    g_cond_wait (&self->cond, &self->mutex);
}

static void
sdc_transition_to_state (StalkerDummyChannel * self,
                         StalkerDummyState new_state)
{
  SDC_LOCK ();
  sdc_transition_to_state_unlocked (self, new_state);
  SDC_UNLOCK ();
}

static void
sdc_transition_to_state_unlocked (StalkerDummyChannel * self,
                                  StalkerDummyState new_state)
{
  self->state = new_state;
  g_cond_signal (&self->cond);
}
