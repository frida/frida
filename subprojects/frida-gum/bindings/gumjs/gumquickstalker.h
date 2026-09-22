/*
 * Copyright (C) 2020 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_STALKER_H__
#define __GUM_QUICK_STALKER_H__

#include "gumquickcodewriter.h"
#include "gumquickinstruction.h"

G_BEGIN_DECLS

typedef struct _GumQuickDefaultIterator GumQuickDefaultIterator;
typedef struct _GumQuickSpecialIterator GumQuickSpecialIterator;
typedef struct _GumQuickProbeArgs GumQuickProbeArgs;

struct _GumQuickStalker
{
  GumQuickCodeWriter * writer;
  GumQuickInstruction * instruction;
  GumQuickCore * core;

  GumStalker * stalker;
  guint queue_capacity;
  guint queue_drain_interval;

  GSource * flush_timer;

  JSClassID default_iterator_class;
  JSClassID special_iterator_class;
  JSClassID probe_args_class;

  GumQuickDefaultIterator * cached_default_iterator;
  gboolean cached_default_iterator_in_use;

  GumQuickSpecialIterator * cached_special_iterator;
  gboolean cached_special_iterator_in_use;

  GumQuickInstructionValue * cached_instruction;
  gboolean cached_instruction_in_use;

  GumQuickCpuContext * cached_cpu_context;
  gboolean cached_cpu_context_in_use;

  GumQuickProbeArgs * cached_probe_args;
  gboolean cached_probe_args_in_use;
};

G_GNUC_INTERNAL void _gum_quick_stalker_init (GumQuickStalker * self,
    JSValue ns, GumQuickCodeWriter * writer, GumQuickInstruction * instruction,
    GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_stalker_flush (GumQuickStalker * self);
G_GNUC_INTERNAL void _gum_quick_stalker_dispose (GumQuickStalker * self);
G_GNUC_INTERNAL void _gum_quick_stalker_finalize (GumQuickStalker * self);

G_GNUC_INTERNAL GumStalker * _gum_quick_stalker_get (GumQuickStalker * self);
G_GNUC_INTERNAL void _gum_quick_stalker_process_pending (GumQuickStalker * self,
    GumQuickScope * scope);

G_END_DECLS

#endif
