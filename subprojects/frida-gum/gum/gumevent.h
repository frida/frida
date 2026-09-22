/*
 * Copyright (C) 2009-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_EVENT_H__
#define __GUM_EVENT_H__

#include <gum/gumdefs.h>

G_BEGIN_DECLS

typedef guint GumEventType;

typedef union _GumEvent GumEvent;

typedef struct _GumAnyEvent     GumAnyEvent;
typedef struct _GumCallEvent    GumCallEvent;
typedef struct _GumRetEvent     GumRetEvent;
typedef struct _GumExecEvent    GumExecEvent;
typedef struct _GumBlockEvent   GumBlockEvent;
typedef struct _GumCompileEvent GumCompileEvent;

enum _GumEventType
{
  GUM_NOTHING     = 0,
  GUM_CALL        = 1 << 0,
  GUM_RET         = 1 << 1,
  GUM_EXEC        = 1 << 2,
  GUM_BLOCK       = 1 << 3,
  GUM_COMPILE     = 1 << 4,
};

struct _GumAnyEvent
{
  GumEventType type;
};

struct _GumCallEvent
{
  GumEventType type;

  gpointer location;
  gpointer target;
  gint depth;
};

struct _GumRetEvent
{
  GumEventType type;

  gpointer location;
  gpointer target;
  gint depth;
};

struct _GumExecEvent
{
  GumEventType type;

  gpointer location;
};

struct _GumBlockEvent
{
  GumEventType type;

  gpointer start;
  gpointer end;
};

struct _GumCompileEvent
{
  GumEventType type;

  gpointer start;
  gpointer end;
};

union _GumEvent
{
  GumEventType type;

  GumAnyEvent any;
  GumCallEvent call;
  GumRetEvent ret;
  GumExecEvent exec;
  GumBlockEvent block;
  GumCompileEvent compile;
};

G_END_DECLS

#endif
