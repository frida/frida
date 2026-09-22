/*
 * Copyright (C) 2010-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2024 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_SPINLOCK_H__
#define __GUM_SPINLOCK_H__

#include <gum/gumdefs.h>

#define GUM_SPINLOCK_INIT { NULL }

G_BEGIN_DECLS

typedef struct _GumSpinlock GumSpinlock;

struct _GumSpinlock
{
  gpointer data;
};

GUM_API void gum_spinlock_init (GumSpinlock * spinlock);

GUM_API void gum_spinlock_acquire (GumSpinlock * spinlock);
GUM_API gboolean gum_spinlock_try_acquire (GumSpinlock * spinlock);
GUM_API void gum_spinlock_release (GumSpinlock * spinlock);

G_END_DECLS

#endif
