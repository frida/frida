#ifndef __GUM_SPINLOCK_H__
#define __GUM_SPINLOCK_H__

#include <glib.h>

#define GUM_SPINLOCK_INIT { NULL }

typedef struct _GumSpinlock GumSpinlock;

struct _GumSpinlock
{
  gpointer data;
};

void gum_spinlock_init (GumSpinlock * spinlock);

void gum_spinlock_acquire (GumSpinlock * spinlock);
void gum_spinlock_release (GumSpinlock * spinlock);

#endif
