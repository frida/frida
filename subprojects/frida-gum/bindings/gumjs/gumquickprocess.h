/*
 * Copyright (C) 2020-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2024 Håvard Sørbø <havard@hsorbo.no>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_PROCESS_H__
#define __GUM_QUICK_PROCESS_H__

#include "gumquickcore.h"
#include "gumquickmodule.h"
#include "gumquickthread.h"

G_BEGIN_DECLS

typedef struct _GumQuickProcess GumQuickProcess;
typedef struct _GumQuickExceptionHandler GumQuickExceptionHandler;

struct _GumQuickProcess
{
  GumQuickModule * module;
  GumQuickThread * thread;
  GumQuickCore * core;

  GHashTable * thread_observers;
  GHashTable * module_observers;

  JSClassID thread_observer_class;
  JSClassID module_observer_class;

  JSValue main_module_value;

  GumStalker * stalker;
  GSource * stalker_gc_timer;

  GumQuickExceptionHandler * exception_handler;
};

G_GNUC_INTERNAL void _gum_quick_process_init (GumQuickProcess * self,
    JSValue ns, GumQuickModule * module, GumQuickThread * thread,
    GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_process_flush (GumQuickProcess * self);
G_GNUC_INTERNAL void _gum_quick_process_dispose (GumQuickProcess * self);
G_GNUC_INTERNAL void _gum_quick_process_finalize (GumQuickProcess * self);

G_END_DECLS

#endif
