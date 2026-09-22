#ifndef __GUM_STALKER_H__
#define __GUM_STALKER_H__

#include "gumdefs.h"
#if defined (HAVE_I386)
# include "arch-x86/gumx86writer.h"
#elif defined (HAVE_ARM)
# include "arch-arm/gumarmwriter.h"
# include "arch-arm/gumthumbwriter.h"
#elif defined (HAVE_ARM64)
# include "arch-arm64/gumarm64writer.h"
#elif defined (HAVE_MIPS)
# include "arch-mips/gummipswriter.h"
#endif

#include <capstone.h>

typedef struct _GumStalkerIterator GumStalkerIterator;
typedef struct _GumStalkerOutput GumStalkerOutput;
typedef union _GumStalkerWriter GumStalkerWriter;
typedef void (* GumStalkerTransformerCallback) (GumStalkerIterator * iterator,
    GumStalkerOutput * output, gpointer user_data);
typedef void (* GumStalkerCallout) (GumCpuContext * cpu_context,
    gpointer user_data);

typedef struct _GumCallSite GumCallSite;

typedef guint GumEventType;

typedef union _GumEvent GumEvent;

typedef struct _GumAnyEvent     GumAnyEvent;
typedef struct _GumCallEvent    GumCallEvent;
typedef struct _GumRetEvent     GumRetEvent;
typedef struct _GumExecEvent    GumExecEvent;
typedef struct _GumBlockEvent   GumBlockEvent;
typedef struct _GumCompileEvent GumCompileEvent;

union _GumStalkerWriter
{
#if defined (HAVE_I386)
  GumX86Writer * x86;
#elif defined (HAVE_ARM)
  GumArmWriter * arm;
  GumThumbWriter * thumb;
#elif defined (HAVE_ARM64)
  GumArm64Writer * arm64;
#elif defined (HAVE_MIPS)
  GumMipsWriter * mips;
#endif
};

struct _GumStalkerOutput
{
  GumStalkerWriter writer;
  GumInstructionEncoding encoding;
};

struct _GumCallSite
{
  gpointer block_address;
  gpointer stack_data;
  GumCpuContext * cpu_context;
};

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

gboolean gum_stalker_iterator_next (GumStalkerIterator * self,
    const cs_insn ** insn);
void gum_stalker_iterator_keep (GumStalkerIterator * self);
GumMemoryAccess gum_stalker_iterator_get_memory_access (
    GumStalkerIterator * self);
void gum_stalker_iterator_put_callout (GumStalkerIterator * self,
    GumStalkerCallout callout, gpointer data, GDestroyNotify data_destroy);
csh gum_stalker_iterator_get_capstone (GumStalkerIterator * self);

#endif
